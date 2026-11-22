// Spec test 13 (state save / restore, hidden values kept across Mode / Style
// changes) + factory presets, Mix Lock, A/B and user presets.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestHelpers.h"
#include "PresetManager.h"

using namespace oe;
using Catch::Approx;
namespace id = oe::params::id;

namespace
{
    std::map<juce::String, float> snapshotValues (OrbitEchoProcessor& p)
    {
        std::map<juce::String, float> m;
        for (const auto& pid : params::allIds())
            m[pid] = test::getParam (p, pid.toRawUTF8());
        return m;
    }
} // namespace

TEST_CASE ("[13] State save/restore and hidden values kept across Mode / Style changes", "[spec][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;

    // Non-default values everywhere, including controls the current mode hides.
    test::setParam (p, id::mode, static_cast<float> (dsp::Mode::Digital));
    test::setParam (p, id::style, static_cast<float> (dsp::Style::Single));
    test::setParam (p, id::wow, 73.0f);          // hidden in Digital
    test::setParam (p, id::pitchShift, -7.0f);   // hidden in Digital
    test::setParam (p, id::freqShift, 123.45f);  // hidden in Digital
    test::setParam (p, id::res, 81.0f);          // hidden in Digital
    test::setParam (p, id::ratio, 33.3f);        // hidden in Single
    test::setParam (p, id::spacing, 44.0f);      // hidden in Single
    test::setParam (p, id::tapC, 0.0f);          // hidden in Single
    test::setParam (p, id::delayR, 1234.0f);     // hidden in Single
    test::setParam (p, id::syncR, 12.0f);
    test::setParam (p, id::feedback, 120.0f);
    test::setParam (p, id::lowCut, 222.0f);
    test::setParam (p, id::duck, 37.0f);
    const auto before = snapshotValues (p);

    // Walk through every Mode and Style: nothing but mode/style may change.
    for (int m = 0; m < dsp::kNumModes; ++m)
        for (int s = 0; s < dsp::kNumStyles; ++s)
        {
            test::setParam (p, id::mode, static_cast<float> (m));
            test::setParam (p, id::style, static_cast<float> (s));
        }
    test::setParam (p, id::mode, static_cast<float> (dsp::Mode::Digital));
    test::setParam (p, id::style, static_cast<float> (dsp::Style::Single));
    const auto after = snapshotValues (p);
    for (const auto& [pid, value] : before)
    {
        INFO (pid);
        REQUIRE (after.at (pid) == Approx (value).margin (1e-4));
    }

    // Save -> restore into a fresh instance (as a DAW does when reopening a project).
    p.mixLock = true;
    p.editorWidth = 1125;
    juce::MemoryBlock block;
    p.getStateInformation (block);

    OrbitEchoProcessor q;
    q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    const auto restored = snapshotValues (q);
    for (const auto& [pid, value] : before)
    {
        INFO (pid);
        REQUIRE (restored.at (pid) == Approx (value).margin (1e-4));
    }
    REQUIRE (q.mixLock.load());
    REQUIRE (q.editorWidth.load() == 1125);

    // Garbage / foreign state is ignored without crashing.
    const char junk[] = "not a valid state";
    q.setStateInformation (junk, static_cast<int> (sizeof (junk)));
    q.setStateInformation (nullptr, 0);
    REQUIRE (snapshotValues (q).at (id::wow) == Approx (73.0f));
}

