#pragma once

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

/** Global Duck: an RMS detector on the dry input (5 ms window / attack,
    ~250 ms release on the level itself, then a 15 ms gain smoother)
    continuously lowers the feedback gain and the wet level while the input
    is playing, and lets the echoes come back in the gaps.

    Gain law (dB): wet reduction = Duck x clamp(0.75 x (level_dBFS + 40), 0, 24);
    the feedback is reduced by a third of that (in dB). Material below
    -40 dBFS RMS is never ducked; a -14 dBFS RMS input at Duck 100 % lowers
    the wet by ~20 dB and the feedback by ~6.5 dB, so the repeats do not pile
    up while playing yet the loop keeps enough material to swell back in the
    gaps. Duck = 0 is an exact bypass (gains 1). */
class Ducker
{
public:
    void prepare (double sampleRate) noexcept
    {
        auto coeff = [sampleRate] (double ms) { return 1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate)); };
        window = coeff (5.0);
        attack = coeff (5.0);
        release = coeff (250.0);
        smooth = coeff (15.0);
        reset();
    }

    void reset() noexcept
    {
        meanSquare = 0.0;
        level = 0.0;
        gain = 1.0;
        fbGain = 1.0;
    }

    void setAmount (float amount01) noexcept { amount = static_cast<double> (std::clamp (amount01, 0.0f, 1.0f)); }

    /** Feeds one input frame, returns the duck gain (1 = no ducking). */
    float process (float inL, float inR) noexcept
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
            fbTarget = std::pow (10.0, -reductionDb / 60.0);
        }
        gain += smooth * (target - gain);
        fbGain += smooth * (fbTarget - fbGain);
        return static_cast<float> (gain);
    }

    /** Wet gain of the last processed frame. */
    float getGain() const noexcept { return static_cast<float> (gain); }

    /** Feedback gain of the last processed frame (a third of the wet reduction in dB). */
    float getFeedbackGain() const noexcept { return static_cast<float> (fbGain); }

private:
    double window = 0.0, attack = 0.0, release = 0.0, smooth = 0.0;
    double meanSquare = 0.0, level = 0.0, gain = 1.0, fbGain = 1.0, amount = 0.0;
};

} // namespace oe::dsp
