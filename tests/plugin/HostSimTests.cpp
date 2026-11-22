// Host-simulation tests: call patterns real DAWs use, with FL Studio's
// behaviour as the main reference (variable / oversized / zero-length blocks,
// repeated prepareToPlay, state calls while audio runs, host bypass, tempo).

#include <atomic>
#include <thread>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AllocationCounter.h"
#include "PluginTestHelpers.h"

using namespace oe;
using Catch::Approx;

TEST_CASE ("Bus layouts: mono->mono, mono->stereo, stereo->stereo", "[host][layout]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
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
    REQUIRE_FALSE (p.checkBusesLayoutSupported (layout (CS::stereo(), CS::mono())));
    REQUIRE_FALSE (p.checkBusesLayoutSupported (layout (CS::create5point1(), CS::create5point1())));

    for (const auto& [ins, outs] : { std::pair { 1, 1 }, std::pair { 1, 2 }, std::pair { 2, 2 } })
    {
        test::prepare (p, 48000.0, 256, ins, outs);
        test::cleanDigital (p, 100.0f);
        juce::AudioBuffer<float> buffer (2, 9600);
        buffer.clear();
        buffer.setSample (0, 0, 0.5f);
        if (ins == 2)
            buffer.setSample (1, 0, 0.5f);
        test::processInChunks (p, buffer, { 256 });
        REQUIRE (test::isFinite (buffer));
        INFO ("ins " << ins << " outs " << outs);
        REQUIRE (std::abs (test::argmaxAbs (buffer, 0) - 4800) <= 1);
        if (outs == 2)
            REQUIRE (std::abs (test::argmaxAbs (buffer, 1) - 4800) <= 1); // mono input reaches both sides
    }
}

TEST_CASE ("Oversized, tiny and zero-length blocks (FL Studio variable buffers)", "[host][blocks]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto run = [] (const std::vector<int>& sizes)
    {
        OrbitEchoProcessor p;
        test::prepare (p, 48000.0, 512); // announce 512 ...
        test::setParam (p, params::id::mode, static_cast<float> (dsp::Mode::Digital));
        test::setParam (p, params::id::feedback, 80.0f);
        test::setParam (p, params::id::syncL, 0.0f);
        test::setParam (p, params::id::delayL, 60.0f);
        juce::AudioBuffer<float> b (2, 48000);
        juce::Random rng (42);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
        test::processInChunks (p, b, sizes); // ... then send anything
        return b;
    };

    const auto reference = run ({ 64 });
    const auto fl = run ({ 1, 7, 512, 513, 4096, 8192, 33 });
    REQUIRE (test::isFinite (fl));
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < reference.getNumSamples(); ++i)
            REQUIRE (fl.getSample (ch, i) == reference.getSample (ch, i));

    // Zero-length buffers are ignored.
    OrbitEchoProcessor p;
    test::prepare (p, 48000.0, 512);
    juce::AudioBuffer<float> empty (2, 0);
    juce::MidiBuffer midi;
    p.processBlock (empty, midi);
    p.processBlockBypassed (empty, midi);
    SUCCEED();
}

TEST_CASE ("processBlock before prepareToPlay is a safe passthrough", "[host]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    juce::AudioBuffer<float> b (2, 128);
    for (int i = 0; i < 128; ++i)
    {
        b.setSample (0, i, 0.25f);
        b.setSample (1, i, -0.25f);
    }
    juce::MidiBuffer midi;
    p.processBlock (b, midi);
    REQUIRE (b.getSample (0, 100) == 0.25f);
    REQUIRE (b.getSample (1, 100) == -0.25f);
}

TEST_CASE ("prepareToPlay called repeatedly with different sample rates", "[host][samplerate]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::prepare (p, 44100.0, 256);
    test::cleanDigital (p, 250.0f);

    for (const double fs : { 44100.0, 96000.0, 48000.0, 192000.0, 48000.0 })
    {
        test::prepare (p, fs, fs > 90000.0 ? 1024 : 256);
        REQUIRE (test::getParam (p, params::id::delayL) == Approx (250.0f).margin (0.1f)); // values survive
        juce::AudioBuffer<float> b (2, static_cast<int> (0.4 * fs));
        b.clear();
        b.setSample (0, 0, 0.5f);
        b.setSample (1, 0, 0.5f);
        test::processInChunks (p, b, { 480 });
        INFO ("fs " << fs);
        REQUIRE (std::abs (test::argmaxAbs (b, 0) - static_cast<int> (0.25 * fs)) <= 1);
    }
}

TEST_CASE ("State calls while the audio thread runs (FL Studio undo history)", "[host][threads]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::prepare (p, 48000.0, 128);

    juce::MemoryBlock stateA, stateB;
    test::setParam (p, params::id::mode, 3.0f);
    test::setParam (p, params::id::feedback, 120.0f);
    p.getStateInformation (stateA);
    test::setParam (p, params::id::mode, 7.0f);
    test::setParam (p, params::id::style, 4.0f);
    test::setParam (p, params::id::feedback, 20.0f);
    p.getStateInformation (stateB);

    std::atomic<bool> stop { false };
    std::atomic<bool> finite { true };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> b (2, 128);
        juce::MidiBuffer midi;
        juce::Random rng (3);
        while (! stop.load())
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                    b.setSample (ch, i, 0.2f * (rng.nextFloat() - 0.5f));
            p.processBlock (b, midi);
            if (! test::isFinite (b))
                finite = false;
        }
    });

    for (int i = 0; i < 200; ++i)
    {
        const auto& s = (i % 2 == 0) ? stateA : stateB;
        p.setStateInformation (s.getData(), static_cast<int> (s.getSize()));
        juce::MemoryBlock tmp;
        p.getStateInformation (tmp);
        juce::Thread::sleep (1);
    }
    stop = true;
    audio.join();
    REQUIRE (finite.load());
}

