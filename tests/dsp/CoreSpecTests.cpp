// Spec tests from the project brief (numbering follows the brief):
//  2 decay, 3 single exact echo, 4 Quad heads, 5 sync, 6 width, 7 mix law,
//  11 block-size independence, 12 sample-rate independence.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "TestHelpers.h"
#include "dsp/core/BitCrusher.h"
#include "dsp/styles/QuadStyle.h"

using namespace oe;
using Catch::Approx;

namespace
{
    constexpr double kFs = 48000.0;

    test::Stereo runImpulse (const dsp::EngineParams& p, double seconds, double fs = kFs)
    {
        dsp::DelayEngine e;
        e.prepare (fs);
        e.setParameters (p);
        auto s = test::impulse (test::samplesFor (seconds, fs));
        test::render (e, s);
        return s;
    }
} // namespace

TEST_CASE ("[2] Decay: Feedback 50 % with silent input falls below -90 dBFS", "[spec][decay]")
{
    const auto style = GENERATE (dsp::Style::Single, dsp::Style::Dual, dsp::Style::Ratio,
                                 dsp::Style::PingPong, dsp::Style::Quad);
    const auto mode = GENERATE (Catch::Generators::range (0, dsp::kNumModes));

    dsp::EngineParams p;
    p.mode = static_cast<dsp::Mode> (mode);
    p.style = style;
    p.feedback = 0.5f;
    p.mix = 1.0f;
    p.syncL = p.syncR = dsp::kSyncFree;
    p.delayLMs = 300.0f;
    p.delayRMs = 400.0f;
    p.quadFeedback = dsp::QuadFeedback::Swell;

    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);

    auto s = test::whiteNoise (test::samplesFor (14.0, kFs), 0.5f);
    std::fill (s.l.begin() + static_cast<std::ptrdiff_t> (kFs / 2), s.l.end(), 0.0f); // 0.5 s burst, then silence
    std::fill (s.r.begin() + static_cast<std::ptrdiff_t> (kFs / 2), s.r.end(), 0.0f);
    test::render (e, s);

    INFO ("mode " << dsp::kModeNames[static_cast<std::size_t> (mode)] << ", style "
                  << dsp::kStyleNames[static_cast<std::size_t> (style)]);
    REQUIRE (test::allFinite (s));
    const auto tailStart = s.size() - test::samplesFor (1.0, kFs);
    const auto tailPeak = std::max (test::peakAbs (s.l, tailStart), test::peakAbs (s.r, tailStart));
    REQUIRE (test::toDb (tailPeak) < -90.0);
}

TEST_CASE ("[3] Digital, Single, FB 0, Diffusion 0, Age 0: one echo at the exact time", "[spec][impulse]")
{
    const auto delayMs = GENERATE (1.0f, 37.5f, 250.0f, 1234.0f);
    const auto p = test::cleanDigital (delayMs);
    const auto s = runImpulse (p, static_cast<double> (delayMs) * 0.001 + 0.5);

    const auto expected = static_cast<double> (delayMs) * 0.001 * kFs; // NOLINT
    for (const auto* ch : { &s.l, &s.r })
    {
        const auto peakIndex = test::argmaxAbs (*ch);
        INFO ("delay " << delayMs << " ms, expected " << expected << ", got " << peakIndex);
        REQUIRE (std::abs (static_cast<double> (peakIndex) - expected) <= 1.0);

        // A single echo: nothing else above -40 dB outside +-2 ms of it, and
        // (almost) all of the energy concentrated around it.
        const auto peak = std::abs ((*ch)[peakIndex]);
        REQUIRE (peak > 0.25f);
        const auto guard = static_cast<std::size_t> (0.002 * kFs);
        const auto lo = peakIndex > guard ? peakIndex - guard : 0;
        const auto hi = peakIndex + guard;
        REQUIRE (test::peakAbs (*ch, 0, lo) < peak * 0.01f);
        REQUIRE (test::peakAbs (*ch, hi) < peak * 0.01f);
        const auto total = test::rms (*ch);
        const auto window = test::rms (*ch, lo, hi) * std::sqrt (static_cast<double> (hi - lo) / static_cast<double> (ch->size()));
        REQUIRE (window / total > 0.995);
    }
}

