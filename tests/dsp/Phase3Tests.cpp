// Phase 3: Clean, Crystal, LoFi, Phaser DDL, Duck, Diffusion; spec tests 8, 9, 10.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "TestHelpers.h"
#include "dsp/core/DiffusionNetwork.h"
#include "dsp/core/Ducker.h"
#include "dsp/core/Filters.h"

using namespace oe;

namespace
{
    constexpr double kFs = 48000.0;

    test::Stereo run (const dsp::EngineParams& p, test::Stereo s, dsp::DelayEngine* engineOut = nullptr)
    {
        dsp::DelayEngine local;
        auto& e = engineOut != nullptr ? *engineOut : local;
        e.prepare (kFs);
        e.setParameters (p);
        test::render (e, s);
        return s;
    }

    test::Stereo sine (std::size_t n, double hz, float amp)
    {
        test::Stereo s (n);
        for (std::size_t i = 0; i < n; ++i)
            s.l[i] = s.r[i] = amp * static_cast<float> (std::sin (2.0 * dsp::kPi * hz * static_cast<double> (i) / kFs));
        return s;
    }

    /** Goertzel magnitude of frequency hz in v[from, to). */
    double toneMagnitude (const std::vector<float>& v, std::size_t from, std::size_t to, double hz)
    {
        const auto w = 2.0 * dsp::kPi * hz / kFs;
        const auto c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (std::size_t i = from; i < to; ++i)
        {
            const auto s0 = static_cast<double> (v[i]) + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const auto re = s1 - s2 * std::cos (w);
        const auto im = s2 * std::sin (w);
        return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (to - from);
    }

    struct Envelope
    {
        double attackMs, decayMs, continuity;
    };

    /** Attack / decay (-20 dB points) of the diffused first echo of an impulse. */
    Envelope diffusedEcho (float amount, float size)
    {
        auto p = test::cleanDigital (500.0f);
        p.diffAmount = amount;
        p.diffSize = size;
        const auto s = run (p, test::impulse (test::samplesFor (3.0, kFs)));
        const std::size_t w = 96; // 2 ms
        std::vector<double> env;
        for (std::size_t i = 0; i + w < s.size(); i += w)
            env.push_back (test::rms (s.l, i, i + w));
        std::size_t pk = 0;
        for (std::size_t i = 0; i < env.size(); ++i)
            if (env[i] > env[pk])
                pk = i;
        const auto thr = env[pk] * 0.1;
        std::size_t first = pk, last = pk;
        for (std::size_t i = 0; i <= pk; ++i)
            if (env[i] > thr)
            {
                first = i;
                break;
            }
        for (auto i = env.size() - 1; i > pk; --i)
            if (env[i] > thr)
            {
                last = i;
                break;
            }
        // Continuity: share of 1 ms windows in the response above -30 dB of the loudest one.
        std::vector<double> e1;
        for (auto i = first * w; i + 48 < (last + 1) * w; i += 48)
            e1.push_back (test::rms (s.l, i, i + 48));
        const auto mx = *std::max_element (e1.begin(), e1.end());
        const auto on = std::count_if (e1.begin(), e1.end(), [mx] (double v) { return v > mx * 0.0316; });
        return { static_cast<double> (pk - first) * 2.0, static_cast<double> (last - pk) * 2.0,
                 static_cast<double> (on) / static_cast<double> (e1.size()) };
    }
} // namespace

TEST_CASE ("[8] Drive +12 dB changes the wet RMS by less than ~3 dB (program level)", "[spec][drive]")
{
    // Program material at a typical mastered level (-9 dBFS RMS). In the linear
    // region the -Drive/2 compensation gives +6 dB for +12 dB; the mode's
    // saturation brings real program material back within +-3 dB.
    const auto mode = GENERATE (Catch::Generators::range (0, dsp::kNumModes));
    double r[2] {};
    for (int k = 0; k < 2; ++k)
    {
        dsp::EngineParams p;
        p.mode = static_cast<dsp::Mode> (mode);
        p.mix = 1.0f;
        p.feedback = 0.0f;
        p.syncL = p.syncR = dsp::kSyncFree;
        p.delayLMs = 100.0f;
        p.driveDb = k == 0 ? 0.0f : 12.0f;
        const auto s = run (p, test::whiteNoise (test::samplesFor (2.0, kFs), 0.61f));
        r[k] = test::rms (s.l, test::samplesFor (0.2, kFs));
    }
    const auto delta = test::toDb (r[1] / r[0]);
    INFO (dsp::kModeNames[static_cast<std::size_t> (mode)] << ": " << delta << " dB");
    REQUIRE (std::abs (delta) < 3.0);
}

TEST_CASE ("[10] Diffusion 0 % skips the network entirely", "[spec][diffusion]")
{
    auto p = test::cleanDigital();
    p.feedback = 0.5f;
    p.diffAmount = 0.0f;
    dsp::DelayEngine e;
    run (p, test::whiteNoise (24000, 0.3f), &e);
    REQUIRE (e.getDiffusionProcessCount() == 0);
    REQUIRE (e.getDiffusionCompensation() == 0.0);

    p.diffAmount = 0.2f;
    dsp::DelayEngine e2;
    run (p, test::whiteNoise (24000, 0.3f), &e2);
    REQUIRE (e2.getDiffusionProcessCount() == 24000);

    // Turning it back off fades out (30 ms) and then stops calling the network.
    auto s = test::whiteNoise (48000, 0.3f);
    p.diffAmount = 0.0f;
    e2.setParameters (p);
    test::render (e2, s);
    const auto count = e2.getDiffusionProcessCount();
    test::render (e2, s);
    REQUIRE (e2.getDiffusionProcessCount() == count);
}

TEST_CASE ("Diffusion 68 % gives a bell envelope (attack ~ decay), 91 % a dense reverb", "[diffusion]")
{
    for (const auto size : { 0.3f, 0.5f, 0.8f })
    {
        const auto bell = diffusedEcho (0.68f, size);
        INFO ("size " << size << " attack " << bell.attackMs << " decay " << bell.decayMs);
        REQUIRE (bell.decayMs / bell.attackMs > 0.75);
        REQUIRE (bell.decayMs / bell.attackMs < 1.4);

        const auto verb = diffusedEcho (0.91f, size);
        INFO ("91 %: attack " << verb.attackMs << " decay " << verb.decayMs << " continuity " << verb.continuity);
        REQUIRE (verb.decayMs > 3.0 * std::max (verb.attackMs, 10.0)); // fast build-up, long decay
        REQUIRE (verb.continuity > 0.9);                                // continuous, no discrete echoes
    }

    REQUIRE (dsp::DiffusionNetwork::gainForAmount (0.0) == 0.0);
    REQUIRE (dsp::DiffusionNetwork::gainForAmount (1.0) < 0.9);
}

TEST_CASE ("Diffused echoes stay centred on the delay time", "[diffusion]")
{
    for (const auto amount : { 0.05f, 0.5f, 0.9f })
    {
        auto p = test::cleanDigital (600.0f);
        p.diffAmount = amount;
        p.diffSize = 0.6f;
        const auto s = run (p, test::impulse (test::samplesFor (2.5, kFs)));
        double num = 0.0, den = 0.0;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const auto e = static_cast<double> (s.l[i]) * static_cast<double> (s.l[i]);
            num += e * static_cast<double> (i);
            den += e;
        }
        const auto centroidMs = num / den / kFs * 1000.0;
        INFO ("amount " << amount << " centroid " << centroidMs << " ms");
        // Linear interpolation inside the all-passes slightly damps the late
        // tail, so very dense settings land a few % early.
        REQUIRE (centroidMs == Catch::Approx (600.0).margin (50.0));
    }
}

