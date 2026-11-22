#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/DelayEngine.h"

class OrbitEchoProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kDefaultEditorWidth = 900;
    static constexpr int kDefaultEditorHeight = 480;

    OrbitEchoProcessor();
    ~OrbitEchoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    oe::dsp::EngineParams currentEngineParams() const noexcept { return paramRefs.load (lastBpm.load()); }
    double getLastBpm() const noexcept { return lastBpm.load(); }

    // Editor size (saved in the plugin state, not in presets).
    std::atomic<int> editorWidth { kDefaultEditorWidth };

    // Mix Lock: loading a preset keeps the current Mix (plugin state, not preset).
    std::atomic<bool> mixLock { false };

    /** Read-only access for the visualiser / tests (audio-thread data, approximate). */
    const oe::dsp::DelayEngine& getEngine() const noexcept { return engine; }

private:
    void processInternal (juce::AudioBuffer<float>& buffer, bool bypassed);
    double readHostBpm() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    oe::params::Refs paramRefs;
    oe::dsp::DelayEngine engine;
    std::atomic<double> lastBpm { 120.0 };

    // Scratch for the mono-output case, processed in chunks (hosts may send
    // blocks larger than announced in prepareToPlay; nothing is allocated here).
    static constexpr int kScratchSize = 1024;
    std::array<float, kScratchSize> scratch {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitEchoProcessor)
};
