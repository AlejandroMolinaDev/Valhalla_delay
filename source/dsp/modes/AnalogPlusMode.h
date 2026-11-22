#pragma once

#include "dsp/modes/BBDMode.h"

namespace oe::dsp
{

/** Analog+: modern BBD with about twice the bandwidth and half the noise of the
    BBD mode and a thicker saturation. */
class AnalogPlusMode final : public BBDMode
{
public:
    AnalogPlusMode() noexcept : BBDMode (Mode::AnalogPlus) {}
};

} // namespace oe::dsp
