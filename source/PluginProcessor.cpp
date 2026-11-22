#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/TempoSync.h"

namespace
{
    const juce::Identifier kStateType { "OrbitEchoState" };
    const juce::Identifier kEditorWidthProp { "editorWidth" };
    const juce::Identifier kMixLockProp { "mixLock" };
    const juce::Identifier kVersionProp { "pluginVersion" };
} // namespace

OrbitEchoProcessor::OrbitEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateType, oe::params::createLayout()),
      paramRefs (apvts)
{
    // The constructor allocates nothing large: the 20 s delay lines are created
    // in prepareToPlay, which keeps plugin scanning fast (FL Studio, Live, ...).
}

OrbitEchoProcessor::~OrbitEchoProcessor() = default;

void OrbitEchoProcessor::prepareToPlay (double sampleRate, int)
{
    // May be called repeatedly and with different rates (device change, offline
    // render): the engine only reallocates when the sample rate changes.
    setLatencySamples (0);
    engine.setParameters (paramRefs.load (lastBpm.load()));
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

double OrbitEchoProcessor::readHostBpm() noexcept
{
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
            if (const auto bpm = position->getBpm())
                if (*bpm >= 20.0 && *bpm <= 999.0)
                    return *bpm;

    return lastBpm.load(); // keep the last known tempo (120 BPM until a host reports one)
}

void OrbitEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, false);
}

void OrbitEchoProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, true);
}

void OrbitEchoProcessor::processInternal (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return; // zero-length calls (parameter flushes in some hosts)

    const auto numIn = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();

    // Mono input on a stereo bus: the second channel is not valid input, duplicate the first.
    if (numIn == 1 && numChannels > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    for (auto ch = std::max (2, numOut); ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (! engine.isPrepared())
        return; // processBlock before prepareToPlay: dry passthrough

    const auto bpm = readHostBpm();
    lastBpm.store (bpm);
    engine.setParameters (paramRefs.load (bpm));
    engine.setBypassed (bypassed);

    auto* left = buffer.getWritePointer (0);

    if (numChannels > 1 && numOut > 1)
    {
        engine.process (left, buffer.getWritePointer (1), numSamples);
        return;
    }

    // Mono output: run the stereo engine with a duplicated input and fold down.
    for (int start = 0; start < numSamples; start += kScratchSize)
    {
        const auto n = std::min (kScratchSize, numSamples - start);
        auto* l = left + start;
        std::copy (l, l + n, scratch.begin());
        engine.process (l, scratch.data(), n);
        for (int i = 0; i < n; ++i)
            l[i] = 0.5f * (l[i] + scratch[static_cast<std::size_t> (i)]);
    }
}

double OrbitEchoProcessor::getTailLengthSeconds() const
{
    // Generous estimate so that offline renders (FL Studio, Reaper, Live...) and
    // FL Studio's Smart Disable never cut the echoes; 60 s with Feedback >= 100 %.
    return oe::dsp::DelayEngine::estimateTailSeconds (paramRefs.load (lastBpm.load()));
}

juce::AudioProcessorEditor* OrbitEchoProcessor::createEditor()
{
    return new OrbitEchoEditor (*this);
}

void OrbitEchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kEditorWidthProp, editorWidth.load(), nullptr);
    state.setProperty (kMixLockProp, mixLock.load(), nullptr);
    state.setProperty (kVersionProp, JucePlugin_VersionString, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OrbitEchoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateType))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    editorWidth = juce::jlimit (kDefaultEditorWidth * 3 / 4, kDefaultEditorWidth * 2,
                                static_cast<int> (state.getProperty (kEditorWidthProp, kDefaultEditorWidth)));
    mixLock = static_cast<bool> (state.getProperty (kMixLockProp, false));
    state.removeProperty (kEditorWidthProp, nullptr);
    state.removeProperty (kMixLockProp, nullptr);
    state.removeProperty (kVersionProp, nullptr);

    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitEchoProcessor();
}
