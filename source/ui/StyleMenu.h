#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** Style selector (L/R topology): Single, Dual, Ratio, PingPong, Quad. */
class StyleMenu final : public juce::ComboBox
{
public:
    explicit StyleMenu (juce::AudioProcessorValueTreeState& state);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

} // namespace oe::ui