TEST_CASE ("[4] Quad, Spacing 0, Delay 500 ms: heads at 125/250/375/500 ms; Repeat != Swell", "[spec][quad]")
{
    auto p = test::cleanDigital (500.0f);
    p.style = dsp::Style::Quad;
    p.spacing = 0.0f;

    const auto s = runImpulse (p, 0.8);
    for (const double ms : { 125.0, 250.0, 375.0, 500.0 })
    {
        const auto at = static_cast<std::size_t> (ms * 0.001 * kFs);
        const auto local = test::argmaxAbs (s.l, at - 48, at + 48);
        INFO ("head at " << ms << " ms found at sample " << local);
        REQUIRE (std::abs (static_cast<double> (local) - static_cast<double> (at)) <= 1.0);
        REQUIRE (std::abs (s.l[local]) > 0.12f);
    }

    // Feedback patterns: Repeat recirculates head D only, Swell every head.
    p.feedback = 0.6f;
    p.quadFeedback = dsp::QuadFeedback::Repeat;
    const auto rep = runImpulse (p, 2.0);
    p.quadFeedback = dsp::QuadFeedback::Swell;
    const auto swl = runImpulse (p, 2.0);

    // Head A (125 ms) re-recorded and replayed by head A again lands on 250 ms: only in Swell.
    // In Repeat the first recirculation starts at 500 + 125 = 625 ms.
    const auto at250 = static_cast<std::size_t> (0.25 * kFs);
    REQUIRE (std::abs (swl.l[at250]) > std::abs (rep.l[at250]) * 1.02f);

    double diff = 0.0;
    for (std::size_t i = static_cast<std::size_t> (0.5 * kFs); i < rep.size(); ++i)
        diff += std::abs (static_cast<double> (rep.l[i]) - static_cast<double> (swl.l[i]));
    REQUIRE (diff > 1.0);
}

TEST_CASE ("[5] Tempo sync at 120 BPM", "[spec][sync]")
{
    REQUIRE (dsp::syncToMs (dsp::syncIndexOf ("1/4"), 120.0) == Approx (500.0));
    REQUIRE (dsp::syncToMs (dsp::syncIndexOf ("1/8D"), 120.0) == Approx (375.0));
    REQUIRE (dsp::syncToMs (dsp::syncIndexOf ("1/4T"), 120.0) == Approx (333.3333).epsilon (1e-4));
    REQUIRE (dsp::syncToMs (dsp::syncIndexOf ("1 bar"), 120.0) == Approx (2000.0));
    REQUIRE (dsp::syncToMs (dsp::syncIndexOf ("1/64"), 120.0) == Approx (31.25));
    REQUIRE (dsp::syncToMs (dsp::kSyncFree, 120.0) < 0.0);
    REQUIRE (dsp::kSyncQuarter == dsp::syncIndexOf ("1/4"));

    // No tempo from the host -> 120 BPM fallback.
    REQUIRE (dsp::syncToMs (dsp::kSyncQuarter, 0.0) == Approx (500.0));

    // The engine lands the echo on the synced time.
    auto p = test::cleanDigital();
    p.syncL = p.syncR = dsp::kSyncQuarter;
    p.bpm = 120.0;
    const auto s = runImpulse (p, 0.7);
    REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.l)) - 0.5 * kFs) <= 1.0);
}

