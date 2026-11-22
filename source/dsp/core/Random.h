#pragma once

#include <cstdint>

namespace oe::dsp
{

/** Small, fast xorshift32 generator. One instance per channel / per voice,
    each with its own seed, so noise is decorrelated and fully deterministic. */
class FastRandom
{
public:
    explicit FastRandom (std::uint32_t seed = 0x1234567u) noexcept { setSeed (seed); }

    void setSeed (std::uint32_t seed) noexcept { state = seed != 0 ? seed : 0x9E3779B9u; }

    std::uint32_t next() noexcept
    {
        auto x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    /** Uniform in [-1, 1). */
    float nextBipolar() noexcept
    {
        return static_cast<float> (static_cast<std::int32_t> (next())) * (1.0f / 2147483648.0f);
    }

    /** Uniform in [0, 1). */
    float nextUnipolar() noexcept
    {
        return static_cast<float> (next() >> 8) * (1.0f / 16777216.0f);
    }

private:
    std::uint32_t state = 0x1234567u;
};

/** Stateless integer hash (avalanching mix) used to build phase-indexable
    random sequences: hash (seed, n) always returns the same value for the same
    step, so a "random LFO" can be evaluated at any phase offset. */
inline std::uint32_t hash32 (std::uint32_t x) noexcept
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/** Hash mapped to a bipolar float in [-1, 1). */
inline float hashBipolar (std::uint32_t seed, std::uint32_t n) noexcept
{
    const auto h = hash32 (n * 0x9E3779B1u + hash32 (seed));
    return static_cast<float> (static_cast<std::int32_t> (h)) * (1.0f / 2147483648.0f);
}

} // namespace oe::dsp
