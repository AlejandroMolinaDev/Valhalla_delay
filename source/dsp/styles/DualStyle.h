#pragma once

#include "dsp/styles/IStyle.h"

namespace oe::dsp
{

/** Dual: two independent mono delays in parallel (Delay L, Delay R). Each
    channel feeds back into itself only — no cross feedback. Quadrature
    (0° / 90°) modulation. */
class DualStyle final : public IStyle
{
public:
    Style id() const noexcept override { return Style::Dual; }

    void plan (const StyleParams& p, TapPlan& out) noexcept override
    {
        out.numTaps = 2;
        out.delay[0] = p.delayL;
        out.delay[1] = p.delayR;
        out.line[0] = 0;
        out.line[1] = 1;
        out.layout[0] = { 0.0, 1.0f, 0 };
        out.layout[1] = { 0.25, 1.0f, 0 };
        out.mainDelay = 0.5 * (p.delayL + p.delayR);
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