TEST_CASE ("[6] Width: -100 % swaps L/R, 0 % makes L = R", "[spec][width]")
{
    auto p = test::cleanDigital();
    p.style = dsp::Style::Dual;
    p.delayLMs = 200.0f;
    p.delayRMs = 300.0f;

    p.width = 1.0f;
    const auto normal = runImpulse (p, 0.5);
    p.width = -1.0f;
    const auto swapped = runImpulse (p, 0.5);
    p.width = 0.0f;
    const auto mono = runImpulse (p, 0.5);

    REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (normal.l)) - 0.2 * kFs) <= 1.0);
    REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (normal.r)) - 0.3 * kFs) <= 1.0);

    for (std::size_t i = 0; i < normal.size(); ++i)
    {
        REQUIRE (swapped.l[i] == Approx (normal.r[i]).margin (1e-6));
        REQUIRE (swapped.r[i] == Approx (normal.l[i]).margin (1e-6));
        REQUIRE (mono.l[i] == Approx (mono.r[i]).margin (1e-7));
    }
}

TEST_CASE ("[7] Mix is equal-power: dry^2 + wet^2 = 1", "[spec][mix]")
{
    for (int i = 0; i <= 100; ++i)
    {
        const auto g = dsp::equalPowerGains (static_cast<float> (i) / 100.0f);
        REQUIRE (g.dry * g.dry + g.wet * g.wet == Approx (1.0f).margin (1e-6));
    }
    REQUIRE (dsp::equalPowerGains (0.0f).wet == Approx (0.0f).margin (1e-7));
    REQUIRE (dsp::equalPowerGains (1.0f).dry == Approx (0.0f).margin (1e-7));
    REQUIRE (dsp::equalPowerGains (0.5f).dry == Approx (std::sqrt (0.5f)));

    // Engine: Mix 0 % = dry only; the dry path is untouched.
    auto p = test::cleanDigital();
    p.mix = 0.0f;
    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);
    auto s = test::whiteNoise (4800, 0.3f);
    const auto ref = s;
    test::render (e, s);
    for (std::size_t i = 0; i < s.size(); ++i)
        REQUIRE (s.l[i] == Approx (ref.l[i]).margin (1e-6));
}

TEST_CASE ("[11] Variable block sizes give the same output as one block", "[spec][blocks]")
{
    const auto mode = GENERATE (Catch::Generators::range (0, dsp::kNumModes));
    const auto style = GENERATE (dsp::Style::Single, dsp::Style::Quad, dsp::Style::PingPong);

    dsp::EngineParams p;
    p.mode = static_cast<dsp::Mode> (mode);
    p.style = style;
    p.feedback = 0.7f;
    p.modDepth = 0.4f;
    p.diffAmount = 0.5f;
    p.duck = 0.3f;
    p.syncL = p.syncR = dsp::kSyncFree;
    p.delayLMs = 90.0f;
    p.delayRMs = 140.0f;

    auto a = test::whiteNoise (test::samplesFor (1.0, kFs), 0.4f, 99);
    auto b = a;

    dsp::DelayEngine ea, eb;
    ea.prepare (kFs);
    eb.prepare (kFs);
    ea.setParameters (p);
    eb.setParameters (p);
    test::render (ea, a, { static_cast<int> (a.size()) });
    test::render (eb, b, { 1, 7, 64, 513, 4096 });

    INFO ("mode " << dsp::kModeNames[static_cast<std::size_t> (mode)]);
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        REQUIRE (a.l[i] == b.l[i]);
        REQUIRE (a.r[i] == b.r[i]);
    }
}

TEST_CASE ("[12] Echo times are sample-rate independent (44.1 vs 96 kHz)", "[spec][samplerate]")
{
    const auto delayMs = GENERATE (125.0f, 480.0f);
    double seconds[2] {};
    int k = 0;
    for (const double fs : { 44100.0, 96000.0 })
    {
        const auto s = runImpulse (test::cleanDigital (delayMs), static_cast<double> (delayMs) * 0.001 + 0.2, fs);
        seconds[k++] = static_cast<double> (test::argmaxAbs (s.l)) / fs;
    }
    const auto expectedS = static_cast<double> (delayMs) * 0.001;
    REQUIRE (seconds[0] == Approx (expectedS).margin (1.0 / 44100.0));
    REQUIRE (seconds[1] == Approx (expectedS).margin (1.0 / 96000.0));
    REQUIRE (std::abs (seconds[0] - seconds[1]) <= 1.0 / 44100.0);
}

