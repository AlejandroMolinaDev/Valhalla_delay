#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "dsp/Types.h"
#include "dsp/core/FractionalDelayLine.h"

namespace oe::dsp
{

/** Reverse + pitch reader for the Pitch Reverse mode.

    Plays back segments of the delay line backwards; the splice (segment)
    length is the head's delay time. A grain starting at delay d0 moves its
    read delay as d(t) = d0 + (1 + r)·t, i.e. the read point travels backwards
    through the recorded audio at r x real time: reversal and pitch shift (r =
    pitch ratio) in one move. Two grains half a segment apart with Hann (sin²)
    windows (gains sum to 1) crossfade every splice without clicks. The
    segment is shortened when (1 + r)·T would not fit in the 20 s buffer. */
class ReversePitchShifter
{
public:
    void prepare (double sampleRate, double maxDelaySamples) noexcept
    {
        fs = sampleRate;
        maxDelay = maxDelaySamples;
        reset();
    }

    void reset() noexcept
    {
        for (auto& p : phases)
            p = { 0.0, 0.5 };
        segment.fill (0.25 * fs);
    }

    void setRatios (double left, double right) noexcept { ratio = { left, right }; }

    /** Advances every head's grains by one sample. */
    void advance() noexcept;

    /** `segmentLength` = the head's delay (samples): sets the splice size. */
    float read (const FractionalDelayLine& line, double segmentLength, int tap) noexcept;

private:
    double fs = 48000.0;
    double maxDelay = 48000.0;
    std::array<double, 2> ratio { 1.0, 1.0 };
    std::array<std::array<double, 2>, kMaxTaps> phases {};
    std::array<double, kMaxTaps> segment {};
};

} // namespace oe::dsp
