#include "dsp/modes/ModeFactory.h"

#include "dsp/modes/DigitalMode.h"

namespace oe::dsp
{

std::unique_ptr<IModeProcessor> createMode (Mode mode)
{
    switch (mode)
    {
        case Mode::Digital:
            return std::make_unique<DigitalMode>();

        // Modes implemented in later phases temporarily run the Digital engine.
        default:
            return std::make_unique<DigitalMode> (mode);
    }
}

} // namespace oe::dsp
