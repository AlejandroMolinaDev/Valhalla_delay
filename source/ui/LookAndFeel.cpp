#include "ui/LookAndFeel.h"

#if OE_HAS_BINARY_DATA
 #include "BinaryData.h"
#endif

namespace oe::ui
{

OrbitLookAndFeel::OrbitLookAndFeel()
{
#if OE_HAS_BINARY_DATA
    regular = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchMedium_ttf, BinaryData::ChakraPetchMedium_ttfSize);
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchSemiBold_ttf, BinaryData::ChakraPetchSemiBold_ttfSize);
#endif

    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::amberDim);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::Label::textWhenEditingColourId, colours::text);
    setColour (juce::Label::backgroundWhenEditingColourId, colours::screen);
    setColour (juce::Label::outlineWhenEditingColourId, colours::amber);
    setColour (juce::TextEditor::backgroundColourId, colours::screen);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::amberDim);
    setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::amber);
    setColour (juce::CaretComponent::caretColourId, colours::amber);
    setColour (juce::ComboBox::textColourId, colours::amber);
    setColour (juce::ComboBox::backgroundColourId, colours::screen);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, colours::amber);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::copper);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amberDim);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::buttonOnColourId, colours::amberDim);
    setColour (juce::TextButton::textColourOffId, colours::textDim);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::screen);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::amberDim);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::amberDim);
}

OrbitLookAndFeel::~OrbitLookAndFeel() = default;

juce::Font OrbitLookAndFeel::font (float height, bool isBold) const
{
    auto tf = isBold ? bold : regular;
    if (tf != nullptr)
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    return juce::Font (juce::FontOptions (height, isBold ? juce::Font::bold : juce::Font::plain));
}

juce::Typeface::Ptr OrbitLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (regular != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() && bold != nullptr ? bold : regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void OrbitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h)).reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();
    const auto accent = enabled ? colours::amber : colours::textDim;

    // Track + value arc (bipolar controls start from the centre).
    const auto arcRadius = radius - 2.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::screen);
    g.strokePath (track, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto bipolar = static_cast<bool> (slider.getProperties().getWithDefault ("bipolar", false));
    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body: drop shadow, metal gradient, bevel.
    const auto bodyRadius = radius * 0.74f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (colours::knobTop, centre.x, body.getY(), colours::knobBottom, centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (1.0f), 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (body, 1.0f);

    // Glowing indicator.
    const auto inner = bodyRadius * 0.28f;
    const auto outer = bodyRadius * 0.92f;
    const juce::Point<float> p1 (centre.x + inner * std::sin (angle), centre.y - inner * std::cos (angle));
    const juce::Point<float> p2 (centre.x + outer * std::sin (angle), centre.y - outer * std::cos (angle));
    g.setColour (accent.withAlpha (0.22f));
    g.drawLine ({ p1, p2 }, 6.0f);
    g.setColour (accent.withAlpha (0.45f));
    g.drawLine ({ p1, p2 }, 3.5f);
    g.setColour (accent.brighter (0.35f));
    g.drawLine ({ p1, p2 }, 1.6f);
}

juce::Label* OrbitLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (font (12.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font OrbitLookAndFeel::getLabelFont (juce::Label& l)
{
    return font (l.getFont().getHeight());
}

void OrbitLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = b.getToggleState();
    auto base = on ? colours::amberDim.withMultipliedSaturation (1.1f) : colours::screen;
    if (highlighted)
        base = base.brighter (0.12f);
    if (down)
        base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::amber.withAlpha (0.85f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (on)
    {
        g.setColour (colours::amber.withAlpha (0.12f));
        g.fillRoundedRectangle (r.expanded (1.5f), 5.0f);
    }
}

void OrbitLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::text : (b.isEnabled() ? colours::textDim.brighter (0.2f) : colours::textDim.darker (0.4f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

juce::Font OrbitLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (13.5f, static_cast<float> (buttonHeight) * 0.56f), true);
}

void OrbitLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (colours::screen);
    g.fillRoundedRectangle (r, 4.0f);
    // Faint scan-line glow of an LED display.
    g.setColour (colours::amber.withAlpha (box.isMouseOver() || down ? 0.10f : 0.05f));
    g.fillRoundedRectangle (r.reduced (2.0f), 3.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::amber : colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const auto ax = static_cast<float> (width) - 14.0f, ay = static_cast<float> (height) * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour (colours::amber);
    g.fillPath (arrow);
}

juce::Font OrbitLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (juce::jmin (17.0f, static_cast<float> (box.getHeight()) * 0.55f), true);
}

void OrbitLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font OrbitLookAndFeel::getPopupMenuFont()
{
    return font (15.0f);
}

void OrbitLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panel);
    g.setColour (colours::panelEdge);
    g.drawRect (0, 0, width, height, 1);
}

void OrbitLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (font (12.0f, true));
    g.setColour (colours::copper);
    g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
}

void OrbitLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
    g.setColour (colours::screen.withAlpha (0.96f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::amberDim);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (10, 6), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> OrbitLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = font (13.0f);
    const auto lineWidth = juce::GlyphArrangement::getStringWidthInt (f, tipText);
    const auto w = juce::jmin (300, lineWidth + 24);
    const auto lines = juce::jmax (1, (lineWidth + 24) / 300 + 1);
    const auto h = static_cast<int> (static_cast<float> (lines) * f.getHeight() * 1.35f) + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

} // namespace oe::ui