TEST_CASE ("Factory presets: at least 25, named, categorised, loadable", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    auto& pm = p.getPresetManager();

    int factory = 0;
    juce::StringArray names;
    for (int i = 0; i < pm.getNumPresets(); ++i)
        if (pm.getPreset (i).isFactory)
        {
            ++factory;
            names.addIfNotAlreadyThere (pm.getPreset (i).name);
        }
    REQUIRE (factory >= 25);
    REQUIRE (names.size() == factory); // unique names
    REQUIRE (pm.getCategories().size() >= 6);

    for (int i = 0; i < pm.getNumPresets(); ++i)
    {
        if (! pm.getPreset (i).isFactory)
            continue;
        INFO (pm.getPreset (i).name);
        REQUIRE (pm.loadPreset (i));
        REQUIRE (pm.getCurrentName() == pm.getPreset (i).name);
        // Every value lands inside its range, and the preset plays without problems.
        for (auto* param : p.getParameters())
        {
            const auto v = param->getValue();
            REQUIRE (v >= 0.0f);
            REQUIRE (v <= 1.0f);
        }
        test::prepare (p, 48000.0, 256);
        juce::AudioBuffer<float> b (2, 4800);
        juce::Random rng (static_cast<juce::int64> (i));
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < b.getNumSamples(); ++s)
                b.setSample (ch, s, 0.3f * (rng.nextFloat() - 0.5f));
        test::processInChunks (p, b, { 256 });
        REQUIRE (test::isFinite (b));
    }

    // Spot checks against the brief's list.
    auto find = [&pm] (const juce::String& name)
    {
        for (int i = 0; i < pm.getNumPresets(); ++i)
            if (pm.getPreset (i).name == name)
                return i;
        return -1;
    };
    REQUIRE (pm.loadPreset (find ("Golden Bloom")));
    REQUIRE (test::getParam (p, id::style) == static_cast<float> (dsp::Style::Ratio));
    REQUIRE (test::getParam (p, id::ratio) == Approx (61.8f));
    REQUIRE (pm.loadPreset (find ("Diffuse Hall")));
    REQUIRE (test::getParam (p, id::diffAmount) == Approx (91.0f));
    REQUIRE (test::getParam (p, id::feedback) == Approx (0.0f));
    REQUIRE (pm.loadPreset (find ("Phase Only")));
    REQUIRE (test::getParam (p, id::delayL) == Approx (0.0f));
    REQUIRE (test::getParam (p, id::mode) == static_cast<float> (dsp::Mode::PhaserDDL));
}

TEST_CASE ("Mix Lock keeps the Mix when loading presets", "[presets][mixlock]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    auto& pm = p.getPresetManager();

    test::setParam (p, id::mix, 77.0f);
    p.mixLock = true;
    REQUIRE (pm.loadPreset (0));
    REQUIRE (test::getParam (p, id::mix) == Approx (77.0f));

    p.mixLock = false;
    REQUIRE (pm.loadPreset (0));
    REQUIRE (test::getParam (p, id::mix) != Approx (77.0f));
}

TEST_CASE ("A/B comparison swaps two complete settings", "[presets][ab]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    auto& pm = p.getPresetManager();

    test::setParam (p, id::feedback, 20.0f);
    pm.selectB();                     // B starts as a copy of A
    REQUIRE (pm.isOnB());
    REQUIRE (test::getParam (p, id::feedback) == Approx (20.0f));
    test::setParam (p, id::feedback, 90.0f);
    test::setParam (p, id::mode, 9.0f);

    pm.selectA();
    REQUIRE (test::getParam (p, id::feedback) == Approx (20.0f));
    REQUIRE (test::getParam (p, id::mode) == 0.0f);

    pm.selectB();
    REQUIRE (test::getParam (p, id::feedback) == Approx (90.0f));
    REQUIRE (test::getParam (p, id::mode) == 9.0f);
}

TEST_CASE ("User presets: save, rescan, load", "[presets][user]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    auto& pm = p.getPresetManager();

    const auto name = "zz Orbit Echo test " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    test::setParam (p, id::feedback, 66.0f);
    test::setParam (p, id::mode, 4.0f);
    if (! pm.saveUserPreset (name))
    {
        WARN ("user preset folder not writable here; skipping");
        return;
    }
    const auto file = PresetManager::getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".xml");
    REQUIRE (file.existsAsFile());
    REQUIRE (pm.getCurrentName() == name);

    test::setParam (p, id::feedback, 10.0f);
    int index = -1;
    for (int i = 0; i < pm.getNumPresets(); ++i)
        if (pm.getPreset (i).name == name)
            index = i;
    REQUIRE (index >= 0);
    REQUIRE (pm.getPreset (index).category == "User");
    REQUIRE (pm.loadPreset (index));
    REQUIRE (test::getParam (p, id::feedback) == Approx (66.0f));
    REQUIRE (test::getParam (p, id::mode) == 4.0f);

    file.deleteFile();
    pm.rescanUserPresets();
}
