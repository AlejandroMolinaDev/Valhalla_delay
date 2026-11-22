#pragma once

#include <algorithm>

#include "dsp/styles/IStyle.h"

namespace oe::dsp
{

/** Single: one master delay for both channels. Spread adds its time to L and
    subtracts it from R to widen the image. Same modulation on both sides. */
class SingleStyle final : public IStyle
{
public:
    Style id() const noexcept override { return Style::Single; }

    void plan (const StyleParams& p, TapPlan& out) noexcept override
    {
        out.numTaps = 2;
        out.delay[0] = std::max (0.0, p.delayL + p.spread);
        out.delay[1] = std::max (0.0, p.delayL - p.spread);
        out.line[0] = 0;
        out.line[1] = 1;
        out.layout[0] = { 0.0, 1.0f, 0 };
        out.layout[1] = { 0.0, 1.0f, 0 };
        out.mainDelay = p.delayL;
    }

    void record (float inL, float inR, const float* prev, float fb, float& recL, float& recR) const noexcept override
    {
        recL = inL + fb * prev[0];
        recR = inR + fb * prev[1];
    }

    void wet (const float* taps, float& wetL, float& wetR) const noexcept override
    {
        wetL = taps[0];
        wetR = taps[1];
    }
};

} // namespace oe::dsp
