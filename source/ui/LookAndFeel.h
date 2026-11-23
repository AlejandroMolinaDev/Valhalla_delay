#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

namespace oe::ui
{

/** Flat, warm palette: deep maroon panel, white legends, and section colours
    that run from light amber to dark crimson across the panel. */
namespace colours
{
    inline const juce::Colour background { 0xff230a08 };
    inline const juce::Colour backgroundDeep { 0xff140403 };
    inline const juce::Colour text { 0xffffffff };
    inline const juce::Colour textDim { 0xffc9a99a };
    inline const juce::Colour accent { 0xfff6a03a };   // values in the bottom bar, title first word
    inline const juce::Colour accentRed { 0xffdc3a2e }; // title second word
    inline const juce::Colour screen { 0xff170504 };
    inline const juce::Colour outline { 0xff4a1a14 };

    // Section colours, left to right.
    inline const juce::Colour amberLight { 0xfffbb847 };
    inline const juce::Colour amber { 0xfff79b33 };
    inline const juce::Colour orange { 0xfff2772c };
    inline const juce::Colour orangeRed { 0xffe5512d };
    inline const juce::Colour red { 0xffcb2f34 };
    inline const juce::Colour crimson { 0xff951820 };

    // Kept names used by older components.
    inline const juce::Colour copper { 0xffe5512d };
    inline const juce::Colour amberDim { 0xff8a3a1c };
    inline const juce::Colour panel { 0xff2c0e0b };
    inline const juce::Colour panelEdge { 0xff4a1a14 };

    /** Concentric rings of the main (Delay) knob, inner to outer. */
    inline const std::array<juce::Colour, 6> heroRings { juce::Colour (0xfffbc35a), juce::Colour (0xfff9a53c),
                                                         juce::Colour (0xfff27d2f), juce::Colour (0xffe5542e),
                                                         juce::Colour (0xffc8302f), juce::Colour (0xff8e1a1c) };
} // namespace colours

/** One instance per editor (no static JUCE objects: safe plugin unloading). */
class OrbitLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OrbitLookAndFeel();
    ~OrbitLookAndFeel() override;

    enum class Weight
    {
        medium,
        semiBold,
        bold
    };

    juce::Font font (float height, bool bold = false) const;
    juce::Font font (float height, Weight weight) const;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    /** Flat knob: solid disc in the knob's colour with a black pointer. The
        "hero" knob is drawn as concentric coloured rings around a black core. */
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float startAngle,
                           float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;

private:
    juce::Typeface::Ptr medium, semiBold, bold;
};

} // namespace oe::ui
