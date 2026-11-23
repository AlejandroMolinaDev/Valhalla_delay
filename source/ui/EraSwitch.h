#pragma once

#include "ui/TextSelector.h"

namespace oe::ui
{

/** "ERA: Present" in the bottom bar (Past / Present / Future). */
class EraSwitch final : public TextSelector
{
public:
    explicit EraSwitch (juce::AudioProcessorValueTreeState& state);
};

} // namespace oe::ui
