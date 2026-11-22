#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "dsp/core/Lfo.h"
#include "dsp/core/RandomLfo.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Largest Mod Depth excursion (100 %), in milliseconds. The engine further
    limits the excursion to 45 % of the tap's own delay so short delays turn
    into chorus / flanger instead of reading "the future". */
constexpr double kMaxModDepthMs = 10.0;

inline double limitModDepth (double depthSamples, double tapDelay) noexcept
{
    return std::min (depthSamples, 0.45 * tapDelay + 1.0);
}

/** Periodic LFO modulation (Mod Rate / Mod Depth). Quad taps with a
    decorrelation index get an extra phase offset and a slightly different
    rate so the heads drift independently. */
class PeriodicModulator
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; }
    void reset() noexcept { lfo.reset(); }

    void update (double rateHz, double depth01, LfoShape newShape) noexcept
    {
        lfo.setRate (rateHz, fs);
        depthSamples = depth01 * kMaxModDepthMs * 0.001 * fs;
        shape = newShape;
    }

    void advance() noexcept { lfo.advance(); }

    double at (const TapLayout& t, double tapDelay) const noexcept
    {
        if (depthSamples <= 0.0)
            return 0.0;
        const auto offset = t.phaseOffset + 0.1618 * static_cast<double> (t.decorrelation);
        return static_cast<double> (t.polarity) * limitModDepth (depthSamples, tapDelay) * lfo.value (shape, offset);
    }

    double getDepthSamples() const noexcept { return depthSamples; }

private:
    double fs = 48000.0;
    Lfo lfo;
    LfoShape shape = LfoShape::SmoothTriangle;
    double depthSamples = 0.0;
};

/** Smooth random modulation (wow, flutter, random mod). */
class RandomModulator
{
public:
    explicit RandomModulator (std::uint32_t seed = 7u) noexcept : lfo (seed) {}

    void prepare (double sampleRate) noexcept { fs = sampleRate; }
    void reset() noexcept
    {
        lfo.reset();
        cache = {};
    }

    void update (double rateHz, double depthSamplesValue) noexcept
    {
        lfo.setRate (rateHz, fs);
        depthSamples = depthSamplesValue;
    }

    void advance() noexcept { lfo.advance(); }

    double at (const TapLayout& t, double tapDelay) const noexcept
    {
        if (depthSamples <= 0.0)
            return 0.0;
        // Each decorrelation index jumps far ahead in the random sequence.
        const auto offset = t.phaseOffset + 4099.37 * static_cast<double> (t.decorrelation);
        return static_cast<double> (t.polarity) * limitModDepth (depthSamples, tapDelay) * lfo.value (offset, cache.forOffset (offset));
    }

private:
    double fs = 48000.0;
    RandomLfo lfo;
    mutable RandomLfo::CacheSet<kMaxTaps> cache;
    double depthSamples = 0.0;
};

} // namespace oe::dsp
