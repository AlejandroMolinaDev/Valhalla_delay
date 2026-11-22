#pragma once

#include "dsp/modes/PitchMode.h"

namespace oe::dsp
{

/** Pitch Duck: the Pitch algorithm with the (global) Duck control in the main
    section, so the shifted spirals stay out of the way while playing. */
class PitchDuckMode final : public PitchMode
{
public:
    PitchDuckMode() noexcept : PitchMode (Mode::PitchDuck) {}
};

} // namespace oe::dsp
