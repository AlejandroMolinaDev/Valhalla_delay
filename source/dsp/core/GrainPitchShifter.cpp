#include "dsp/core/GrainPitchShifter.h"

#include <cmath>

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

void GrainPitchShifter::advance() noexcept
{
    for (std::size_t t = 0; t < phases.size(); ++t)
    {
        const auto step = (1.0 - ratio[t & 1u]) / window;
        for (auto& ph : phases[t])
        {
            ph += step;
            ph -= std::floor (ph);
        }
    }
}

float GrainPitchShifter::read (const FractionalDelayLine& line, double delay, int tap) const noexcept
{
    const auto& ph = phases[static_cast<std::size_t> (tap) % phases.size()];
    float out = 0.0f;
    for (const auto p : ph)
    {
        out += static_cast<float> (grainWindow (p)) * line.readHermite (delay + p * window);
    }
    return out;
}

} // namespace oe::dsp
