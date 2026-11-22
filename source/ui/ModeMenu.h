#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** Mode selector: an LED-style display; clicking opens the list of the 17
    algorithms grouped by family. Attached to the "mode" parameter. */
class ModeMenu final : public juce::ComboBox
{
public:
    explicit ModeMenu (juce::AudioProcessorValueTreeState& state);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

} // namespace oe::ui
