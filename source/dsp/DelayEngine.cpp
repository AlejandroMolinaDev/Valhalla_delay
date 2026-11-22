#include "dsp/DelayEngine.h"

#include <algorithm>
#include <cmath>

#include "dsp/TempoSync.h"
#include "dsp/core/Saturation.h"
#include "dsp/modes/ModeFactory.h"

namespace oe::dsp
{

namespace
{
    constexpr float kLoopClamp = 2.5f;     // last-resort bound inside the loop
    constexpr float kOutputGuard = 1.0e4f; // anything above is treated as a blow-up
    constexpr std::size_t kClearChunk = 32768;

    inline bool isSane (float x) noexcept { return std::abs (x) < kOutputGuard; } // false for NaN / Inf
} // namespace

DelayEngine::DelayEngine()
{
    for (int i = 0; i < kNumModes; ++i)
    {
        bankA[static_cast<std::size_t> (i)] = createMode (static_cast<Mode> (i));
        bankB[static_cast<std::size_t> (i)] = createMode (static_cast<Mode> (i));
    }
    current = bankA[0].get();
}

DelayEngine::~DelayEngine() = default;

void DelayEngine::prepare (double sampleRate)
{
    const auto rateChanged = std::abs (sampleRate - fs) > 1.0e-6;
    fs = sampleRate;

    const auto needed = static_cast<std::size_t> (std::ceil ((kMaxDelaySeconds + kMaxModulationSeconds) * fs)) + 16;
    for (auto& line : lines)
        if (rateChanged || line.size() < needed + 4)
            line.allocate (needed);

    maxReadDelay = std::min (lines[0].maxDelay(), kMaxDelaySeconds * fs + kMaxModulationSeconds * fs);

    for (auto& m : bankA)
        m->prepare (fs);
    for (auto& m : bankB)
        m->prepare (fs);

    loopEq.prepare (fs);
    wetEq.prepare (fs);
    diffusion.prepare (fs, kMaxDelaySeconds + 0.2);
    for (auto& d : dc)
        d.setup (fs, 5.0);
    limiter.prepare (fs);
    ducker.prepare (fs);

    for (auto* s : { &feedbackS, &dryS, &wetS, &widthS, &driveS, &driveCompS })
        s->setTime (fs, 0.02);
    bypassStep = static_cast<float> (1.0 / (0.03 * fs));

    glideSeconds = -1.0;
    reset();
}

void DelayEngine::reset() noexcept
{
    for (auto& line : lines)
        line.clear();

    for (auto& m : bankA)
        m->reset();
    for (auto& m : bankB)
        m->reset();

    loopEq.reset();
    wetEq.reset();
    diffusion.reset();
    for (auto& d : dc)
        d.reset();
    limiter.reset();
    ducker.reset();

    heads.fill (0.0f);
    prevHeads.fill (0.0f);
    for (auto& x : timeXfade)
        x.ramp.finish();

    previous = nullptr;
    modeRamp.finish();
    modeMix = 1.0f;
    styleFade = StyleFade::None;
    styleGain = 1.0f;

    controlCountdown = 0;
    needsSnap = true;
    clearPending = false;
    clearPosition = 0;
    nanDetected = false;
    ++resetCount;
}

IStyle& DelayEngine::styleFor (Style s) noexcept
{
    switch (s)
    {
        case Style::Single:   return singleStyle;
        case Style::Dual:     return dualStyle;
        case Style::Ratio:    return ratioStyle;
        case Style::PingPong: return pingPongStyle;
        case Style::Quad:     return quadStyle;
    }
    return singleStyle;
}

void DelayEngine::switchMode (Mode mode, Era era) noexcept
{
    // A switch while a crossfade is still running completes the old one first.
    previous = needsSnap ? nullptr : current;
    usingBankA = ! usingBankA;
    auto& bank = usingBankA ? bankA : bankB;
    current = bank[static_cast<std::size_t> (mode)].get();
    current->reset();
    currentMode = mode;
    currentEra = era;

    if (previous != nullptr)
        modeRamp.start (static_cast<int> (kModeCrossfadeSeconds * fs));
    else
        modeRamp.finish();
}

void DelayEngine::applyStylePlan (bool snap) noexcept
{
    const auto bpm = sanitiseBpm (params.bpm);
    const auto toSamples = [this] (double ms) { return std::max (0.0, ms) * 0.001 * fs; };
    const auto maxMs = kMaxDelaySeconds * 1000.0;

    StyleParams sp;
    sp.delayL = toSamples (std::min (maxMs, effectiveDelayMs (params.delayLMs, params.syncL, bpm)));
    sp.delayR = toSamples (std::min (maxMs, effectiveDelayMs (params.delayRMs, params.syncR, bpm)));
    sp.spread = toSamples (static_cast<double> (std::clamp (params.spreadMs, 0.0f, 100.0f)));
    sp.ratio = static_cast<double> (params.ratio);
    sp.spacing = static_cast<double> (params.spacing);
    sp.taps = params.taps;
    sp.quadFeedback = params.quadFeedback;

    style->plan (sp, plan);

    const auto useCrossfade = params.digitalTimeCrossfade && current->isDigital();
    for (int t = 0; t < plan.numTaps; ++t)
    {
        const auto ti = static_cast<std::size_t> (t);
        const auto target = std::clamp (plan.delay[ti], 0.0, maxReadDelay);
        plan.delay[ti] = target;

        if (snap)
        {
            tapTime[ti].reset (target);
            timeXfade[ti].from = timeXfade[ti].to = target;
            timeXfade[ti].ramp.finish();
            continue;
        }

        tapTime[ti].setTarget (target);

        if (useCrossfade)
        {
            auto& x = timeXfade[ti];
            if (! x.ramp.isActive() && std::abs (target - x.to) > 0.5)
            {
                x.from = x.to;
                x.to = target;
                x.ramp.start (static_cast<int> (kTimeCrossfadeSeconds * fs));
            }
        }
        else
        {
            timeXfade[ti].from = timeXfade[ti].to = tapTime[ti].getCurrent();
            timeXfade[ti].ramp.finish();
        }
    }

    if (snap)
        mainDelay.reset (plan.mainDelay);
    else
        mainDelay.setTarget (plan.mainDelay);
}

void DelayEngine::updateModeContext() noexcept
{
    auto& c = modeContext;
    c.sampleRate = fs;
    c.era = params.era;
    c.age = std::clamp (params.age, 0.0f, 1.0f);
    c.pitchMix = std::clamp (params.pitchMix, 0.0f, 1.0f);
    c.res = std::clamp (params.res, 0.0f, 1.0f);
    c.wow = std::clamp (params.wow, 0.0f, 1.0f);
    c.flutter = std::clamp (params.flutter, 0.0f, 1.0f);
    c.modRateHz = std::clamp (params.modRateHz, 0.01f, 10.0f);
    c.modDepth = std::clamp (params.modDepth, 0.0f, 1.0f);
    c.freqShiftHz = std::clamp (params.freqShiftHz, -1000.0f, 1000.0f);
    c.freqDetuneHz = std::clamp (params.freqDetuneHz, 0.0f, 50.0f);
    c.pitchShiftSt = std::clamp (params.pitchShiftSt, -24.0f, 24.0f);
    c.pitchDetuneCents = std::clamp (params.pitchDetuneCents, 0.0f, 100.0f);
    c.driveDb = std::clamp (params.driveDb, 0.0f, 24.0f);
    c.diffAmount = std::clamp (params.diffAmount, 0.0f, 1.0f);
    c.mainDelaySamples = mainDelay.getCurrent();

    current->update (c);
    if (previous != nullptr)
        previous->update (c);
}

void DelayEngine::controlUpdate() noexcept
{
    const auto snap = needsSnap;

    // Mode / era change -> crossfade between banks.
    if (snap || params.mode != currentMode || params.era != currentEra)
        switchMode (params.mode, params.era);

    // Style change -> short dip (15 ms out, switch, 15 ms in), buffers kept.
    if (snap)
    {
        currentStyle = pendingStyle = params.style;
        style = &styleFor (currentStyle);
        styleFade = StyleFade::None;
        styleGain = 1.0f;
    }
    else if (params.style != currentStyle && styleFade == StyleFade::None)
    {
        pendingStyle = params.style;
        styleFade = StyleFade::Out;
        styleRamp.start (static_cast<int> (0.015 * fs));
    }

    const auto glide = current->glideSeconds();
    if (glide != glideSeconds)
    {
        glideSeconds = glide;
        for (auto& t : tapTime)
            t.setTime (fs, glide);
        mainDelay.setTime (fs, glide);
    }

    applyStylePlan (snap);
    updateModeContext();

    // Diffusion size is relative to the shortest active head delay.
    double reference = 0.0;
    for (int t = 0; t < plan.numTaps; ++t)
    {
        const auto d = plan.delay[static_cast<std::size_t> (t)];
        if (d > 0.0 && (reference <= 0.0 || d < reference))
            reference = d;
    }
    diffusion.setParameters (params.diffAmount, params.diffSize, reference, snap);

    loopEq.setCutoffs (static_cast<double> (params.lowCutHz), static_cast<double> (params.highCutHz), snap);
    wetEq.setCutoffs (static_cast<double> (params.lowCutHz), static_cast<double> (params.highCutHz), snap);
    ducker.setAmount (params.duck);

    // Gains (equal-power mix: dry = cos θ, wet = sin θ).
    const auto mixGains = equalPowerGains (params.mix);
    const auto driveDb = std::clamp (params.driveDb, 0.0f, 24.0f);
    const auto targets = std::array<float, 6> {
        std::clamp (params.feedback, 0.0f, 2.0f),
        mixGains.dry,
        mixGains.wet,
        std::clamp (params.width, -1.0f, 1.0f),
        dbToGain (driveDb),
        dbToGain (-0.5f * driveDb) // automatic output compensation: -Drive/2 dB
    };
    const std::array<OnePoleSmoother<float>*, 6> smoothers { &feedbackS, &dryS, &wetS, &widthS, &driveS, &driveCompS };
    for (std::size_t i = 0; i < targets.size(); ++i)
    {
        if (snap)
            smoothers[i]->reset (targets[i]);
        else
            smoothers[i]->setTarget (targets[i]);
    }

    if (snap)
        bypassAmount = bypassTarget ? 1.0f : 0.0f;

    needsSnap = false;
}

float DelayEngine::readHead (IModeProcessor& m, int tap, double delay) noexcept
{
    const auto ti = static_cast<std::size_t> (tap);
    const auto& line = lines[static_cast<std::size_t> (plan.line[ti])];
    const auto offset = m.readOffsetSamples (delay) + m.recordGroupDelaySamples()
                      + (m.loopEqInLoop() ? loopEq.groupDelaySamples() : 0.0)
                      + diffusion.compensationSamples();
    const auto mod = m.modulationSamples (plan.layout[ti], delay);
    const auto d = std::clamp (delay + mod - offset, 0.0, maxReadDelay);
    return m.postRead (tap, m.readTap (line, d, tap));
}

void DelayEngine::processRecordChain (float& l, float& r) noexcept
{
    if (previous != nullptr)
    {
        auto pl = l, pr = r;
        previous->processRecord (pl, pr);
        current->processRecord (l, r);
        l = pl + modeMix * (l - pl);
        r = pr + modeMix * (r - pr);
    }
    else
    {
        current->processRecord (l, r);
    }

    // Diffusion (skipped entirely at 0 %).
    if (diffusion.isActive())
        diffusion.process (l, r);

    // Loop EQ (in the loop for every mode except Crystal).
    const auto curIn = current->loopEqInLoop();
    const auto prevIn = previous != nullptr ? previous->loopEqInLoop() : curIn;
    if (curIn || prevIn)
    {
        auto el = static_cast<double> (l), er = static_cast<double> (r);
        loopEq.process (el, er);
        const auto wCur = curIn ? 1.0f : 0.0f;
        const auto wPrev = prevIn ? 1.0f : 0.0f;
        const auto w = previous != nullptr ? wPrev + modeMix * (wCur - wPrev) : wCur;
        l += w * (static_cast<float> (el) - l);
        r += w * (static_cast<float> (er) - r);
    }

    l = static_cast<float> (dc[0].process (static_cast<double> (l)));
    r = static_cast<float> (dc[1].process (static_cast<double> (r)));

    if (! isSane (l) || ! isSane (r))
    {
        nanDetected = true;
        l = r = 0.0f;
    }
    l = std::clamp (l, -kLoopClamp, kLoopClamp);
    r = std::clamp (r, -kLoopClamp, kLoopClamp);
}

void DelayEngine::progressiveClear() noexcept
{
    if (! clearPending)
        return;
    std::size_t next = clearPosition;
    for (auto& line : lines)
        next = line.clearChunk (clearPosition, kClearChunk);
    clearPosition = next;
    if (diffusionClearPending)
    {
        if (diffusion.clearStep())
            diffusionClearPending = false;
        return;
    }
    if (clearPosition >= lines[0].size())
    {
        clearPending = false;
        for (auto& m : bankA)
            m->reset();
        for (auto& m : bankB)
            m->reset();
        loopEq.reset();
        wetEq.reset();
        for (auto& d : dc)
            d.reset();
        limiter.reset();
        heads.fill (0.0f);
        prevHeads.fill (0.0f);
    }
}

void DelayEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0 || left == nullptr || right == nullptr || ! isPrepared())
        return;

    // Fully bypassed: the dry signal passes untouched, buffers are cleared in chunks.
    if (fullyBypassed)
    {
        if (bypassTarget)
        {
            progressiveClear();
            return;
        }

        // Leaving bypass: finish clearing so no stale echo comes back, restart clean.
        while (clearPending)
            progressiveClear();
        fullyBypassed = false;
        needsSnap = true;
        controlCountdown = 0;
    }

    double wetEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        if (controlCountdown <= 0)
        {
            controlUpdate();
            controlCountdown = kControlInterval;
        }
        --controlCountdown;

        const auto inL = left[i];
        const auto inR = right[i];

        // --- Crossfade / fade state ------------------------------------
        if (previous != nullptr)
        {
            modeMix = modeRamp.next();
            if (! modeRamp.isActive())
            {
                previous = nullptr;
                modeMix = 1.0f;
            }
        }

        if (styleFade != StyleFade::None)
        {
            const auto t = styleRamp.next();
            if (styleFade == StyleFade::Out)
            {
                styleGain = 1.0f - t;
                if (! styleRamp.isActive())
                {
                    currentStyle = pendingStyle;
                    style = &styleFor (currentStyle);
                    applyStylePlan (true);
                    prevHeads.fill (0.0f);
                    styleFade = StyleFade::In;
                    styleRamp.start (static_cast<int> (0.015 * fs));
                }
            }
            else
            {
                styleGain = t;
                if (! styleRamp.isActive())
                {
                    styleFade = StyleFade::None;
                    styleGain = 1.0f;
                }
            }
        }

        const auto duckGain = ducker.process (inL, inR);
        const auto fb = feedbackS.next() * ducker.getFeedbackGain();
        const auto drive = driveS.next();

        // --- Record path (write first: a 0 ms delay reads this very sample) --
        float recL = 0.0f, recR = 0.0f;
        style->record (drive * inL, drive * inR, prevHeads.data(), fb, recL, recR);
        processRecordChain (recL, recR);
        lines[0].push (recL);
        lines[1].push (recR);

        // --- Read heads --------------------------------------------------
        current->advanceModulation();
        if (previous != nullptr)
            previous->advanceModulation();
        mainDelay.next();

        for (int t = 0; t < plan.numTaps; ++t)
        {
            const auto ti = static_cast<std::size_t> (t);
            const auto glided = tapTime[ti].next();
            auto& xf = timeXfade[ti];

            auto readBoth = [&] (double d)
            {
                auto v = readHead (*current, t, d);
                if (previous != nullptr)
                {
                    const auto pv = readHead (*previous, t, d);
                    v = pv + modeMix * (v - pv);
                }
                return v;
            };

            float v;
            if (xf.ramp.isActive())
            {
                const auto a = xf.ramp.next();
                const auto va = readBoth (xf.from);
                const auto vb = readBoth (xf.to);
                v = va + a * (vb - va);
                if (! xf.ramp.isActive())
                    xf.from = xf.to;
            }
            else
            {
                v = readBoth (params.digitalTimeCrossfade && current->isDigital() ? xf.to : glided);
            }
            heads[ti] = v * styleGain;
        }

        // --- Wet ---------------------------------------------------------
        float wetL = 0.0f, wetR = 0.0f;
        style->wet (heads.data(), wetL, wetR);

        if (previous != nullptr)
        {
            auto pl = wetL, pr = wetR;
            previous->processWet (pl, pr);
            current->processWet (wetL, wetR);
            wetL = pl + modeMix * (wetL - pl);
            wetR = pr + modeMix * (wetR - pr);
        }
        else
        {
            current->processWet (wetL, wetR);
        }

        // Crystal: Low/High Cut on the wet output instead of inside the loop.
        const auto curOut = ! current->loopEqInLoop();
        const auto prevOut = previous != nullptr ? ! previous->loopEqInLoop() : curOut;
        if (curOut || prevOut)
        {
            auto el = static_cast<double> (wetL), er = static_cast<double> (wetR);
            wetEq.process (el, er);
            const auto wCur = curOut ? 1.0f : 0.0f;
            const auto wPrev = prevOut ? 1.0f : 0.0f;
            const auto w = previous != nullptr ? wPrev + modeMix * (wCur - wPrev) : wCur;
            wetL += w * (static_cast<float> (el) - wetL);
            wetR += w * (static_cast<float> (er) - wetR);
        }

        const auto wetGain = driveCompS.next() * duckGain;
        wetL *= wetGain;
        wetR *= wetGain;
        limiter.process (wetL, wetR);

        const auto w = widthS.next();
        const auto a = 0.5f * (1.0f + w);
        const auto b = 0.5f * (1.0f - w);
        const auto outWetL = a * wetL + b * wetR;
        const auto outWetR = b * wetL + a * wetR;
        const auto el2 = static_cast<double> (outWetL);
        const auto er2 = static_cast<double> (outWetR);
        wetEnergy += 0.5 * (el2 * el2 + er2 * er2);

        bypassAmount = bypassTarget ? std::min (1.0f, bypassAmount + bypassStep)
                                    : std::max (0.0f, bypassAmount - bypassStep);
        const auto byp = bypassAmount;
        const auto dryG = dryS.next();
        const auto wetG = wetS.next() * (1.0f - byp);
        const auto dryMix = dryG + byp * (1.0f - dryG);

        auto outL = dryMix * inL + wetG * outWetL;
        auto outR = dryMix * inR + wetG * outWetR;
        if (! isSane (outL) || ! isSane (outR))
        {
            nanDetected = true;
            outL = inL;
            outR = inR;
        }
        left[i] = outL;
        right[i] = outR;

        prevHeads = heads;
    }

    lastWetRms = static_cast<float> (std::sqrt (wetEnergy / static_cast<double> (numSamples)));

    if (nanDetected)
        reset(); // NaN / Inf anywhere: restart the engine from silence

    if (bypassTarget && bypassAmount >= 1.0f)
    {
        fullyBypassed = true;
        clearPending = true;
        diffusionClearPending = true;
        clearPosition = 0;
    }
}

