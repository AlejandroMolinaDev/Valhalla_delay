#include "ui/Knob.h"

#include "ui/LookAndFeel.h"

namespace oe::ui
{

namespace
{
    constexpr int kNormalSensitivity = 220; // pixels for the full range
    constexpr int kFineSensitivity = 1400;
} // namespace

KnobSlider::KnobSlider() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow)
{
    setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    setMouseDragSensitivity (kNormalSensitivity);
    // No velocity mode, and the modifier keys must not swap into it: Ctrl/Cmd is our fine mode.
    setVelocityModeParameters (1.0, 1, 0.0, false);
    setTextBoxIsEditable (true);
    setScrollWheelEnabled (true);
    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
}

void KnobSlider::mouseDown (const juce::MouseEvent& e)
{
    const auto mods = e.mods;
    fineDrag = mods.isCommandDown() || mods.isCtrlDown() || mods.isShiftDown();
    setMouseDragSensitivity (fineDrag ? kFineSensitivity : kNormalSensitivity);
    juce::Slider::mouseDown (e);
}

void KnobSlider::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    fineDrag = false;
    setMouseDragSensitivity (kNormalSensitivity);
}

double KnobSlider::snapValue (double attemptedValue, DragMode mode)
{
    if (coarseSnap > 0.0 && ! fineDrag && mode != notDragging)
        return std::round (attemptedValue / coarseSnap) * coarseSnap;
    return attemptedValue;
}

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
            const juce::String& tooltip, bool bipolar)
    : paramId (id), title (t)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, static_cast<double> (p->convertFrom0to1 (p->getDefaultValue())));
    setName (title);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15); // title
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 17);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<OrbitLookAndFeel*> (&getLookAndFeel());
    g.setFont (lf != nullptr ? lf->font (11.5f, true) : juce::Font (juce::FontOptions (11.5f)));
    g.setColour (isEnabled() ? colours::textDim.brighter (0.15f) : colours::textDim.darker (0.4f));
    g.drawFittedText (title.toUpperCase(), getLocalBounds().removeFromTop (15), juce::Justification::centred, 1);
}

} // namespace oe::ui
