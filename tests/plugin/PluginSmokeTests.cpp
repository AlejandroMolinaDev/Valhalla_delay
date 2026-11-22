#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

TEST_CASE ("Plugin passes audio through and supports the required bus layouts", "[plugin][smoke]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    OrbitEchoProcessor p;

    using CS = juce::AudioChannelSet;
    auto layout = [] (CS in, CS out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };

    REQUIRE (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::stereo())));
    REQUIRE (p.checkBusesLayoutSupported (layout (CS::mono(), CS::stereo())));
    REQUIRE (p.checkBusesLayoutSupported (layout (CS::mono(), CS::mono())));
    REQUIRE_FALSE (p.checkBusesLayoutSupported (layout (CS::create5point1(), CS::create5point1())));

    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();
    buffer.setSample (0, 10, 0.5f);
    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);
    REQUIRE (buffer.getNumSamples() == 256);
}