double DelayEngine::getTapDelaySamples (int tap) const noexcept
{
    if (tap < 0 || tap >= kMaxTaps)
        return 0.0;
    return tapTime[static_cast<std::size_t> (tap)].getCurrent();
}

int DelayEngine::getQuantizerBits() const noexcept
{
    return current != nullptr ? current->quantizerBits() : -1;
}

Mode DelayEngine::getActiveMode() const noexcept
{
    return currentMode;
}

double DelayEngine::estimateTailSeconds (const EngineParams& p) noexcept
{
    constexpr double kMaxTail = 60.0;
    const auto bpm = sanitiseBpm (p.bpm);
    const auto dL = effectiveDelayMs (p.delayLMs, p.syncL, bpm);
    const auto dR = effectiveDelayMs (p.delayRMs, p.syncR, bpm);
    auto longest = std::max (dL, (p.style == Style::Dual || p.style == Style::PingPong) ? dR : 0.0);
    if (p.style == Style::Single)
        longest += static_cast<double> (p.spreadMs);
    if (p.style == Style::PingPong)
        longest = dL + dR;
    longest = std::min (longest, kMaxDelaySeconds * 1000.0) * 0.001;

    const auto fb = static_cast<double> (std::clamp (p.feedback, 0.0f, 2.0f));
    if (fb >= 0.995)
        return kMaxTail;

    double tail = longest;
    if (fb > 1.0e-3)
        tail += longest * std::log (1.0e-5) / std::log (fb); // repeats until -100 dB

    // Diffusion turns the echoes into a reverb-like decay of its own.
    tail += static_cast<double> (p.diffAmount) * (1.0 + 8.0 * static_cast<double> (p.diffSize));
    if (p.mode == Mode::PitchReverse)
        tail += 2.0 * longest;

    return std::clamp (tail + 0.1, 0.1, kMaxTail);
}

} // namespace oe::dsp
