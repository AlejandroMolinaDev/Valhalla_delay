#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OrbitEchoProcessor;
class PresetManager;

namespace oe::ui
{

/** Top bar: typographic logo, preset browser (previous / next, menu by
    category, save), A/B comparison and Mix Lock. */
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (OrbitEchoProcessor& processor);
    ~PresetBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void showSaveDialog();
    void refresh();

    OrbitEchoProcessor& processor;
    PresetManager& presets;
    juce::TextButton prev { "<" }, next { ">" }, name, save { "Save" };
    juce::TextButton a { "A" }, b { "B" }, lock { "Mix Lock" };
    std::unique_ptr<juce::AlertWindow> saveWindow;
};

} // namespace oe::ui
