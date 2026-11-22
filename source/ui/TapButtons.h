#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** Quad style: head A-D on/off buttons and the Repeat / Swell switch. */
class TapButtons final : public juce::Component
{
public:
    explicit TapButtons (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    std::array<juce::TextButton, 4> heads;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 4> headAttachments;
    juce::TextButton repeat { "Repeat" }, swell { "Swell" };
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
};

} // namespace oe::ui
