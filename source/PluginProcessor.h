#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/DelayEngine.h"
#include "ui/VisualizerData.h"

class PresetManager;

class OrbitEchoProcessor final : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int kDefaultEditorWidth = 1120;
    static constexpr int kDefaultEditorHeight = 448;

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
    PresetManager& getPresetManager() noexcept { return *presetManager; }
    oe::dsp::EngineParams currentEngineParams() const noexcept { return paramRefs.load (lastBpm.load()); }
    double getLastBpm() const noexcept { return lastBpm.load(); }

    /** Pops the most recent visualiser frame (message thread). Returns false if none is new. */
    bool popVisualizerFrame (oe::ui::VisualizerFrame& out) noexcept;

    // Editor size (saved in the plugin state, not in presets).
    std::atomic<int> editorWidth { kDefaultEditorWidth };

    // Mix Lock: loading a preset keeps the current Mix (plugin state, not preset).
    std::atomic<bool> mixLock { false };

    const oe::dsp::DelayEngine& getEngine() const noexcept { return engine; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void processInternal (juce::AudioBuffer<float>& buffer, bool bypassed);
    double readHostBpm() noexcept;
    void publishVisualizerFrame (const oe::dsp::EngineParams& p, int numSamples) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    oe::params::Refs paramRefs;
    oe::dsp::DelayEngine engine;
    std::unique_ptr<PresetManager> presetManager;
    std::atomic<double> lastBpm { 120.0 };

    // Scratch for the mono-output case, processed in chunks (hosts may send
    // blocks larger than announced in prepareToPlay; nothing is allocated here).
    static constexpr int kScratchSize = 1024;
    std::array<float, kScratchSize> scratch {};

    // Visualiser FIFO (single producer: audio thread, single consumer: UI).
    static constexpr int kVisFifoSize = 16;
    juce::AbstractFifo visFifo { kVisFifoSize };
    std::array<oe::ui::VisualizerFrame, kVisFifoSize> visFrames {};
    int visSamplesUntilFrame = 0;
    double sampleRateForVis = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitEchoProcessor)
};
