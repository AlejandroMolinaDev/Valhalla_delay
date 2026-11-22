#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** Three-position Era switch (Past / Present / Future) bound to "era". */
class EraSwitch final : public juce::Component
{
public:
    explicit EraSwitch (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    void select (int index);

    std::array<juce::TextButton, 3> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace oe::ui
