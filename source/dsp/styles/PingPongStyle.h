#pragma once

#include "dsp/styles/IStyle.h"

namespace oe::dsp
{

/** PingPong: L+R is summed into the left delay; the left output feeds the
    right delay and the right output feeds back into the left one. Independent
    L and R times. Anti-phase (0° / 180°) modulation. */
class PingPongStyle final : public IStyle
{
public:
    Style id() const noexcept override { return Style::PingPong; }

    void plan (const StyleParams& p, TapPlan& out) noexcept override
    {
        out.numTaps = 2;
        out.delay[0] = p.delayL;
        out.delay[1] = p.delayR;
        out.line[0] = 0;
        out.line[1] = 1;
        out.layout[0] = { 0.0, 1.0f, 0 };
        out.layout[1] = { 0.0, -1.0f, 0 };
        out.mainDelay = 0.5 * (p.delayL + p.delayR);
    }

    void record (float inL, float inR, const float* prev, float fb, float& recL, float& recR) const noexcept override
    {
        recL = 0.5f * (inL + inR) + fb * prev[1];
        recR = fb * prev[0];
    }

    void wet (const float* taps, float& wetL, float& wetR) const noexcept override
    {
        wetL = taps[0];
        wetR = taps[1];
    }
};

} // namespace oe::dsp
