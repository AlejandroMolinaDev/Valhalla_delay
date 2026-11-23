#include "ui/Knob.h"

#include "ui/LookAndFeel.h"

namespace oe::ui
{

namespace
{
    constexpr int kNormalSensitivity = 220; // pixels for the full range
    constexpr int kFineSensitivity = 1400;
    constexpr int kTitleHeight = 24;
    constexpr int kValueHeight = 24;
} // namespace

KnobSlider::KnobSlider() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow)
{
    setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    setMouseDragSensitivity (kNormalSensitivity);
    // No velocity mode, and the modifier keys must not swap into it: Ctrl/Cmd is our fine mode.
    setVelocityModeParameters (1.0, 1, 0.0, false);
    setTextBoxIsEditable (true);
    setScrollWheelEnabled (true);
    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 110, kValueHeight);
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

void Knob::setTitle (const juce::String& t)
{
    if (t == title)
        return;
    title = t;
    repaint();
}

void Knob::setKnobColour (juce::Colour c)
{
    slider.getProperties().set ("knobColour", c.toString());
    slider.repaint();
}

void Knob::setHero (bool shouldBeHero)
{
    hero = shouldBeHero;
    slider.getProperties().set ("hero", hero);
    slider.lookAndFeelChanged(); // recreate the value box with the hero font
    resized();
}

void Knob::setTitleDimmed (bool dimmed)
{
    if (dimmed == titleDimmed)
        return;
    titleDimmed = dimmed;
    repaint();
}

void Knob::setProminentTitle (bool shouldBeProminent)
{
    prominent = shouldBeProminent;
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (hero ? kTitleHeight + 6 : kTitleHeight);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), hero ? kValueHeight + 4 : kValueHeight);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<OrbitLookAndFeel*> (&getLookAndFeel());
    const auto big = hero || prominent;
    const auto size = hero ? 24.0f : (prominent ? 20.0f : 17.0f);
    g.setFont (lf != nullptr ? lf->font (size, big ? OrbitLookAndFeel::Weight::bold : OrbitLookAndFeel::Weight::semiBold)
                             : juce::Font (juce::FontOptions (size)));
    g.setColour (! isEnabled() || titleDimmed ? colours::textDim : colours::text);
    g.drawFittedText (big ? title.toUpperCase() : title, getLocalBounds().removeFromTop (hero ? kTitleHeight + 6 : kTitleHeight),
                      juce::Justification::centred, 1);
}

} // namespace oe::ui