TEST_CASE ("Crystal keeps the filtering out of the loop: repeats stay bright", "[crystal]")
{
    auto measure = [] (dsp::Mode mode)
    {
        auto p = test::cleanDigital (200.0f);
        p.mode = mode;
        p.feedback = 0.9f;
        p.highCutHz = 3000.0f;
        auto in = test::Stereo (test::samplesFor (1.5, kFs));
        const auto burst = test::whiteNoise (test::samplesFor (0.02, kFs), 0.3f, 5);
        std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
        const auto out = run (p, in);
        auto energyAbove = [&out] (double startS, double hz)
        {
            dsp::Svf hp;
            hp.setup (hz, 0.7071, kFs);
            const auto from = test::samplesFor (startS, kFs), to = from + test::samplesFor (0.05, kFs);
            double e = 0.0, all = 0.0;
            for (auto i = from; i < to; ++i)
            {
                const auto x = static_cast<double> (out.l[i]);
                const auto h = hp.highpass (x);
                e += h * h;
                all += x * x;
            }
            return e / all;
        };
        return std::pair { energyAbove (0.19, 2000.0), energyAbove (0.99, 2000.0) }; // echo 1 vs echo 5
    };

    const auto [d1, d5] = measure (dsp::Mode::Digital);
    const auto [c1, c5] = measure (dsp::Mode::Crystal);
    INFO ("digital " << d1 << " -> " << d5 << ", crystal " << c1 << " -> " << c5);
    REQUIRE (d5 < d1 * 0.5);                      // in-loop High Cut: every repeat darker
    REQUIRE (c5 == Catch::Approx (c1).epsilon (0.25)); // Crystal: same brightness on every repeat
}

