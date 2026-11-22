#pragma once

#include <array>

#include "dsp/Types.h"
#include "dsp/core/FractionalDelayLine.h"

namespace oe::dsp
{

/** Granular (delay-line) pitch shifter that reads straight from the echo's
    delay line, so it adds no latency of its own.

    Two grains per read head, half a window apart. Each grain's extra delay
    o = phase x W ramps at (1 - ratio) samples per sample, so the read point
    moves at `ratio` x real time (the pitch shift), and wraps every window;
    a Hann (sin²) window per grain makes the two gains sum to exactly 1, so
    the splices are click-free. The average extra delay is W / 2; the mode
    reports it so the engine reads that much earlier and the echo stays on
    time. Ratios are set per channel (L / R detune); taps with an even index
    read the left line, odd ones the right line (all Styles). */
class GrainPitchShifter
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }

    void reset() noexcept
    {
        for (auto& p : phases)
            p = { 0.0, 0.5 };
    }

    void setRatios (double left, double right) noexcept { ratio = { left, right }; }

    /** Grain window length in samples (>= 64). */
    void setWindow (double samples) noexcept { window = samples < 64.0 ? 64.0 : samples; }
    double getWindow() const noexcept { return window; }

    /** Advances every head's grains by one sample. */
    void advance() noexcept;

    /** Pitch-shifted read around `delay` (already reduced by window / 2 by the engine). */
    float read (const FractionalDelayLine& line, double delay, int tap) const noexcept;

private:
    double fs = 48000.0;
    double window = 2880.0;
    std::array<double, 2> ratio { 1.0, 1.0 };
    std::array<std::array<double, 2>, kMaxTaps> phases {};
};

} // namespace oe::dsp
