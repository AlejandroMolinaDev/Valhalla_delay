#pragma once

#include <array>

#include "dsp/Types.h"

namespace oe::ui
{

/** Snapshot the audio thread publishes (lock-free FIFO, ~60 per second) for
    the EchoVisualizer. Plain data, trivially copyable. */
struct VisualizerFrame
{
    int numTaps = 2;
    std::array<float, dsp::kMaxTaps> tapMs {};
    std::array<bool, 4> headsOn { true, true, true, true };
    int style = 0;
    int quadSwell = 0;
    float feedback = 0.35f;   // 0..2
    float diffusion = 0.0f;   // 0..1
    float wetRms = 0.0f;
    float width = 1.0f;
};

} // namespace oe::ui
