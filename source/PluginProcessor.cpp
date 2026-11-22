#include "PluginProcessor.h"
#include "PluginEditor.h"

OrbitEchoProcessor::OrbitEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

OrbitEchoProcessor::~OrbitEchoProcessor() = default;

void OrbitEchoProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (0);
    engine.prepare (sampleRate);
}

void OrbitEchoProcessor::releaseResources() {}

bool OrbitEchoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // mono->mono, mono->stereo and stereo->stereo.
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();

    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();

    return false;
}

void OrbitEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, false);
}

void OrbitEchoProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, true);
}

void OrbitEchoProcessor::processInternal (juce::AudioBuffer<float>& buffer, bool)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (getTotalNumInputChannels() == 1 && numChannels > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
}

double OrbitEchoProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

juce::AudioProcessorEditor* OrbitEchoProcessor::createEditor()
{
    return new OrbitEchoEditor (*this);
}

void OrbitEchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("OrbitEchoState");
    xml.setAttribute ("version", JucePlugin_VersionString);
    copyXmlToBinary (xml, destData);
}

void OrbitEchoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    juce::ignoreUnused (xml);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitEchoProcessor();
}
