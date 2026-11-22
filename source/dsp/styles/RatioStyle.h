#pragma once

#include <algorithm>

#include "dsp/styles/IStyle.h"

namespace oe::dsp
{

/** Ratio: base delay on L, R = Ratio x L. The feedback is mixed between the
    two lines through an energy-preserving rotation (35°), so echoes of length
    L and R combine on every pass (m·L + n·R): with incommensurate ratios such
    as 61.8 % the echo density keeps growing — a pseudo-reverb. Anti-phase
    (0° / 180°) modulation. */
class RatioStyle final : public IStyle
{
public:
    static constexpr float kCos = 0.81915204f; // cos 35°
    static constexpr float kSin = 0.57357644f; // sin 35°

    Style id() const noexcept override { return Style::Ratio; }

    void plan (const StyleParams& p, TapPlan& out) noexcept override
    {
        out.numTaps = 2;
        out.delay[0] = p.delayL;
        out.delay[1] = p.delayL * std::clamp (p.ratio, 0.01, 1.0);
        out.line[0] = 0;
        out.line[1] = 1;
        out.layout[0] = { 0.0, 1.0f, 0 };
        out.layout[1] = { 0.0, -1.0f, 0 };
        out.mainDelay = p.delayL;
    }

    void record (float inL, float inR, const float* prev, float fb, float& recL, float& recR) const noexcept override
    {
        recL = inL + fb * (kCos * prev[0] + kSin * prev[1]);
        recR = inR + fb * (-kSin * prev[0] + kCos * prev[1]);
    }

    void wet (const float* taps, float& wetL, float& wetR) const noexcept override
    {
        wetL = taps[0];
        wetR = taps[1];
    }
};

} // namespace oe::dsp
