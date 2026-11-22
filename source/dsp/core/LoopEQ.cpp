#include "dsp/core/LoopEQ.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

void LoopEQ::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    lastLow = lastHigh = -1.0;
    reset();
}

void LoopEQ::reset() noexcept
{
    for (auto& f : hp)
        f.reset();
    for (auto& f : lp)
        f.reset();
}

void LoopEQ::setCutoffs (double lowCutHz, double highCutHz, bool snap) noexcept
{
    lowTargetLog = std::log2 (std::clamp (lowCutHz, 10.0, 2000.0));
    highTargetLog = std::log2 (std::clamp (highCutHz, 200.0, 20000.0));

    if (snap)
    {
        lowLog = lowTargetLog;
        highLog = highTargetLog;
    }
    else
    {
        // ~25 ms glide at a 32-sample control rate (independent of block size).
        constexpr double k = 0.08;
        lowLog += k * (lowTargetLog - lowLog);
        highLog += k * (highTargetLog - highLog);
    }

    updateCoefficients();
}

void LoopEQ::updateCoefficients() noexcept
{
    const auto lo = std::exp2 (lowLog);
    const auto hi = std::exp2 (highLog);
    if (std::abs (lo - lastLow) > 1.0e-6 * lo)
    {
        for (auto& f : hp)
            f.setup (lo, 0.7071068, sampleRate);
        lastLow = lo;
    }
    if (std::abs (hi - lastHigh) > 1.0e-6 * hi)
    {
        for (auto& f : lp)
            f.setup (hi, 0.7071068, sampleRate);
        lastHigh = hi;
        groupDelay = lowpassGroupDelay (hi, 0.7071068, sampleRate);
    }
}

void LoopEQ::process (double& l, double& r) noexcept
{
    l = lp[0].lowpass (hp[0].highpass (l));
    r = lp[1].lowpass (hp[1].highpass (r));
}

double LoopEQ::processMono (int channel, double x) noexcept
{
    const auto c = channel != 0 ? 1 : 0;
    return lp[c].lowpass (hp[c].highpass (x));
}

} // namespace oe::dsp
