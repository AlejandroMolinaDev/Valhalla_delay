#pragma once

#include "dsp/modes/TapeMode.h"

namespace oe::dsp
{

/** Reel HiFi (studio open-reel machine): full bandwidth (16 kHz, fixed: the speed
    does not change with the delay time), random wow/flutter independent of the
    delay (Mod Rate / Mod Depth), asperity noise without splices. */
class ReelHiFiMode final : public TapeMode
{
public:
    ReelHiFiMode() noexcept : TapeMode (Mode::ReelHiFi) {}
};

} // namespace oe::dsp
