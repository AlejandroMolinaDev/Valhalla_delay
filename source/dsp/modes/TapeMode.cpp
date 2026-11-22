#include "dsp/modes/TapeMode.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

namespace
{
    constexpr double kReferenceDelaySeconds = 0.15; // speed factor 1 at 150 ms
    constexpr float kEmphasis = 1.0f;               // +6 dB high-frequency pre-emphasis
} // namespace

TapeMode::TapeMode (Mode v) noexcept : variant (v), transport (static_cast<std::uint32_t> (v) * 31u + 3u)
{
    rng[0].setSeed (0xA341316Cu + static_cast<std::uint32_t> (v));
    rng[1].setSeed (0xC8013EA4u + static_cast<std::uint32_t> (v));

    switch (variant)
    {
        case Mode::TapeClean: hfBase = 10000.0; noiseScale = 0.25f; bias = 0.05f; break;
        case Mode::ReelHiFi:  hfBase = 16000.0; noiseScale = 0.4f;  bias = 0.08f; tracksSpeed = false; break;
        case Mode::TapeNR:    hfBase = 11500.0; noiseScale = 0.12f; bias = 0.10f; noiseReduction = true; break;
        default:              hfBase = 7500.0;  noiseScale = 1.0f;  bias = 0.12f; splices = true; break;
    }
}

void TapeMode::prepare (double sampleRate)
{
    fs = sampleRate;
    transport.prepare (fs);
    for (auto& f : preEmph)
        f.setup (3000.0, fs);
    for (auto& f : deEmph)
        f.setup (3000.0, fs);
    for (auto& f : noiseHp)
        f.setup (1200.0, fs);
    for (auto& e : env)
        e.setup (fs, 1.0, 50.0);
    for (auto& c : nrEncode)
        c.setup (fs, 2.0, 40.0, 0.1f);
    for (auto& d : nrDecode)
        d.setup (fs, 2.5, 50.0, 0.1f);

    // Soft hysteresis: the magnetisation follows the drive faster when it
    // grows than when it relaxes (level-dependent HF compression).
    hystUp = static_cast<float> (1.0 - std::exp (-kTwoPi * 22000.0 / fs));
    hystDown = static_cast<float> (1.0 - std::exp (-kTwoPi * 14000.0 / fs));

    lastHf = lastQ = lastBump = -1.0;
    reset();
}

void TapeMode::reset() noexcept
{
    transport.reset();
    for (auto* arr : { &preEmph, &deEmph, &noiseHp })
        for (auto& f : *arr)
            f.reset();
    for (auto& f : gapLoss)
        f.reset();
    for (auto& f : headBump)
        f.reset();
    for (auto& e : env)
        e.reset();
    for (auto& c : nrEncode)
        c.reset();
    for (auto& d : nrDecode)
        d.reset();
    magnet = { 0.0f, 0.0f };
}

