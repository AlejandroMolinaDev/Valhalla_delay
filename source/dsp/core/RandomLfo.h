#pragma once

#include <cmath>
#include <cstdint>

#include "dsp/core/Random.h"

namespace oe::dsp
{

/** Smooth random modulation source (wow, flutter, "random" mod).

    The value is a cubic (Catmull-Rom) interpolation of a hashed random
    sequence indexed by the phase, so it can be read at arbitrary phase
    offsets: 0.25 of a step for "quadrature", a large offset for fully
    decorrelated taps, a sign flip for "anti-phase". The sequence repeats every
    2^20 steps, which keeps the double phase exact forever. */
class RandomLfo
{
public:
    explicit RandomLfo (std::uint32_t seedValue = 1u) noexcept : seed (seedValue) {}

    void setSeed (std::uint32_t s) noexcept { seed = s; }
    void setRate (double hz, double sampleRate) noexcept { increment = sampleRate > 0.0 ? hz / sampleRate : 0.0; }
    void reset (double startPhase = 0.0) noexcept { phase = startPhase; }

    void advance() noexcept
    {
        phase += increment;
        if (phase >= kPeriod)
            phase -= kPeriod;
    }

    /** Value in roughly [-1, 1] at the current phase + offset (offset in steps). */
    double value (double offsetSteps = 0.0) const noexcept
    {
        auto p = phase + offsetSteps;
        p -= std::floor (p / kPeriod) * kPeriod;
        const auto n = static_cast<std::uint32_t> (p);
        const auto t = p - static_cast<double> (n);

        const auto y0 = static_cast<double> (sample (n - 1u));
        const auto y1 = static_cast<double> (sample (n));
        const auto y2 = static_cast<double> (sample (n + 1u));
        const auto y3 = static_cast<double> (sample (n + 2u));

        const auto a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
        const auto b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const auto c = -0.5 * y0 + 0.5 * y2;
        const auto v = ((a * t + b) * t + c) * t + y1;
        return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v);
    }

private:
    static constexpr double kPeriod = 1048576.0; // 2^20 steps
    static constexpr std::uint32_t kMask = (1u << 20) - 1u;

    float sample (std::uint32_t n) const noexcept { return hashBipolar (seed, n & kMask); }

    std::uint32_t seed;
    double phase = 0.0, increment = 0.0;
};

} // namespace oe::dsp
