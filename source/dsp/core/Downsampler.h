#pragma once

#include <cmath>

namespace oe::dsp
{

/** Sample-rate reduction by fractional sample-and-hold (LoFi).

    No interpolation and no reconstruction: the held steps alias freely, which
    is the point. The hold rate is continuous (`ratio` = new rate / host
    rate), so sweeping it produces the classic stepping / whistling. On
    average the hold delays the signal by half a hold period, which the mode
    reports for echo-time compensation. */
class Downsampler
{
public:
    void reset() noexcept
    {
        phase = 1.0;
        held = 0.0f;
    }

    void setRatio (double newRatio) noexcept { ratio = newRatio <= 0.0 ? 1.0 : (newRatio > 1.0 ? 1.0 : newRatio); }
    double getRatio() const noexcept { return ratio; }

    float process (float x) noexcept
    {
        phase += ratio;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = x;
        }
        return held;
    }

    /** Average delay of the hold (samples). */
    double averageDelay() const noexcept { return 0.5 * (1.0 / ratio - 1.0); }

private:
    double ratio = 1.0, phase = 1.0;
    float held = 0.0f;
};

} // namespace oe::dsp
