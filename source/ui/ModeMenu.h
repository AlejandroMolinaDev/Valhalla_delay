#pragma once

#include "ui/TextSelector.h"

namespace oe::ui
{

/** "MODE: Tape" in the bottom bar; clicking opens the 17 algorithms grouped
    by family. Attached to the "mode" parameter. */
class ModeMenu final : public TextSelector
{
public:
    explicit ModeMenu (juce::AudioProcessorValueTreeState& state);

protected:
    juce::PopupMenu buildMenu() override;
};

} // namespace oe::ui
