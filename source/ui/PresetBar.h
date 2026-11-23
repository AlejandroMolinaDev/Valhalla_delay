#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OrbitEchoProcessor;
class PresetManager;

namespace oe::ui
{

/** Bottom bar: "PRESETS: name" (click = menu by category, save, rescan)
    with previous / next arrows. */
class PresetBar final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PresetBar (OrbitEchoProcessor& processor);
    ~PresetBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    void showSaveDialog();

private:
    void timerCallback() override;
    void showPresetMenu();
    juce::Rectangle<int> nameArea() const;

    PresetManager& presets;
    juce::String shownName;
    juce::ArrowButton prev { "previous", 0.5, juce::Colour (0xfff6a03a) };
    juce::ArrowButton next { "next", 0.0, juce::Colour (0xfff6a03a) };
    std::unique_ptr<juce::AlertWindow> saveWindow;
};

/** Header: SAVE, A / B comparison. */
class CompareBar final : public juce::Component, private juce::Timer
{
public:
    CompareBar (OrbitEchoProcessor& processor, PresetBar& presetBar);
    ~CompareBar() override;

    void resized() override;

private:
    void timerCallback() override;

    PresetManager& presets;
    juce::TextButton save { "SAVE" }, a { "A" }, b { "B" };
};

} // namespace oe::ui
