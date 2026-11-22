#include "dsp/core/BBDModel.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

namespace
{
    constexpr float kCompanderRef = 0.1f;   // -20 dBFS: unity point of the compander
    constexpr float kChipHeadroom = 3.0f;   // chip clips at ~0.33 in the compressed domain
} // namespace

BBDModel::BBDModel (std::uint32_t seed) noexcept
{
    rng[0].setSeed (seed * 2654435761u + 1u);
    rng[1].setSeed (seed * 2246822519u + 7u);
}

void BBDModel::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    for (auto& c : compressor)
        c.setup (fs, 4.0, 60.0, kCompanderRef);
    // The decoder's detector is a little slower: the pair never tracks perfectly.
    for (auto& e : expander)
        e.setup (fs, 6.0, 80.0, kCompanderRef);
    for (auto& g : gate)
        g.setup (fs, 1.0, 60.0);
    for (auto& t : noiseTilt)
        t.setup (900.0, fs);
    lastCutoff = -1.0;
    reset();
}

void BBDModel::reset() noexcept
{
    for (auto& f : antiAlias)
        f.reset();
    for (auto& f : reconstruction)
        f.reset();
    for (auto& c : compressor)
        c.reset();
    for (auto& e : expander)
        e.reset();
    for (auto& g : gate)
        g.reset();
    for (auto& t : noiseTilt)
        t.reset();
    clock.reset();
}

void BBDModel::setBandwidth (double cutoffHz, int order) noexcept
{
    if (std::abs (cutoffHz - lastCutoff) < 0.5 && order == lastOrder)
        return;
    lastCutoff = cutoffHz;
    lastOrder = order;
    for (auto& f : antiAlias)
        f.setup (cutoffHz, order, fs);
    for (auto& f : reconstruction)
        f.setup (cutoffHz, order, fs);
    filterGroupDelay = antiAlias[0].groupDelaySamples();
}

void BBDModel::setNoise (float amount, double clockHz) noexcept
{
    noiseAmount = std::max (0.0f, amount);
    clock.setRate (std::clamp (clockHz, 2000.0, 0.45 * fs), fs);
}

void BBDModel::setCurves (SatCurve input, SatCurve chip) noexcept
{
    inCurve = input;
    chipCurve = chip;
}

float BBDModel::record (int channel, float x) noexcept
{
    const auto c = static_cast<std::size_t> (channel);
    const auto env = gate[c].process (x);

    auto y = saturate (inCurve, x);                                   // input buffer
    y = static_cast<float> (antiAlias[c].process (static_cast<double> (y)));
    y = compressor[c].process (y);                                    // 2:1
    y = saturate (chipCurve, y * kChipHeadroom) / kChipHeadroom;      // chip clipping

    if (noiseAmount > 0.0f)
    {
        // Hiss + clock leakage, gated by the signal (silent in silence).
        const auto g = static_cast<float> (std::min (1.0, env * 30.0));
        const auto hiss = static_cast<float> (noiseTilt[c].highpass (static_cast<double> (rng[c].nextBipolar())));
        if (channel == 0)
            clock.advance();
        const auto tone = static_cast<float> (clock.value (LfoShape::Sine, channel == 0 ? 0.0 : 0.25));
        y += g * noiseAmount * (0.006f * hiss + 0.0015f * tone);
    }
    return y;
}

float BBDModel::playback (int head, float x) noexcept
{
    const auto h = static_cast<std::size_t> (head);
    const auto y = static_cast<float> (reconstruction[h].process (static_cast<double> (x)));
    return expander[h].process (y);                                   // 1:2
}

} // namespace oe::dsp
