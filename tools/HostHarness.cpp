// oe_host_harness: loads the BUILT plugin (.vst3, or .component on macOS)
// through JUCE's hosting layer — i.e. through the real VST3 / AU wrapper, as a
// DAW does — and drives it with the call patterns hosts use, FL Studio first:
//
//  - identity, I/O, latency, tail, parameter list (stable, unique, named)
//  - variable, tiny and zero-length blocks up to the announced maximum, and a
//    large (8192) maximum block size (VST3 forbids exceeding the announced
//    maximum; the processor itself also survives that — see HostSimTests)
//  - repeated prepareToPlay with sample-rate changes
//  - parameter values and text through the host interface ("Browse parameters")
//  - echo timing through the wrapper (Delay = 250 ms -> echo at 250 ms)
//  - state save / restore into a second instance (project reload)
//  - state calls on the message thread while audio runs (undo history)
//  - host bypass (wrapper bypass parameter): dry passes through
//
// Exit code 0 = every check passed.

#include <atomic>
#include <cmath>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok)
            ++failures;
    }

    juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
    {
        for (auto* param : p.getParameters())
            if (param->getName (64) == name)
                return param;
        return nullptr;
    }

    bool setByText (juce::AudioPluginInstance& p, const juce::String& name, const juce::String& text)
    {
        auto* param = findParam (p, name);
        if (param == nullptr)
            return false;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValueForText (text));
        param->endChangeGesture();
        return true;
    }

    void noise (juce::AudioBuffer<float>& b, juce::Random& rng, float amp)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, amp * (rng.nextFloat() * 2.0f - 1.0f));
    }

    bool finite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    void pumpMessages()
    {
        // Let the wrapper deliver parameter changes posted to the message thread.
        for (int i = 0; i < 10; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    }

    std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& fm, const juce::String& path,
                                                     double sampleRate, int block)
    {
        for (auto* format : fm.getFormats())
        {
            if (! format->fileMightContainThisPluginType (path))
                continue;
            juce::OwnedArray<juce::PluginDescription> types;
            format->findAllTypesForFile (types, path);
            if (types.isEmpty())
                continue;
            juce::String error;
            if (auto instance = fm.createPluginInstance (*types[0], sampleRate, block, error))
                return instance;
            std::printf ("load error: %s\n", error.toRawUTF8());
        }
        return {};
    }
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: oe_host_harness <path to .vst3 / .component>\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    const juce::String path = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).getFullPathName();
    std::printf ("Host harness: %s\n", path.toRawUTF8());

    auto plugin = load (formats, path, 48000.0, 512);
    check (plugin != nullptr, "plugin loads through the host wrapper");
    if (plugin == nullptr)
        return 1;

    auto& p = *plugin;
    const auto desc = p.getPluginDescription();
    check (desc.name == "Orbit Echo", "name is 'Orbit Echo' (" + desc.name + ")");
    check (desc.manufacturerName == "Alejandro Audio", "manufacturer is 'Alejandro Audio'");
    check (! desc.isInstrument, "registered as an effect, not an instrument");
    check (! p.acceptsMidi(), "no MIDI input (no phantom MIDI CC parameters in FL Studio)");
    check (p.getTotalNumInputChannels() == 2 && p.getTotalNumOutputChannels() == 2, "stereo in / stereo out");

    // Parameters ---------------------------------------------------------------
    juce::StringArray names;
    bool allNamed = true;
    for (auto* param : p.getParameters())
    {
        const auto n = param->getName (64);
        allNamed = allNamed && n.isNotEmpty();
        names.add (n);
    }
    auto unique = names;
    unique.removeDuplicates (false);
    check (names.size() >= 35, "parameter count >= 35 (" + juce::String (names.size()) + ")");
    check (allNamed, "every parameter has a name");
    check (unique.size() == names.size(), "parameter names are unique");
    for (const auto* required : { "Mix", "Mode", "Style", "Era", "Delay", "Feedback", "Width", "Drive", "Diffusion", "Duck" })
        check (names.contains (required), juce::String ("parameter '") + required + "' exposed");

    // Prepare / process with FL Studio-like buffers ----------------------------
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    check (p.getLatencySamples() == 0, "zero latency");
    check (p.getTailLengthSeconds() > 0.0, "tail length reported (" + juce::String (p.getTailLengthSeconds(), 2) + " s)");

    juce::Random rng (1234);
    juce::MidiBuffer midi;
    bool ok = true;
    for (const int n : { 512, 1, 7, 33, 0, 511, 128, 2, 500 })
    {
        juce::AudioBuffer<float> b (2, n);
        noise (b, rng, 0.3f);
        p.processBlock (b, midi);
        ok = ok && finite (b);
    }
    check (ok, "variable / tiny / zero-length blocks process cleanly");

    p.releaseResources();
    p.setRateAndBufferSizeDetails (48000.0, 8192);
    p.prepareToPlay (48000.0, 8192);
    ok = true;
    for (const int n : { 8192, 3000, 8192, 1 })
    {
        juce::AudioBuffer<float> b (2, n);
        noise (b, rng, 0.3f);
        p.processBlock (b, midi);
        ok = ok && finite (b);
    }
    check (ok, "large host buffers (8192 samples, e.g. FL Studio's biggest setting)");

    ok = true;
    for (const double sr : { 96000.0, 44100.0, 192000.0, 48000.0 })
    {
        p.releaseResources();
        p.setRateAndBufferSizeDetails (sr, 256);
        p.prepareToPlay (sr, 256);
        juce::AudioBuffer<float> b (2, 256);
        for (int k = 0; k < 20; ++k)
        {
            noise (b, rng, 0.3f);
            p.processBlock (b, midi);
            ok = ok && finite (b);
        }
    }
    check (ok, "prepareToPlay repeated with 96 / 44.1 / 192 / 48 kHz");

    // Echo timing through the wrapper's parameter interface --------------------
    ok = setByText (p, "Mode", "Digital") && setByText (p, "Style", "Single") && setByText (p, "Era", "Future")
      && setByText (p, "Delay Sync", "ms") && setByText (p, "Delay", "250 ms") && setByText (p, "Feedback", "0")
      && setByText (p, "Mix", "100") && setByText (p, "Mod Depth", "0") && setByText (p, "Age", "0")
      && setByText (p, "Low Cut", "10") && setByText (p, "High Cut", "20000") && setByText (p, "Diffusion", "0");
    check (ok, "parameters can be set from text through the host interface");
    pumpMessages();
    if (auto* d = findParam (p, "Delay"))
        check (d->getText (d->getValue(), 32).startsWith ("250"), "parameter text reads back (" + d->getText (d->getValue(), 32) + ")");

    p.releaseResources();
    p.prepareToPlay (48000.0, 512);
    {
        juce::AudioBuffer<float> b (2, 24000);
        b.clear();
        b.setSample (0, 0, 0.5f);
        b.setSample (1, 0, 0.5f);
        for (int pos = 0; pos < b.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, pos, juce::jmin (512, b.getNumSamples() - pos));
            p.processBlock (view, midi);
        }
        int peak = 0;
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::abs (b.getSample (0, i)) > std::abs (b.getSample (0, peak)))
                peak = i;
        check (std::abs (peak - 12000) <= 1, "echo lands at 250 ms through the wrapper (sample " + juce::String (peak) + ")");
    }

    // State save / restore into a second instance ------------------------------
    setByText (p, "Mode", "Tape NR");
    setByText (p, "Feedback", "77");
    setByText (p, "Wow", "61");
    setByText (p, "Pitch Shift", "-5");
    pumpMessages();
    juce::MemoryBlock state;
    p.getStateInformation (state);
    check (state.getSize() > 0, "state is saved");

    auto second = load (formats, path, 48000.0, 512);
    check (second != nullptr, "second instance loads");
    if (second != nullptr)
    {
        second->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        pumpMessages();
        ok = true;
        for (int i = 0; i < p.getParameters().size(); ++i)
        {
            const auto a = p.getParameters()[i]->getValue();
            const auto b = second->getParameters()[i]->getValue();
            if (std::abs (a - b) > 1.0e-4f)
            {
                ok = false;
                std::printf ("  mismatch: %s %f vs %f\n", p.getParameters()[i]->getName (64).toRawUTF8(), a, b);
            }
        }
        check (ok, "restored instance has identical parameter values");
    }

    // State calls while audio runs on another thread ---------------------------
    {
        std::atomic<bool> stop { false }, clean { true };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> b (2, 128);
            juce::Random r (9);
            juce::MidiBuffer m;
            while (! stop.load())
            {
                noise (b, r, 0.2f);
                p.processBlock (b, m);
                if (! finite (b))
                    clean = false;
            }
        });
        for (int i = 0; i < 100; ++i)
        {
            juce::MemoryBlock s;
            p.getStateInformation (s);
            p.setStateInformation (s.getData(), static_cast<int> (s.getSize()));
            juce::Thread::sleep (2);
        }
        stop = true;
        audio.join();
        check (clean.load(), "state get/set during audio processing");
    }

    // Host bypass ------------------------------------------------------------------
    if (auto* bypass = p.getBypassParameter())
    {
        setByText (p, "Mix", "50");
        setByText (p, "Feedback", "60");
        pumpMessages();
        bypass->setValueNotifyingHost (1.0f);
        pumpMessages();
        juce::AudioBuffer<float> b (2, 512);
        bool passes = true;
        for (int k = 0; k < 40; ++k)
        {
            noise (b, rng, 0.3f);
            juce::AudioBuffer<float> dry;
            dry.makeCopyOf (b);
            p.processBlock (b, midi);
            if (k > 10)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    passes = passes && std::abs (b.getSample (0, i) - dry.getSample (0, i)) < 1.0e-6f;
        }
        check (passes, "host bypass passes the dry signal");
        bypass->setValueNotifyingHost (0.0f);
    }
    else
    {
        std::printf ("[INFO] no wrapper bypass parameter for this format\n");
    }

    p.releaseResources();
    second.reset();
    plugin.reset();

    std::printf ("%s: %d failure(s)\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