TEST_CASE ("Host bypass passes the dry signal and fades the wet without clicks", "[host][bypass]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::prepare (p, 48000.0, 256);
    test::setParam (p, params::id::mix, 50.0f);
    test::setParam (p, params::id::feedback, 60.0f);

    const int seg = 24000;
    juce::AudioBuffer<float> b (2, seg * 6);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const auto v = 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / 48000.0));
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    const auto dry = b;

    juce::MidiBuffer midi;
    for (int s = 0; s < 6; ++s)
    {
        juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, s * seg, seg);
        test::processInChunks (p, view, { 256 }, s % 2 == 1);
    }

    REQUIRE (test::isFinite (b));
    float maxJump = 0.0f;
    for (int i = 1; i < b.getNumSamples(); ++i)
        maxJump = std::max (maxJump, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
    REQUIRE (maxJump < 0.12f); // a 220 Hz sine moves at most ~0.03 per sample; a click would jump far more

    // Deep inside a bypassed segment the output is exactly the dry input.
    for (int i = seg + seg / 2; i < 2 * seg; ++i)
        REQUIRE (b.getSample (0, i) == dry.getSample (0, i));
}

TEST_CASE ("Tempo from the play head, fallback to 120 BPM, tempo changes", "[host][tempo]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::prepare (p, 48000.0, 256);
    test::cleanDigital (p, 100.0f);
    test::setParam (p, params::id::syncL, static_cast<float> (dsp::kSyncQuarter));

    auto echoAt = [&p]
    {
        p.prepareToPlay (48000.0, 256); // fresh buffers
        juce::AudioBuffer<float> b (2, 48000);
        b.clear();
        b.setSample (0, 0, 0.5f);
        b.setSample (1, 0, 0.5f);
        test::processInChunks (p, b, { 256 });
        return test::argmaxAbs (b, 0);
    };

    REQUIRE (std::abs (echoAt() - 24000) <= 1); // no play head -> 120 BPM -> 500 ms

    test::TestPlayHead head;
    head.bpm = 90.0;
    p.setPlayHead (&head);
    REQUIRE (std::abs (echoAt() - 32000) <= 1); // 1/4 at 90 BPM = 666.7 ms

    head.reportTempo = false; // host stops reporting: keep the last tempo
    REQUIRE (std::abs (echoAt() - 32000) <= 1);

    // Tempo automation while running: glides, stays finite.
    head.reportTempo = true;
    juce::AudioBuffer<float> b (2, 256);
    juce::MidiBuffer midi;
    for (int i = 0; i < 400; ++i)
    {
        head.bpm = 60.0 + (i % 100);
        for (int s = 0; s < 256; ++s)
            b.setSample (0, s, 0.1f), b.setSample (1, s, 0.1f);
        p.processBlock (b, midi);
        REQUIRE (test::isFinite (b));
    }
    p.setPlayHead (nullptr);
}

TEST_CASE ("No heap allocation on the audio thread", "[host][realtime]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::TestPlayHead head;
    p.setPlayHead (&head);
    test::prepare (p, 48000.0, 512);

    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (11);
    std::size_t total = 0;

    for (int block = 0; block < 600; ++block)
    {
        // Parameter changes happen on the "UI" side, outside the counted region.
        if (block % 20 == 0)
        {
            test::setParam (p, params::id::mode, static_cast<float> ((block / 20) % dsp::kNumModes));
            test::setParam (p, params::id::style, static_cast<float> ((block / 40) % dsp::kNumStyles));
            test::setParam (p, params::id::era, static_cast<float> ((block / 60) % 3));
            test::setParam (p, params::id::feedback, static_cast<float> ((block * 7) % 200));
            test::setParam (p, params::id::delayL, static_cast<float> (5 + (block * 37) % 2000));
            test::setParam (p, params::id::duck, static_cast<float> ((block * 13) % 100));
            head.bpm = 70.0 + block % 80;
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                b.setSample (ch, i, 0.3f * (rng.nextFloat() - 0.5f));

        test::ScopedAllocationCounter counter;
        if (block % 97 == 50)
            p.processBlockBypassed (b, midi);
        else
            p.processBlock (b, midi);
        total += counter.count();
    }

    p.setPlayHead (nullptr);
    REQUIRE (total == 0);
}

TEST_CASE ("Tail length reported to the host", "[host][tail]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::setParam (p, params::id::feedback, 100.0f);
    REQUIRE (p.getTailLengthSeconds() == Approx (60.0));
    test::setParam (p, params::id::feedback, 200.0f);
    REQUIRE (p.getTailLengthSeconds() == Approx (60.0));
    test::setParam (p, params::id::feedback, 35.0f);
    test::setParam (p, params::id::syncL, 0.0f);
    test::setParam (p, params::id::delayL, 500.0f);
    const auto t = p.getTailLengthSeconds();
    REQUIRE (t > 2.0);
    REQUIRE (t < 60.0);
}

TEST_CASE ("Every parameter: value -> text -> value round trip", "[host][params]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        REQUIRE (ranged != nullptr);
        for (const float v : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
        {
            const auto text = ranged->getText (v, 64);
            const auto back = ranged->getValueForText (text);
            INFO (ranged->getParameterID() << " v=" << v << " text='" << text << "' back=" << back);
            REQUIRE_FALSE (text.isEmpty());
            const auto a = ranged->convertFrom0to1 (v);
            const auto b2 = ranged->convertFrom0to1 (back);
            const auto span = ranged->getNormalisableRange().end - ranged->getNormalisableRange().start;
            REQUIRE (std::abs (a - b2) <= std::max (0.011f * std::abs (a), 0.002f * span));
        }
    }
}
