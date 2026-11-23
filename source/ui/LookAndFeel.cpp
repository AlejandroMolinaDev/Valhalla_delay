#include "ui/LookAndFeel.h"

#if OE_HAS_BINARY_DATA
 #include "BinaryData.h"
#endif

namespace oe::ui
{

OrbitLookAndFeel::OrbitLookAndFeel()
{
#if OE_HAS_BINARY_DATA
    medium = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsSemiBold_ttf, BinaryData::PoppinsSemiBold_ttfSize);
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
#endif

    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::amberDim);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::Label::textWhenEditingColourId, colours::text);
    setColour (juce::Label::backgroundWhenEditingColourId, colours::screen);
    setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    setColour (juce::TextEditor::backgroundColourId, colours::screen);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::amberDim);
    setColour (juce::TextEditor::outlineColourId, colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::orangeRed);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::accent);
    setColour (juce::TextButton::textColourOnId, colours::backgroundDeep);
    setColour (juce::TooltipWindow::backgroundColourId, colours::screen);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::accent);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::accent);
}

OrbitLookAndFeel::~OrbitLookAndFeel() = default;

juce::Font OrbitLookAndFeel::font (float height, bool isBold) const
{
    return font (height, isBold ? Weight::semiBold : Weight::medium);
}

juce::Font OrbitLookAndFeel::font (float height, Weight weight) const
{
    auto tf = weight == Weight::bold ? bold : (weight == Weight::semiBold ? semiBold : medium);
    if (tf != nullptr)
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    return juce::Font (juce::FontOptions (height, weight == Weight::medium ? juce::Font::plain : juce::Font::bold));
}

juce::Typeface::Ptr OrbitLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (medium != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() && semiBold != nullptr ? semiBold : medium;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void OrbitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h)).reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();
    auto pointerAt = [&centre, angle] (float r)
    { return juce::Point<float> (centre.x + r * std::sin (angle), centre.y - r * std::cos (angle)); };

    if (static_cast<bool> (slider.getProperties().getWithDefault ("hero", false)))
    {
        // Concentric rings (outer to inner), black core, black pointer through the rings.
        const auto& rings = colours::heroRings;
        const auto ringWidth = radius * 0.42f / static_cast<float> (rings.size());
        for (std::size_t i = 0; i < rings.size(); ++i)
        {
            const auto r = radius - static_cast<float> (i) * ringWidth;
            g.setColour (rings[rings.size() - 1 - i].withMultipliedAlpha (enabled ? 1.0f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        }
        const auto core = radius * 0.58f;
        g.setColour (juce::Colours::black);
        g.fillEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre));
        g.drawLine ({ pointerAt (core * 0.98f), pointerAt (radius * 1.02f) }, radius * 0.075f);
        return;
    }

    // Flat knob: solid disc in the section colour, black pointer from the centre area to the rim.
    const auto colourText = slider.getProperties().getWithDefault ("knobColour", "").toString();
    auto colour = colourText.isNotEmpty() ? juce::Colour::fromString (colourText) : colours::orange;
    if (! enabled)
        colour = colour.withMultipliedSaturation (0.25f).withMultipliedBrightness (0.6f);
    if (slider.isMouseOverOrDragging())
        colour = colour.brighter (0.08f);

    const auto disc = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    g.setColour (colour);
    g.fillEllipse (disc);
    g.setColour (juce::Colours::black);
    g.drawLine ({ pointerAt (radius * 0.42f), pointerAt (radius * 1.0f) }, juce::jmax (3.0f, radius * 0.14f));
}

juce::Label* OrbitLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    const auto hero = static_cast<bool> (slider.getProperties().getWithDefault ("hero", false));
    l->setFont (font (hero ? 21.0f : 17.0f, Weight::semiBold));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font OrbitLookAndFeel::getLabelFont (juce::Label& l)
{
    return font (l.getFont().getHeight(), Weight::semiBold);
}

void OrbitLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = b.getToggleState();
    const auto colourText = b.getProperties().getWithDefault ("accent", "").toString();
    const auto accent = colourText.isNotEmpty() ? juce::Colour::fromString (colourText) : colours::accent;
    const auto round = static_cast<bool> (b.getProperties().getWithDefault ("round", false));
    const auto corner = round ? r.getHeight() * 0.5f : 5.0f;

    if (on)
    {
        g.setColour (down ? accent.darker (0.15f) : accent);
        g.fillRoundedRectangle (r, corner);
    }
    else if (highlighted || down)
    {
        g.setColour (accent.withAlpha (down ? 0.3f : 0.15f));
        g.fillRoundedRectangle (r, corner);
    }
    if (static_cast<bool> (b.getProperties().getWithDefault ("outlined", true)))
    {
        g.setColour (accent);
        g.drawRoundedRectangle (r, corner, 2.0f);
    }
}

void OrbitLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const auto colourText = b.getProperties().getWithDefault ("accent", "").toString();
    const auto accent = colourText.isNotEmpty() ? juce::Colour::fromString (colourText) : colours::accent;
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::backgroundDeep : (highlighted ? accent.brighter (0.2f) : accent));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

juce::Font OrbitLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (17.0f, static_cast<float> (buttonHeight) * 0.62f), Weight::semiBold);
}

juce::Font OrbitLookAndFeel::getPopupMenuFont()
{
    return font (17.0f, Weight::medium);
}

void OrbitLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panel);
    g.setColour (colours::accent.withAlpha (0.6f));
    g.drawRect (0, 0, width, height, 1);
}

void OrbitLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (font (13.0f, Weight::bold));
    g.setColour (colours::accent);
    g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
}

void OrbitLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
    g.setColour (colours::screen.withAlpha (0.97f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::accent);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (14.0f, Weight::medium));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (10, 6), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> OrbitLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = font (14.0f, Weight::medium);
    const auto lineWidth = juce::GlyphArrangement::getStringWidthInt (f, tipText);
    const auto w = juce::jmin (320, lineWidth + 24);
    const auto lines = juce::jmax (1, (lineWidth + 24) / 320 + 1);
    const auto h = static_cast<int> (static_cast<float> (lines) * f.getHeight() * 1.35f) + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

juce::Font OrbitLookAndFeel::getAlertWindowTitleFont()
{
    return font (19.0f, Weight::bold);
}

juce::Font OrbitLookAndFeel::getAlertWindowMessageFont()
{
    return font (15.0f, Weight::medium);
}

} // namespace oe::ui
