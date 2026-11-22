#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

/** Original "imaginary hardware echo unit" interface.

    The whole panel is laid out at a base size of 900 x 480 and scaled as one
    piece (75 %-200 %, fixed aspect ratio, size remembered in the plugin
    state), so every host — including FL Studio's wrapper with Windows display
    scaling — shows it crisp and proportioned. Controls that do not apply to
    the current Mode x Style fade out; their values are kept. */
class OrbitEchoEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseWidth = OrbitEchoProcessor::kDefaultEditorWidth;
    static constexpr int kBaseHeight = OrbitEchoProcessor::kDefaultEditorHeight;

    explicit OrbitEchoEditor (OrbitEchoProcessor&);
    ~OrbitEchoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Test hook: whether the control bound to a parameter is currently shown. */
    bool isControlShown (const juce::String& paramId) const;

    /** Re-reads Mode / Style / sync immediately (normally done by a 15 Hz timer). */
    void updateVisibility (bool animate = false);

    class Content;

private:
    void timerCallback() override;

    OrbitEchoProcessor& orbitProcessor;
    oe::ui::OrbitLookAndFeel lookAndFeel;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 650 };
    bool constructed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitEchoEditor)
};
