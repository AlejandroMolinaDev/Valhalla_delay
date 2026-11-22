#pragma once

#include <memory>

#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Creates the processor for a mode (message thread / construction only). */
std::unique_ptr<IModeProcessor> createMode (Mode mode);

} // namespace oe::dsp
