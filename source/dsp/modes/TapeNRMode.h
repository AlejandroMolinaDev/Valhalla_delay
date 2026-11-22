#pragma once

#include "dsp/modes/TapeMode.h"

namespace oe::dsp
{

/** Tape NR: the Tape algorithm wrapped in a 2:1 / 1:2 noise-reduction compander
    (record amplifier saturation -> encoder -> tape -> per-head decoder): brighter
    (11.5 kHz) and almost noiseless; Duck in the main section. */
class TapeNRMode final : public TapeMode
{
public:
    TapeNRMode() noexcept : TapeMode (Mode::TapeNR) {}
};

} // namespace oe::dsp
