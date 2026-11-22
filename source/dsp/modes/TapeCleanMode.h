#pragma once

#include "dsp/modes/TapeMode.h"

namespace oe::dsp
{

/** Tape Clean (chrome tape): the Tape algorithm with a brighter gap loss (10 kHz at the
    reference speed), a finer saturation (bias 0.05), a quarter of the asperity
    noise and no splices. Wow / Flutter follow the delay time as in Tape. */
class TapeCleanMode final : public TapeMode
{
public:
    TapeCleanMode() noexcept : TapeMode (Mode::TapeClean) {}
};

} // namespace oe::dsp
