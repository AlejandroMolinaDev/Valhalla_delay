#include "PluginEditor.h"

OrbitEchoEditor::OrbitEchoEditor (OrbitEchoProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (900, 480);
}

OrbitEchoEditor::~OrbitEchoEditor() = default;

void OrbitEchoEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff24211f));
    g.setColour (juce::Colour (0xffe0a04a));
    g.setFont (juce::FontOptions (28.0f));
    g.drawFittedText ("Orbit Echo", getLocalBounds(), juce::Justification::centred, 1);
}

void OrbitEchoEditor::resized() {}
