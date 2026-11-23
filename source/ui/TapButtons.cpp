#include "ui/TapButtons.h"

#include "Parameters.h"
#include "ui/LookAndFeel.h"

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
        heads[i].getProperties().set ("round", true);
        heads[i].getProperties().set ("accent", colours::amberLight.toString());
        heads[i].setTooltip (juce::String ("Playback head ") + names[i] + " on/off. With Spacing 0 % the heads sit at 1/4, "
                             "1/2, 3/4 and 4/4 of the delay time.");
        addAndMakeVisible (heads[i]);
        headAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ids[i], heads[i]);
    }

    repeat.setTooltip ("Repeat: only head D feeds back - rhythmic, repeating patterns.");
    swell.setTooltip ("Swell: every active head feeds back - multi-head resonance (with high Spacing: pseudo-reverb).");
    for (auto* b : { &repeat, &swell })
    {
        b->getProperties().set ("accent", colours::amberLight.toString());
        addAndMakeVisible (*b);
    }

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
    // Two rows: the four heads as round buttons, then Repeat / Swell.
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() / 2);
    const auto d = juce::jmin (top.getHeight() - 4, top.getWidth() / 4 - 4);
    const auto gap = (top.getWidth() - 4 * d) / 5;
    for (std::size_t i = 0; i < heads.size(); ++i)
        heads[i].setBounds (top.getX() + gap + static_cast<int> (i) * (d + gap), top.getCentreY() - d / 2, d, d);
    r.reduce (0, 3);
    repeat.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
    swell.setBounds (r.reduced (2, 0));
}

} // namespace oe::ui
