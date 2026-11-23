#pragma once

#include "ui/TextSelector.h"

namespace oe::ui
{

/** "STYLE: Single" in the bottom bar (L/R topology). */
class StyleMenu final : public TextSelector
{
public:
    explicit StyleMenu (juce::AudioProcessorValueTreeState& state);
};

} // namespace oe::ui
