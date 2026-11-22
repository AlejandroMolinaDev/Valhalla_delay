#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace oe::dsp
{

/** Circular delay line with fractional reads.

    - Power-of-two storage, so wrapping is a mask.
    - `push()` writes the newest sample; `read*(d)` returns the signal delayed by
      d samples (d = 0 is the sample just written).
    - Interpolators: linear, 4-point 3rd-order Hermite (used for modulated reads)
      and 4-point 3rd-order Lagrange (used by the clean digital modes). Both are
      exact for integer delays, so an unmodulated echo lands exactly on its time.
    - Storage is float; the read position is double (positions reach ~4e6). */
class FractionalDelayLine
{
public:
    /** Allocates (message thread / prepareToPlay only) at least `minSamples` of storage. */
    void allocate (std::size_t minSamples)
    {
        std::size_t size = 1;
        while (size < minSamples + 4)
            size <<= 1;

        if (size != buffer.size())
        {
            buffer.assign (size, 0.0f);
            mask = static_cast<std::uint32_t> (size - 1);
        }
        clear();
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    /** Clears up to `count` samples starting at `start` (progressive clearing
        spread over several audio blocks). Returns the next start position. */
    std::size_t clearChunk (std::size_t start, std::size_t count) noexcept
    {
        const auto end = std::min (buffer.size(), start + count);
        std::fill (buffer.begin() + static_cast<std::ptrdiff_t> (start),
                   buffer.begin() + static_cast<std::ptrdiff_t> (end), 0.0f);
        return end;
    }

    std::size_t size() const noexcept { return buffer.size(); }

    /** Largest delay (in samples) that can be read with the 4-point interpolators. */
    double maxDelay() const noexcept { return static_cast<double> (buffer.size()) - 4.0; }

    void push (float x) noexcept
    {
        writeIndex = (writeIndex + 1u) & mask;
        buffer[writeIndex] = x;
    }

    float at (std::uint32_t delaySamples) const noexcept { return buffer[(writeIndex - delaySamples) & mask]; }

    float readLinear (double d) const noexcept
    {
        d = clampDelay (d);
        const auto di = static_cast<std::uint32_t> (d);
        const auto f = static_cast<float> (d - static_cast<double> (di));
        const auto y1 = at (di);
        const auto y2 = at (di + 1u);
        return y1 + f * (y2 - y1);
    }

    float readHermite (double d) const noexcept
    {
        d = clampDelay (d);
        if (d < 1.0)
            return readLinear (d);

        const auto di = static_cast<std::uint32_t> (d);
        const auto f = static_cast<float> (d - static_cast<double> (di));
        const auto y0 = at (di - 1u); // newer neighbour
        const auto y1 = at (di);
        const auto y2 = at (di + 1u);
        const auto y3 = at (di + 2u); // older neighbour

        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    float readLagrange (double d) const noexcept
    {
        d = clampDelay (d);
        if (d < 1.0)
            return readLinear (d);

        const auto di = static_cast<std::uint32_t> (d);
        const auto f = static_cast<float> (d - static_cast<double> (di));
        const auto y0 = at (di - 1u);
        const auto y1 = at (di);
        const auto y2 = at (di + 1u);
        const auto y3 = at (di + 2u);

        // Lagrange basis on the nodes -1, 0, 1, 2 evaluated at f in [0, 1).
        const auto fm1 = f - 1.0f;
        const auto fm2 = f - 2.0f;
        const auto fp1 = f + 1.0f;
        return y0 * (-f * fm1 * fm2 * (1.0f / 6.0f))
             + y1 * (fp1 * fm1 * fm2 * 0.5f)
             + y2 * (-fp1 * f * fm2 * 0.5f)
             + y3 * (fp1 * f * fm1 * (1.0f / 6.0f));
    }

private:
    double clampDelay (double d) const noexcept
    {
        const auto hi = maxDelay();
        return d < 0.0 ? 0.0 : (d > hi ? hi : d);
    }

    std::vector<float> buffer = std::vector<float> (8, 0.0f);
    std::uint32_t mask = 7;
    std::uint32_t writeIndex = 0;
};

} // namespace oe::dsp
