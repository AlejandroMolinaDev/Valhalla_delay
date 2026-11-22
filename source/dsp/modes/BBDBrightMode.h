#pragma once

#include "dsp/modes/BBDMode.h"

namespace oe::dsp
{

/** BBD Bright: bucket brigade whose bandwidth follows the clock, i.e. the delay
    time (short = bright, long = dark), with a two-stage asymmetric saturation;
    Duck in the main section. */
class BBDBrightMode final : public BBDMode
{
public:
    BBDBrightMode() noexcept : BBDMode (Mode::BBDBright) {}
};

} // namespace oe::dsp
