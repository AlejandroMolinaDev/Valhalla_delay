#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestHelpers.h"

using namespace oe;
using Catch::Approx;

TEST_CASE ("All parameters exist with the documented defaults", "[plugin][params]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitEchoProcessor p;

    REQUIRE (p.getParameters().size() >= static_cast<int> (params::allIds().size()));
    for (const auto& pidStr : params::allIds())
        REQUIRE (p.getState().getParameter (pidStr) != nullptr);

    REQUIRE (test::getParam (p, params::id::mix) == Approx (30.0f));
    REQUIRE (test::getParam (p, params::id::style) == 0.0f);
    REQUIRE (test::getParam (p, params::id::mode) == 0.0f);
    REQUIRE (test::getParam (p, params::id::era) == 1.0f);
    REQUIRE (test::getParam (p, params::id::delayL) == Approx (350.0f).margin (0.5f));
    REQUIRE (test::getParam (p, params::id::delayR) == Approx (500.0f).margin (0.5f));
    REQUIRE (test::getParam (p, params::id::syncL) == static_cast<float> (dsp::kSyncQuarter));
    REQUIRE (test::getParam (p, params::id::ratio) == Approx (61.8f));
    REQUIRE (test::getParam (p, params::id::feedback) == Approx (35.0f));
    REQUIRE (test::getParam (p, params::id::width) == Approx (100.0f));
    REQUIRE (test::getParam (p, params::id::age) == Approx (20.0f));
    REQUIRE (test::getParam (p, params::id::lowCut) == Approx (40.0f).margin (0.01f));
    REQUIRE (test::getParam (p, params::id::highCut) == Approx (8000.0f).margin (1.0f));
    REQUIRE (test::getParam (p, params::id::modRate) == Approx (0.5f).margin (0.001f));
    REQUIRE (test::getParam (p, params::id::freqShift) == Approx (5.0f).margin (0.01f));
    REQUIRE (test::getParam (p, params::id::pitchShift) == Approx (12.0f));
    REQUIRE (test::getParam (p, params::id::pitchDetune) == Approx (10.0f));
    REQUIRE (test::getParam (p, params::id::duck) == 0.0f);

    // Stable, versioned IDs for host automation.
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            REQUIRE (withId->getVersionHint() == params::kVersion);
}

TEST_CASE ("Visibility table follows Mode x Style", "[plugin][params]")
{
    using dsp::Mode;
    using dsp::Style;
    REQUIRE (params::isVisible (params::id::spread, Mode::Tape, Style::Single));
    REQUIRE_FALSE (params::isVisible (params::id::spread, Mode::Tape, Style::Dual));
    REQUIRE (params::isVisible (params::id::delayR, Mode::Tape, Style::PingPong));
    REQUIRE (params::isVisible (params::id::tapC, Mode::BBD, Style::Quad));
    REQUIRE (params::isVisible (params::id::wow, Mode::TapeNR, Style::Single));
    REQUIRE_FALSE (params::isVisible (params::id::wow, Mode::ReelHiFi, Style::Single));
    REQUIRE (params::isVisible (params::id::modRate, Mode::ReelHiFi, Style::Single));
    REQUIRE (params::isVisible (params::id::pitchMix, Mode::PitchRich, Style::Single));
    REQUIRE_FALSE (params::isVisible (params::id::age, Mode::PitchRich, Style::Single));
    REQUIRE (params::isVisible (params::id::res, Mode::PhaserDDL, Style::Single));
    REQUIRE_FALSE (params::isVisible (params::id::age, Mode::PhaserDDL, Style::Single));
    REQUIRE (params::isVisible (params::id::freqShift, Mode::GhostShift, Style::Ratio));
    REQUIRE (params::isVisible (params::id::duck, Mode::Digital, Style::Single));
    REQUIRE (params::duckIsInMainSection (Mode::PitchDuck));
    REQUIRE_FALSE (params::duckIsInMainSection (Mode::Tape));
}
