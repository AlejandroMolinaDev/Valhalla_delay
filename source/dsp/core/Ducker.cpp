#include "dsp/core/Ducker.h"

namespace oe::dsp
{

void Ducker::prepare (double sampleRate) noexcept
{
    auto coeff = [sampleRate] (double ms) { return 1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate)); };
    window = coeff (5.0);
    attack = coeff (5.0);
    release = coeff (250.0);
    smooth = coeff (15.0);
    reset();
}

float Ducker::process (float inL, float inR) noexcept
{
    const auto dl = static_cast<double> (inL);
    const auto dr = static_cast<double> (inR);
    meanSquare += window * (0.5 * (dl * dl + dr * dr) - meanSquare);
    const auto rms = std::sqrt (meanSquare);
    level += (rms > level ? attack : release) * (rms - level);

    double target = 1.0, fbTarget = 1.0;
    if (amount > 0.0)
    {
        const auto levelDb = 20.0 * std::log10 (level + 1.0e-9);
        const auto reductionDb = amount * std::clamp (0.75 * (levelDb + 40.0), 0.0, 24.0);
        target = std::pow (10.0, -reductionDb / 20.0);
        fbTarget = std::pow (10.0, -reductionDb / 60.0); // a third of the reduction, in dB
    }
    gain += smooth * (target - gain);
    fbGain += smooth * (fbTarget - fbGain);
    return static_cast<float> (gain);
}

} // namespace oe::dsp
