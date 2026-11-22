#include "dsp/core/ReversePitchShifter.h"

#include "dsp/core/Lfo.h"

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
        const auto s = std::sin (kPi * p); // Hann (sin^2) splice window
        out += static_cast<float> (s * s) * line.readHermite (d0 + (1.0 + r) * p * segment[t]);
    }
    return out;
}

} // namespace oe::dsp