TEST_CASE ("Single Spread, Ratio and PingPong routing", "[styles]")
{
    SECTION ("Single: Spread adds to L and subtracts from R")
    {
        auto p = test::cleanDigital (200.0f);
        p.spreadMs = 20.0f;
        const auto s = runImpulse (p, 0.4);
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.l)) - 0.22 * kFs) <= 1.0);
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.r)) - 0.18 * kFs) <= 1.0);
    }

    SECTION ("Ratio: R = ratio x L")
    {
        auto p = test::cleanDigital (400.0f);
        p.style = dsp::Style::Ratio;
        p.ratio = 0.618f;
        const auto s = runImpulse (p, 0.6);
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.l)) - 0.4 * kFs) <= 1.0);
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.r)) - 0.4 * 0.618 * kFs) <= 1.0);
    }

    SECTION ("PingPong: L first, then R, then L...")
    {
        auto p = test::cleanDigital (100.0f);
        p.style = dsp::Style::PingPong;
        p.delayRMs = 150.0f;
        p.feedback = 0.5f;
        const auto s = runImpulse (p, 0.5);
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.l, 0, 4800 + 200)) - 4800.0) <= 1.0);
        REQUIRE (test::peakAbs (s.r, 0, 4800 + 100) < 1e-4f);                         // nothing on R before L
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.r)) - 0.25 * kFs) <= 1.0); // 100 + 150 ms
        // Third pass through the in-loop filters: allow the filter peak spread.
        REQUIRE (std::abs (static_cast<double> (test::argmaxAbs (s.l, 13000)) - 0.35 * kFs) <= 3.0); // 100 + 150 + 100 ms
    }
}

TEST_CASE ("Feedback 200 % stays bounded and finite", "[stability][quick]")
{
    const auto style = GENERATE (dsp::Style::Single, dsp::Style::Ratio, dsp::Style::Quad);
    dsp::EngineParams p;
    p.mode = dsp::Mode::Digital;
    p.style = style;
    p.feedback = 2.0f;
    p.mix = 1.0f;
    p.syncL = p.syncR = dsp::kSyncFree;
    p.delayLMs = 20.0f;

    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);
    auto s = test::whiteNoise (test::samplesFor (3.0, kFs), 0.5f);
    test::render (e, s);
    REQUIRE (test::allFinite (s));
    REQUIRE (test::peakAbs (s.l) <= 1.0f);
    REQUIRE (test::peakAbs (s.r) <= 1.0f);
}

TEST_CASE ("Digital Age 50 % uses a 12-bit floating-point converter", "[spec][age]")
{
    REQUIRE (dsp::BitCrusher::bitsForAge (0.5f) == 12);
    REQUIRE (dsp::BitCrusher::bitsForAge (0.0f) == 24);
    REQUIRE (dsp::BitCrusher::bitsForAge (1.0f) == 3);

    auto p = test::cleanDigital();
    p.age = 0.5f;
    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);
    test::Stereo s (256);
    test::render (e, s);
    REQUIRE (e.getQuantizerBits() == 12);

    // Every quantised value keeps at most 12 significant bits.
    dsp::BitCrusher c;
    c.setBits (12);
    dsp::FastRandom rng (5);
    for (int i = 0; i < 10000; ++i)
    {
        const auto x = rng.nextBipolar();
        const auto y = c.process (x);
        if (std::abs (y) < 1.0f / 32768.0f)
            continue;
        int exp = 0;
        const auto m = std::frexp (static_cast<double> (y), &exp); // |m| in [0.5, 1)
        const auto scaled = std::ldexp (std::abs (m), 12);
        REQUIRE (scaled == std::floor (scaled));
        REQUIRE (std::abs (y - x) <= std::ldexp (1.0f, exp - 12));
    }
}
