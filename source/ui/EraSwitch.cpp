#include "ui/EraSwitch.h"

#include "Parameters.h"
#include "dsp/Types.h"

namespace oe::ui
{

EraSwitch::EraSwitch (juce::AudioProcessorValueTreeState& state)
{
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        auto& b = buttons[i];
        b.setButtonText (dsp::kEraNames[i]);
        b.setClickingTogglesState (false);
        b.setTooltip ("Era: Past = darker and dirtier, Present = in between, Future = cleaner, more linear "
                      "and extended. Changes the hard-wired filters, saturation, modulation shapes and "
                      "diffusion of the current mode.");
        b.onClick = [this, i] { select (static_cast<int> (i)); };
        addAndMakeVisible (b);
    }

    auto* param = state.getParameter (params::id::era);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        const auto index = juce::jlimit (0, 2, juce::roundToInt (v));
        for (std::size_t i = 0; i < buttons.size(); ++i)
            buttons[i].setToggleState (static_cast<int> (i) == index, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void EraSwitch::select (int index)
{
    attachment->setValueAsCompleteGesture (static_cast<float> (index));
}

void EraSwitch::resized()
{
    auto r = getLocalBounds();
    const auto w = r.getWidth() / 3;
    for (auto& b : buttons)
        b.setBounds (r.removeFromLeft (w).reduced (1, 0));
}

} // namespace oe::ui
