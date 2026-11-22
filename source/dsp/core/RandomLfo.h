#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

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

    /** Hashed sequence values around one step, reused while the read point
        stays inside that step (it changes only a few times per second). */
    struct Cache
    {
        double offset = std::numeric_limits<double>::quiet_NaN();
        std::uint32_t n = 0xFFFFFFFFu;
        std::array<double, 4> y {};
    };

    /** A few caches keyed by the read offset (one per head / per offset in use). */
    template <std::size_t N>
    struct CacheSet
    {
        std::array<Cache, N> slots {};
        std::size_t next = 0;

        Cache& forOffset (double offset) noexcept
        {
            for (auto& c : slots)
                if (c.offset == offset)
                    return c;
            auto& c = slots[next];
            next = (next + 1) % N;
            c.offset = offset;
            c.n = 0xFFFFFFFFu;
            return c;
        }
    };

    /** Value in roughly [-1, 1] at the current phase + offset (offset in steps). */
    double value (double offsetSteps = 0.0) const noexcept
    {
        Cache scratch;
        return value (offsetSteps, scratch);
    }

    /** Same, using (and refreshing) a cache of the four sequence values. */
    double value (double offsetSteps, Cache& cache) const noexcept
    {
        auto p = phase + offsetSteps;
        p -= std::floor (p / kPeriod) * kPeriod;
        const auto n = static_cast<std::uint32_t> (p);
        const auto t = p - static_cast<double> (n);

        if (n != cache.n)
        {
            cache.n = n;
            cache.y = { static_cast<double> (sample (n - 1u)), static_cast<double> (sample (n)),
                        static_cast<double> (sample (n + 1u)), static_cast<double> (sample (n + 2u)) };
        }
        const auto y0 = cache.y[0], y1 = cache.y[1], y2 = cache.y[2], y3 = cache.y[3];

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