void TapeMode::update (const ModeContext& ctx) noexcept
{
    const auto delaySec = std::max (ctx.mainDelaySamples / fs, 0.001);
    const auto speed = kReferenceDelaySeconds / delaySec; // tape speed relative to the reference

    // --- Transport ----------------------------------------------------------
    if (variant == Mode::ReelHiFi)
        transport.setFree (static_cast<double> (ctx.modRateHz), ctx.modDepth);
    else
        transport.setDelayLinked (ctx.wow, ctx.flutter, ctx.mainDelaySamples);
    transport.setSplice (splices ? ctx.age : 0.0f, ctx.mainDelaySamples);

    // --- Era ------------------------------------------------------------------
    double eraHf = 1.0, eraQ = 0.707, bumpDb = 2.0;
    switch (ctx.era)
    {
        case Era::Past:
            eraHf = 0.8; eraQ = 1.1; bumpDb = 2.0;
            curve = variant == Mode::TapeClean ? SatCurve::Tape : SatCurve::TapeHot;
            inputScale = 1.15f;
            break;
        case Era::Present:
            eraHf = 1.0; eraQ = 0.85; bumpDb = 1.5;
            curve = SatCurve::Tape;
            inputScale = 1.0f;
            break;
        case Era::Future:
            eraHf = 1.3; eraQ = 0.707; bumpDb = 0.8;
            curve = variant == Mode::ReelHiFi ? SatCurve::Clean : SatCurve::Tape;
            inputScale = 0.8f;
            break;
    }

    // --- EQ: gap loss follows the tape speed, head bump moves with it --------
    double hf = hfBase * eraHf;
    double q = 0.707;
    double bumpHz = 50.0;
    if (tracksSpeed)
    {
        hf *= std::pow (speed, 0.6);
        // Past era: the gap-loss filter resonates more at short delays (high speed).
        q = 0.707 + (eraQ - 0.707) * std::clamp (speed / 2.0, 0.0, 1.0);
        bumpHz = std::clamp (70.0 * std::sqrt (speed), 35.0, 200.0);
    }
    else
    {
        bumpDb *= 0.6;
    }
    hf = std::clamp (hf, 2200.0, 0.45 * fs);

    if (std::abs (hf - lastHf) > 0.5 || std::abs (q - lastQ) > 1.0e-4)
    {
        for (auto& f : gapLoss)
            f.setup (hf, q, fs);
        lastHf = hf;
        lastQ = q;
        groupDelay = lowpassGroupDelay (hf, q, fs) + onePoleGroupDelay (18000.0, fs);
    }
    if (std::abs (bumpHz - lastBump) > 0.1)
    {
        for (auto& f : headBump)
            f.setup (bumpHz, 1.2, fs);
        lastBump = bumpHz;
    }
    bumpGain = static_cast<float> (std::pow (10.0, bumpDb / 20.0) - 1.0);

    // Normalise the chain so its most resonant frequency (head bump, gap-loss
    // peak) has unity gain: self-oscillation then starts at ~100 % Feedback in
    // every era, as on a calibrated machine; mid frequencies lose a little
    // level on each pass, which is part of the tape sound.
    const auto lpfPeak = q > 0.7072 ? q / std::sqrt (1.0 - 1.0 / (4.0 * q * q)) : 1.0;
    chainGain = static_cast<float> (1.0 / ((1.0 + static_cast<double> (bumpGain)) * lpfPeak));

    // --- Age: asperity noise ---------------------------------------------------
    noiseAmount = ctx.age * noiseScale * 0.02f;
}

float TapeMode::processChannel (int ch, float x) noexcept
{
    const auto c = static_cast<std::size_t> (ch);
    const auto e = env[c].process (x);

    if (noiseReduction)
    {
        // The record amplifier saturates before the NR encoder; otherwise the
        // 2:1 encoder would hide the Drive from the tape entirely.
        x = sat::tape (x, bias);
        x = nrEncode[c].process (x);
    }

    // Pre-emphasis -> saturation (+ soft hysteresis) -> de-emphasis.
    auto y = x + kEmphasis * static_cast<float> (preEmph[c].highpass (static_cast<double> (x)));
    const auto in = y * inputScale;
    const auto driven = curve == SatCurve::Tape ? sat::tape (in, bias) : saturate (curve, in);
    auto& m = magnet[c];
    m += (std::abs (driven) > std::abs (m) ? hystUp : hystDown) * (driven - m);
    y = m / inputScale;
    y -= (kEmphasis / (1.0f + kEmphasis)) * static_cast<float> (deEmph[c].highpass (static_cast<double> (y)));

    // Gap loss + head bump.
    auto d = gapLoss[c].lowpass (static_cast<double> (y));
    d += static_cast<double> (bumpGain) * headBump[c].bandpass (d);
    y = static_cast<float> (d) * chainGain;

    // Asperity (modulation) noise: proportional to the signal envelope.
    if (noiseAmount > 0.0f)
        y += noiseAmount * static_cast<float> (e) * static_cast<float> (noiseHp[c].highpass (static_cast<double> (rng[c].nextBipolar())));

    return transport.applySplice (y, e, ch);
}

void TapeMode::processRecord (float& l, float& r) noexcept
{
    l = processChannel (0, l);
    r = processChannel (1, r);
}

float TapeMode::postRead (int tap, float x) noexcept
{
    return noiseReduction ? nrDecode[static_cast<std::size_t> (tap)].process (x) : x;
}

} // namespace oe::dsp
