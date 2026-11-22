#include "dsp/core/ReversePitchShifter.h"

#include "dsp/core/Lfo.h"

namespace
{
    /** Grain window: smoothstep of a triangle. C1-smooth, and two grains half a
        period apart sum to exactly 1 (smoothstep(1 - x) = 1 - smoothstep(x)),
        like a Hann window but without a sin() per grain and per sample. */
    inline double grainWindow (double phase) noexcept
    {
        const auto tri = 1.0 - std::abs (2.0 * phase - 1.0);
        return tri * tri * (3.0 - 2.0 * tri);
    }
} // namespace

namespace oe::dsp
{

void ReversePitchShifter::advance() noexcept
{
    for (std::size_t t = 0; t < phases.size(); ++t)
    {
        const auto step = 1.0 / segment[t];
        for (auto& ph : phases[t])
        {
            ph += step;
            ph -= std::floor (ph);
        }
    }
}

float ReversePitchShifter::read (const FractionalDelayLine& line, double segmentLength, int tap) noexcept
{
    const auto t = static_cast<std::size_t> (tap) % phases.size();
    const auto r = ratio[t & 1u];
    constexpr double d0 = 2.0; // grains start just behind the write head
    const auto fit = (maxDelay - d0) / (1.0 + r);
    segment[t] = std::clamp (segmentLength, 0.01 * fs, fit);

    float out = 0.0f;
    for (const auto p : phases[t])
    {
        out += static_cast<float> (grainWindow (p)) * line.readHermite (d0 + (1.0 + r) * p * segment[t]);
    }
    return out;
}

} // namespace oe::dsp
