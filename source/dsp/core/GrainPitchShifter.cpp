#include "dsp/core/GrainPitchShifter.h"

#include <cmath>

#include "dsp/core/Lfo.h"

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
        const auto s = std::sin (kPi * p); // Hann (sin^2) grain window
        out += static_cast<float> (s * s) * line.readHermite (delay + p * window);
    }
    return out;
}

} // namespace oe::dsp
