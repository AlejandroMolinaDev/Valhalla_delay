#include "ui/TapButtons.h"

#include "Parameters.h"

namespace oe::ui
{

TapButtons::TapButtons (juce::AudioProcessorValueTreeState& state)
{
    const char* ids[] { params::id::tapA, params::id::tapB, params::id::tapC, params::id::tapD };
    const char* names[] { "A", "B", "C", "D" };
    for (std::size_t i = 0; i < heads.size(); ++i)
    {
        heads[i].setButtonText (names[i]);
        heads[i].setClickingTogglesState (true);
        heads[i].setTooltip (juce::String ("Playback head ") + names[i] + " on/off. With Spacing 0 % the heads sit at 1/4, "
                             "1/2, 3/4 and 4/4 of the delay time.");
        addAndMakeVisible (heads[i]);
        headAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ids[i], heads[i]);
    }

    repeat.setTooltip ("Repeat: only head D feeds back - rhythmic, repeating patterns.");
    swell.setTooltip ("Swell: every active head feeds back - multi-head resonance (with high Spacing: pseudo-reverb).");
    for (auto* b : { &repeat, &swell })
        addAndMakeVisible (*b);

    auto* param = state.getParameter (params::id::quadMode);
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        const auto isSwell = juce::roundToInt (v) == 1;
        repeat.setToggleState (! isSwell, juce::dontSendNotification);
        swell.setToggleState (isSwell, juce::dontSendNotification);
    });
    repeat.onClick = [this] { modeAttachment->setValueAsCompleteGesture (0.0f); };
    swell.onClick = [this] { modeAttachment->setValueAsCompleteGesture (1.0f); };
    modeAttachment->sendInitialUpdate();
}

void TapButtons::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() / 2).reduced (0, 2);
    const auto w = top.getWidth() / 4;
    for (auto& b : heads)
        b.setBounds (top.removeFromLeft (w).reduced (2, 0));
    r.reduce (0, 2);
    repeat.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
    swell.setBounds (r.reduced (2, 0));
}

} // namespace oe::ui
