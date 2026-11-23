// Editor: creation, scaling with a fixed aspect ratio, Mode x Style visibility,
// and PNG snapshots (written when OE_SNAPSHOT_DIR is set) for visual review.

#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginTestHelpers.h"

using namespace oe;
namespace id = oe::params::id;

namespace
{
    void snapshot (juce::Component& c, const juce::String& name)
    {
        const auto dir = juce::SystemStats::getEnvironmentVariable ("OE_SNAPSHOT_DIR", {});
        if (dir.isEmpty())
            return;
        const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        juce::File file (juce::File (dir).getChildFile (name + ".png"));
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
} // namespace

TEST_CASE ("Editor opens, scales 75-200 % with a fixed aspect ratio and remembers its size", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditorAndMakeActive());
        REQUIRE (ed != nullptr);
        REQUIRE (ed->getWidth() == 1120);
        REQUIRE (ed->getHeight() == 448);
        REQUIRE (ed->isResizable());

        for (const auto scale : { 0.75, 1.0, 1.5, 2.0 })
        {
            const auto w = juce::roundToInt (1120 * scale);
            ed->setSize (w, juce::roundToInt (448 * scale));
            REQUIRE (ed->getWidth() == w);
        }
        auto* c = ed->getConstrainer();
        REQUIRE (c != nullptr);
        REQUIRE (c->getFixedAspectRatio() == 1120.0 / 448.0);
        REQUIRE (c->getMinimumWidth() == 840);
        REQUIRE (c->getMaximumWidth() == 2240);

        ed->setSize (1680, 672);
    }
    REQUIRE (p.editorWidth.load() == 1680);

    // Saved in the plugin state and restored.
    juce::MemoryBlock block;
    p.getStateInformation (block);
    OrbitEchoProcessor q;
    q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    REQUIRE (q.editorWidth.load() == 1680);
    std::unique_ptr<juce::AudioProcessorEditor> ed2 (q.createEditorAndMakeActive());
    REQUIRE (ed2->getWidth() == 1680);
}

TEST_CASE ("Editor shows only the controls of the current Mode x Style", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    auto* raw = p.createEditorAndMakeActive();
    std::unique_ptr<juce::AudioProcessorEditor> holder (raw);
    auto& ed = dynamic_cast<OrbitEchoEditor&> (*raw);

    auto set = [&] (dsp::Mode m, dsp::Style s)
    {
        test::setParam (p, id::mode, static_cast<float> (m));
        test::setParam (p, id::style, static_cast<float> (s));
        ed.updateVisibility (false);
    };

    for (int m = 0; m < dsp::kNumModes; ++m)
        for (int s = 0; s < dsp::kNumStyles; ++s)
        {
            set (static_cast<dsp::Mode> (m), static_cast<dsp::Style> (s));
            for (const auto& pid : params::allIds())
            {
                if (pid == id::delayL || pid == id::syncL || pid == id::delayR || pid == id::syncR)
                    continue; // one of the ms / note knobs is shown depending on Sync
                INFO (dsp::kModeNames[static_cast<std::size_t> (m)] << " / " << dsp::kStyleNames[static_cast<std::size_t> (s)] << " / " << pid);
                REQUIRE (ed.isControlShown (pid) == params::isVisible (pid, static_cast<dsp::Mode> (m), static_cast<dsp::Style> (s)));
            }
        }

    // Sync: the note knob replaces the ms knob.
    set (dsp::Mode::Tape, dsp::Style::Dual);
    test::setParam (p, id::syncL, 0.0f);
    test::setParam (p, id::syncR, static_cast<float> (dsp::kSyncQuarter));
    ed.updateVisibility (false);
    REQUIRE (ed.isControlShown (id::delayL));
    REQUIRE_FALSE (ed.isControlShown (id::syncL));
    REQUIRE (ed.isControlShown (id::syncR));
    REQUIRE_FALSE (ed.isControlShown (id::delayR));

}

TEST_CASE ("Editor snapshots for visual review", "[ui][snapshot]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;
    test::prepare (p, 48000.0, 512);
    auto* raw = p.createEditorAndMakeActive();
    std::unique_ptr<juce::AudioProcessorEditor> holder (raw);
    auto& ed = dynamic_cast<OrbitEchoEditor&> (*raw);

    struct Shot { const char* name; dsp::Mode mode; dsp::Style style; float diffusion; };
    const Shot shots[] { { "tape_single", dsp::Mode::Tape, dsp::Style::Single, 0.0f },
                         { "bbd_pingpong", dsp::Mode::BBDBright, dsp::Style::PingPong, 0.0f },
                         { "quad_swell", dsp::Mode::Tape, dsp::Style::Quad, 0.3f },
                         { "pitchrich_ratio", dsp::Mode::PitchRich, dsp::Style::Ratio, 0.8f },
                         { "ghost_dual", dsp::Mode::GhostShift, dsp::Style::Dual, 0.5f } };
    for (const auto& s : shots)
    {
        test::setParam (p, id::mode, static_cast<float> (s.mode));
        test::setParam (p, id::style, static_cast<float> (s.style));
        test::setParam (p, id::diffAmount, s.diffusion * 100.0f);
        test::setParam (p, id::feedback, 55.0f);
        // Run audio so the visualiser receives frames.
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 40; ++i)
        {
            b.clear();
            b.setSample (0, 0, 0.5f);
            p.processBlock (b, midi);
        }
        ed.updateVisibility (false);
        for (int i = 0; i < 5; ++i)
        {
            juce::Thread::sleep (17);
            juce::Timer::callPendingTimersSynchronously(); // let the visualiser pick up frames
        }
        snapshot (ed, s.name);
    }
    ed.setSize (840, 336);
    snapshot (ed, "scale75");
    SUCCEED();
}
