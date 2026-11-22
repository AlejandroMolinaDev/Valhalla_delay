#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace oe::dsp
{

/** Floating-point converter emulation (80s gain-ranging DACs/ADCs).

    The value keeps its exponent but its mantissa is rounded to `bits`
    significant bits (implicit leading one included), i.e. the quantisation
    step scales with the signal level — the "grainy" sound of early digital
    delays. Below the converter's lowest range the step stops shrinking
    (linear segment), like a real gain-ranging converter at its last range. */
class BitCrusher
{
public:
    /** Age -> effective bits for the floating-point converter.
        0 % = 24 bits (transparent), 50 % = 12 bits, 100 % = 3 bits (minimum). */
    static int bitsForAge (float age) noexcept
    {
        const auto bits = static_cast<int> (std::lround (24.0 * (1.0 - static_cast<double> (std::clamp (age, 0.0f, 1.0f)))));
        return std::clamp (bits, 3, 24);
    }

    void setBits (int newBits) noexcept
    {
        bits = std::clamp (newBits, 1, 24);
        // Lowest gain range: 2^-15 (~-90 dBFS); below it the step is constant.
        floorLevel = 1.0f / 32768.0f;
        floorStep = std::ldexp (floorLevel, -(bits - 1));
    }

    int getBits() const noexcept { return bits; }

    float process (float x) const noexcept
    {
        if (bits >= 24)
            return x;

        if (std::abs (x) < floorLevel)
            return std::round (x / floorStep) * floorStep;

        // Round the IEEE-754 mantissa to (bits - 1) explicit bits.
        std::uint32_t u;
        std::memcpy (&u, &x, sizeof (u));
        const auto drop = static_cast<std::uint32_t> (23 - (bits - 1));
        const auto half = 1u << (drop - 1u);
        u = (u + half) & ~((1u << drop) - 1u);
        float y;
        std::memcpy (&y, &u, sizeof (y));
        return y;
    }

private:
    int bits = 24;
    float floorLevel = 1.0f / 32768.0f;
    float floorStep = 0.0f;
};

/** Plain linear (fixed-point) quantiser, used by the LoFi mode. */
inline float quantizeLinear (float x, float bits) noexcept
{
    const auto levels = std::exp2 (std::max (1.0f, bits) - 1.0f);
    return std::round (x * levels) / levels;
}

} // namespace oe::dsp
