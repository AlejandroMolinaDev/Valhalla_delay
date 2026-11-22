#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class OrbitEchoEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OrbitEchoEditor (OrbitEchoProcessor&);
    ~OrbitEchoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OrbitEchoProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitEchoEditor)
};
