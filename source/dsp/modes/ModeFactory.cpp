#include "dsp/modes/ModeFactory.h"

#include "dsp/modes/BBDMode.h"
#include "dsp/modes/DigitalMode.h"
#include "dsp/modes/TapeMode.h"

namespace oe::dsp
{

std::unique_ptr<IModeProcessor> createMode (Mode mode)
{
    switch (mode)
    {
        case Mode::Digital:
            return std::make_unique<DigitalMode>();

        case Mode::Tape:
        case Mode::TapeClean:
        case Mode::ReelHiFi:
        case Mode::TapeNR:
            return std::make_unique<TapeMode> (mode);

        case Mode::BBD:
        case Mode::BBDBright:
        case Mode::AnalogPlus:
            return std::make_unique<BBDMode> (mode);

        // Modes implemented in later phases temporarily run the Digital engine.
        default:
            return std::make_unique<DigitalMode> (mode);
    }
}

} // namespace oe::dsp
