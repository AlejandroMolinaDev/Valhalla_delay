#pragma once

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

/** Global Duck: an RMS follower on the dry input (attack ~5 ms, release
    ~250 ms, then smoothed) continuously lowers the feedback gain and the wet
    level while the input is playing, and lets the echoes swell back up in the
    gaps. Duck = 0 is an exact bypass (gain 1). */
class Ducker
{
public:
    void prepare (double sampleRate) noexcept
    {
        auto coeff = [sampleRate] (double ms) { return 1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate)); };
        attack = coeff (5.0);
        release = coeff (250.0);
        smooth = coeff (15.0);
        reset();
    }

    void reset() noexcept
    {
        meanSquare = 0.0;
        gain = 1.0;
    }

    void setAmount (float amount01) noexcept { amount = static_cast<double> (std::clamp (amount01, 0.0f, 1.0f)); }

    /** Feeds one input frame, returns the duck gain (1 = no ducking). */
    float process (float inL, float inR) noexcept
    {
        const auto dl = static_cast<double> (inL);
        const auto dr = static_cast<double> (inR);
        const auto ms = 0.5 * (dl * dl + dr * dr);
        meanSquare += (ms > meanSquare ? attack : release) * (ms - meanSquare);

        double target = 1.0;
        if (amount > 0.0)
        {
            const auto env = std::sqrt (meanSquare);
            // Duck 100 %: about -14 dB for a -20 dBFS RMS input, -22 dB for -10 dBFS.
            target = 1.0 / (1.0 + 40.0 * amount * env);
        }
        gain += smooth * (target - gain);
        return static_cast<float> (gain);
    }

    float getGain() const noexcept { return static_cast<float> (gain); }

private:
    double attack = 0.0, release = 0.0, smooth = 0.0;
    double meanSquare = 0.0, gain = 1.0, amount = 0.0;
};

} // namespace oe::dsp