TEST_CASE ("Crystal at Feedback 100 % sustains a loop", "[crystal]")
{
    auto p = test::cleanDigital (250.0f);
    p.mode = dsp::Mode::Crystal;
    p.feedback = 1.0f;
    p.age = 0.0f;
    auto in = test::Stereo (test::samplesFor (5.0, kFs));
    const auto burst = sine (test::samplesFor (0.1, kFs), 440.0, 0.3f);
    std::copy (burst.l.begin(), burst.l.end(), in.l.begin());
    std::copy (burst.r.begin(), burst.r.end(), in.r.begin());
    const auto out = run (p, in);
    const auto first = test::peakAbs (out.l, test::samplesFor (0.25, kFs), test::samplesFor (0.35, kFs));
    const auto later = test::peakAbs (out.l, test::samplesFor (4.75, kFs), test::samplesFor (4.85, kFs));
    REQUIRE (static_cast<double> (later) > static_cast<double> (first) * 0.8);
    REQUIRE (static_cast<double> (later) < static_cast<double> (first) * 1.2);
}

TEST_CASE ("LoFi: Age controls sample-rate reduction and aliasing", "[lofi]")
{
    auto aliasRatio = [] (float age)
    {
        auto p = test::cleanDigital (50.0f);
        p.mode = dsp::Mode::LoFi;
        p.age = age;
        p.era = dsp::Era::Past; // no anti-alias filter
        const auto out = run (p, sine (test::samplesFor (1.0, kFs), 1750.0, 0.3f));
        const auto from = test::samplesFor (0.2, kFs), to = test::samplesFor (0.9, kFs);
        const auto total = test::rms (out.l, from, to) * std::sqrt (2.0);
        const auto tone = toneMagnitude (out.l, from, to, 1750.0);
        return 1.0 - std::min (1.0, (tone * tone) / (total * total)); // share of energy NOT at 1750 Hz
    };
    const auto clean = aliasRatio (0.0f);
    const auto crushed = aliasRatio (0.8f);
    INFO ("age 0: " << clean << ", age 0.8: " << crushed);
    REQUIRE (crushed > clean + 0.2);
}

