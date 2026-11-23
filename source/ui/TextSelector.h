#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** "LABEL: Value" text control for the bottom bar (label in white, value in
    the accent colour). Clicking it opens a menu; when attached to a choice
    parameter the menu items are its choices and the value follows the
    parameter (host automation included). */
class TextSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    TextSelector (juce::String label, juce::AudioProcessorValueTreeState& state, const juce::String& paramId);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    juce::String getValueText() const { return valueText; }

    /** Width needed to draw the label and the longest choice. */
    int getIdealWidth() const;

protected:
    /** Builds the popup menu; item ID = choice index + 1. Override to group items. */
    virtual juce::PopupMenu buildMenu();

    juce::StringArray choices;
    int currentIndex = 0;

private:
    juce::String label, valueText;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace oe::ui
