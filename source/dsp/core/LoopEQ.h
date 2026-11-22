#pragma once

#include "dsp/core/Filters.h"

namespace oe::dsp
{

/** Low Cut (high-pass, 10 Hz - 2 kHz) + High Cut (low-pass, 200 Hz - 20 kHz),
    both 12 dB/oct Butterworth. Placed inside the feedback loop (except in the
    Crystal mode, where the engine applies it to the wet output instead), so
    every repeat is filtered once more and the echoes darken / thin out
    progressively. Cutoffs are smoothed in the log domain at control rate. */
class LoopEQ
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Control-rate update (every N samples). */
    void setCutoffs (double lowCutHz, double highCutHz, bool snap) noexcept;

    void process (double& l, double& r) noexcept;

    /** DC group delay of the High Cut section (samples). */
    double groupDelaySamples() const noexcept { return groupDelay; }
    double processMono (int channel, double x) noexcept;

private:
    void updateCoefficients() noexcept;

    double sampleRate = 48000.0;
    double lowLog = 0.0, highLog = 0.0;          // smoothed log2 cutoffs
    double lowTargetLog = 0.0, highTargetLog = 0.0;
    double lastLow = -1.0, lastHigh = -1.0;
    double groupDelay = 0.0;
    Svf hp[2], lp[2];
};

} // namespace oe::dsp
