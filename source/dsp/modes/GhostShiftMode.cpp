#include "dsp/modes/GhostShiftMode.h"

#include <algorithm>
#include <cmath>

#include "dsp/core/Saturation.h"

namespace oe::dsp
{

void GhostShiftMode::prepare (double sampleRate)
{
    fs = sampleRate;
    shifter.prepare (fs);
    wow.prepare (fs);
    wow.update (0.35, 0.00015 * fs); // subtle fixed hi-fi transport drift
    for (auto& f : noiseHp)
        f.setup (1200.0, fs);
    for (auto& e : env)
        e.setup (fs, 1.0, 50.0);
    lastHf = -1.0;
    reset();
}

void GhostShiftMode::reset() noexcept
{
    shifter.reset();
    wow.reset();
    for (auto& f : gapLoss)
        f.reset();
    for (auto& f : noiseHp)
        f.reset();
    for (auto& e : env)
        e.reset();
    for (auto& ch : apState)
        ch.fill (0.0);
}

double GhostShiftMode::dispersionGroupDelay (double w) const noexcept
{
    // First-order all-pass (a + z^-1) / (1 + a z^-1): tau = (1 - a^2) / (1 + 2a cos w + a^2).
    const auto a = dispersionCoeff;
    return static_cast<double> (dispersionStages) * (1.0 - a * a) / (1.0 + 2.0 * a * std::cos (w) + a * a);
}

void GhostShiftMode::update (const ModeContext& ctx) noexcept
{
    shifter.setShift (static_cast<double> (ctx.freqShiftHz), static_cast<double> (ctx.freqDetuneHz));

    const auto hf = std::min (ctx.era == Era::Past ? 12000.0 : (ctx.era == Era::Present ? 16000.0 : 19000.0), 0.45 * fs);
    if (std::abs (hf - lastHf) > 0.5)
    {
        for (auto& f : gapLoss)
            f.setup (hf, 0.7071, fs);
        lastHf = hf;
    }

    // Era -> dispersion; strength follows the Diffusion amount.
    const auto strength = 0.3 + 0.7 * static_cast<double> (ctx.diffAmount);
    switch (ctx.era)
    {
        case Era::Past:    dispersionStages = 0;  dispersionCoeff = 0.0; break;
        case Era::Present: dispersionStages = 12; dispersionCoeff = -(0.55 + 0.30 * strength); break;
        case Era::Future:  dispersionStages = 32; dispersionCoeff = -(0.70 + 0.22 * strength); break;
    }

    noiseAmount = ctx.age * 0.4f * 0.02f;

    // Keep the 1 kHz region on the delay grid (lows arrive later, highs earlier).
    const auto w1k = kTwoPi * 1000.0 / fs;
    groupDelay = lowpassGroupDelay (hf, 0.7071, fs) + shifter.groupDelaySamples()
               + (dispersionStages > 0 ? dispersionGroupDelay (w1k) : 0.0);
}

float GhostShiftMode::processTape (int ch, float x) noexcept
{
    const auto c = static_cast<std::size_t> (ch);
    const auto e = env[c].process (x);
    auto y = sat::tape (x, 0.08f);
    y = static_cast<float> (gapLoss[c].lowpass (static_cast<double> (y)));
    if (noiseAmount > 0.0f)
        y += noiseAmount * static_cast<float> (e) * static_cast<float> (noiseHp[c].highpass (static_cast<double> (rng[c].nextBipolar())));
    return y;
}

void GhostShiftMode::processRecord (float& l, float& r) noexcept
{
    l = processTape (0, l);
    r = processTape (1, r);
    shifter.process (l, r);

    if (dispersionStages > 0)
    {
        float* io[2] { &l, &r };
        const auto a = dispersionCoeff;
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            auto x = static_cast<double> (*io[ch]);
            auto& s = apState[ch];
            for (int i = 0; i < dispersionStages; ++i)
            {
                // Transposed direct form II all-pass: y = a x + s; s = x - a y.
                auto& z = s[static_cast<std::size_t> (i)];
                const auto y = a * x + z;
                z = x - a * y;
                x = y;
            }
            *io[ch] = static_cast<float> (x);
        }
    }
}

} // namespace oe::dsp