TEST_CASE ("Phaser DDL with Delay 0 and Feedback 0 is a stand-alone phaser; Era sets the stages", "[phaser]")
{
    auto countNotches = [] (dsp::Era era)
    {
        auto p = test::cleanDigital (0.0f);
        p.mode = dsp::Mode::PhaserDDL;
        p.era = era;
        p.modDepth = 0.0f; // static notches
        p.res = 0.0f;
        int notches = 0;
        double prev2 = 1.0, prev1 = 1.0;
        for (double hz = 60.0; hz < 18000.0; hz *= 1.06)
        {
            const auto out = run (p, sine (test::samplesFor (0.15, kFs), hz, 0.1f));
            const auto g = toneMagnitude (out.l, test::samplesFor (0.05, kFs), out.size(), hz) / 0.1;
            if (prev1 < prev2 && prev1 < g && prev1 < 0.2) // local minimum deeper than -14 dB
                ++notches;
            prev2 = prev1;
            prev1 = g;
        }
        return notches;
    };
    const auto past = countNotches (dsp::Era::Past);
    const auto present = countNotches (dsp::Era::Present);
    const auto future = countNotches (dsp::Era::Future);
    INFO ("notches: past " << past << ", present " << present << ", future " << future);
    REQUIRE (past == 2);    // 4 stages
    REQUIRE (present == 3); // 6 stages
    REQUIRE (future >= 5);  // 12 stages (the highest notch may sit near Nyquist)
}

TEST_CASE ("Duck lowers the wet while the input plays and lets it come back after", "[duck]")
{
    auto render = [] (float duck)
    {
        auto p = test::cleanDigital (400.0f); // typical vocal delay (~1/4 note)
        p.mode = dsp::Mode::Clean;
        p.feedback = 0.6f;
        p.duck = duck;
        auto in = test::Stereo (test::samplesFor (3.0, kFs));
        const auto tone = sine (test::samplesFor (1.5, kFs), 330.0, 0.3f);
        std::copy (tone.l.begin(), tone.l.end(), in.l.begin());
        std::copy (tone.r.begin(), tone.r.end(), in.r.begin());
        return run (p, in);
    };
    const auto off = render (0.0f);
    const auto on = render (1.0f);
    const auto during = [] (const test::Stereo& s) { return test::rms (s.l, test::samplesFor (0.8, kFs), test::samplesFor (1.4, kFs)); };
    const auto after = [] (const test::Stereo& s) { return test::rms (s.l, test::samplesFor (1.65, kFs), test::samplesFor (1.95, kFs)); };
    INFO ("during: off " << during (off) << " on " << during (on) << "; after: off " << after (off) << " on " << after (on));
    REQUIRE (test::toDb (during (on) / during (off)) < -15.0); // strongly ducked while playing
    REQUIRE (test::toDb (after (on) / during (on)) > 3.0);     // the echoes swell up once the input stops

    // Duck 0 is an exact bypass of the ducker.
    dsp::Ducker d;
    d.prepare (kFs);
    d.setAmount (0.0f);
    for (int i = 0; i < 1000; ++i)
        REQUIRE (d.process (0.9f, -0.9f) == 1.0f);
}

TEST_CASE ("Clean: Drive +24 dB through the limiter stays low in distortion", "[clean]")
{
    auto thd = [] (dsp::Mode mode)
    {
        auto p = test::cleanDigital (100.0f);
        p.mode = mode;
        p.era = dsp::Era::Present;
        p.driveDb = 24.0f;
        const auto out = run (p, sine (test::samplesFor (1.0, kFs), 500.0, 0.25f));
        const auto from = test::samplesFor (0.4, kFs), to = test::samplesFor (0.9, kFs);
        const auto f = toneMagnitude (out.l, from, to, 500.0);
        double h = 0.0;
        for (int k = 2; k <= 9; ++k)
        {
            const auto m = toneMagnitude (out.l, from, to, 500.0 * k);
            h += m * m;
        }
        return std::sqrt (h) / f;
    };
    const auto clean = thd (dsp::Mode::Clean);
    const auto digital = thd (dsp::Mode::Digital);
    const auto tape = thd (dsp::Mode::Tape);
    INFO ("THD clean " << clean << " digital " << digital << " tape " << tape);
    REQUIRE (clean < 0.03);
    REQUIRE (clean < digital * 0.3);
    REQUIRE (clean < tape * 0.3);
}
