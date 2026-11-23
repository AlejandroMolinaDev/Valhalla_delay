#include "ui/TextSelector.h"

#include "ui/LookAndFeel.h"

namespace oe::ui
{

namespace
{
    juce::Font selectorFont (juce::Component& c, bool value)
    {
        if (auto* lf = dynamic_cast<OrbitLookAndFeel*> (&c.getLookAndFeel()))
            return lf->font (21.0f, value ? OrbitLookAndFeel::Weight::semiBold : OrbitLookAndFeel::Weight::bold);
        return juce::Font (juce::FontOptions (21.0f));
    }
} // namespace

TextSelector::TextSelector (juce::String labelText, juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    : label (std::move (labelText))
{
    auto* param = state.getParameter (paramId);
    jassert (param != nullptr);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        choices = choice->choices;

    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        currentIndex = juce::jlimit (0, juce::jmax (0, choices.size() - 1), juce::roundToInt (v));
        valueText = choices[currentIndex];
        repaint();
    });
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::PopupMenu TextSelector::buildMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < choices.size(); ++i)
        m.addItem (i + 1, choices[i], true, i == currentIndex);
    return m;
}

void TextSelector::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    juce::Component::SafePointer<TextSelector> safe (this);
    buildMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0)
            safe->attachment->setValueAsCompleteGesture (static_cast<float> (result - 1));
    });
}

int TextSelector::getIdealWidth() const
{
    auto& self = const_cast<TextSelector&> (*this);
    int longest = 0;
    for (const auto& c : choices)
        longest = juce::jmax (longest, juce::GlyphArrangement::getStringWidthInt (selectorFont (self, true), c));
    return juce::GlyphArrangement::getStringWidthInt (selectorFont (self, false), label + ": ") + longest + 8;
}

void TextSelector::paint (juce::Graphics& g)
{
    const auto labelFont = selectorFont (*this, false);
    const auto valueFont = selectorFont (*this, true);
    const auto labelText = label + ": ";
    const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (labelFont, labelText);

    g.setFont (labelFont);
    g.setColour (colours::text);
    g.drawText (labelText, 0, 0, labelWidth, getHeight(), juce::Justification::centredLeft);

    g.setFont (valueFont);
    g.setColour (isMouseOver() ? colours::accent.brighter (0.25f) : colours::accent);
    g.drawText (valueText, labelWidth, 0, getWidth() - labelWidth, getHeight(), juce::Justification::centredLeft, true);
}

} // namespace oe::ui
