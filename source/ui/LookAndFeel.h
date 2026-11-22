#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace oe::ui
{

/** Palette of the imaginary hardware echo unit: warm graphite panel, amber
    and copper accents, cream legends. */
namespace colours
{
    inline const juce::Colour background { 0xff1c1916 };
    inline const juce::Colour backgroundTop { 0xff2a2420 };
    inline const juce::Colour panel { 0xff2d2723 };
    inline const juce::Colour panelEdge { 0xff40372f };
    inline const juce::Colour screen { 0xff110e0c };
    inline const juce::Colour text { 0xffeadfcd };
    inline const juce::Colour textDim { 0xff9d8f7f };
    inline const juce::Colour amber { 0xfff2a541 };
    inline const juce::Colour amberDim { 0xff8a5f2a };
    inline const juce::Colour copper { 0xffc8703a };
    inline const juce::Colour knobTop { 0xff4a413a };
    inline const juce::Colour knobBottom { 0xff1d1917 };
} // namespace colours

/** One instance per editor (no static JUCE objects: safe plugin unloading). */
class OrbitLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OrbitLookAndFeel();
    ~OrbitLookAndFeel() override;

    juce::Font font (float height, bool bold = false) const;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float startAngle,
                           float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

private:
    juce::Typeface::Ptr regular, bold;
};

} // namespace oe::ui
