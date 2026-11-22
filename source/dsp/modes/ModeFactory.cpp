#include "dsp/modes/ModeFactory.h"

#include "dsp/modes/AnalogPlusMode.h"
#include "dsp/modes/BBDBrightMode.h"
#include "dsp/modes/BBDMode.h"
#include "dsp/modes/CleanMode.h"
#include "dsp/modes/CrystalMode.h"
#include "dsp/modes/DigitalMode.h"
#include "dsp/modes/GhostShiftMode.h"
#include "dsp/modes/LoFiMode.h"
#include "dsp/modes/PhaserMode.h"
#include "dsp/modes/PitchDuckMode.h"
#include "dsp/modes/PitchMode.h"
#include "dsp/modes/PitchReverseMode.h"
#include "dsp/modes/PitchRichMode.h"
#include "dsp/modes/ReelHiFiMode.h"
#include "dsp/modes/TapeCleanMode.h"
#include "dsp/modes/TapeMode.h"
#include "dsp/modes/TapeNRMode.h"

namespace oe::dsp
{

std::unique_ptr<IModeProcessor> createMode (Mode mode)
{
    switch (mode)
    {
        case Mode::Tape:         return std::make_unique<TapeMode> (Mode::Tape);
        case Mode::TapeClean:    return std::make_unique<TapeCleanMode>();
        case Mode::ReelHiFi:     return std::make_unique<ReelHiFiMode>();
        case Mode::TapeNR:       return std::make_unique<TapeNRMode>();
        case Mode::BBD:          return std::make_unique<BBDMode> (Mode::BBD);
        case Mode::BBDBright:    return std::make_unique<BBDBrightMode>();
        case Mode::AnalogPlus:   return std::make_unique<AnalogPlusMode>();
        case Mode::Digital:      return std::make_unique<DigitalMode>();
        case Mode::Clean:        return std::make_unique<CleanMode>();
        case Mode::Crystal:      return std::make_unique<CrystalMode>();
        case Mode::LoFi:         return std::make_unique<LoFiMode>();
        case Mode::GhostShift:   return std::make_unique<GhostShiftMode>();
        case Mode::Pitch:        return std::make_unique<PitchMode>();
        case Mode::PitchRich:    return std::make_unique<PitchRichMode>();
        case Mode::PitchReverse: return std::make_unique<PitchReverseMode>();
        case Mode::PitchDuck:    return std::make_unique<PitchDuckMode>();
        case Mode::PhaserDDL:    return std::make_unique<PhaserMode>();
    }
    return std::make_unique<DigitalMode>();
}

} // namespace oe::dsp
