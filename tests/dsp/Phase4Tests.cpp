// Phase 4: Ghost Shift and the Pitch modes; spec test 1 (stability of every
// Mode x Style x Era at Feedback 200 %).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "TestHelpers.h"
#include "dsp/core/FrequencyShifter.h"

using namespace oe;

namespace
{
    constexpr double kFs = 48000.0;

    test::Stereo run (const dsp::EngineParams& p, test::Stereo s)
    {
        dsp::DelayEngine e;
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

    double tone (const std::vector<float>& v, std::size_t from, std::size_t to, double hz)
    {
        const auto w = 2.0 * dsp::kPi * hz / kFs;
        const auto c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto s0 = static_cast<double> (v[i]) + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const auto re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
        return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (to - from);
    }

    /** Dominant frequency between lo and hi Hz (coarse scan + refinement). */
    double dominant (const std::vector<float>& v, std::size_t from, std::size_t to, double lo, double hi)
    {
        double best = lo, bestMag = -1.0;
        for (double f = lo; f <= hi; f *= 1.005)
        {
            const auto m = tone (v, from, to, f);
            if (m > bestMag)
            {
                bestMag = m;
                best = f;
            }
        }
        return best;
    }

    dsp::EngineParams pitchBase (dsp::Mode mode, float delayMs)
    {
        auto p = test::cleanDigital (delayMs);
        p.mode = mode;
        p.era = dsp::Era::Future;
        p.age = 0.0f;
        p.pitchDetuneCents = 0.0f;
        return p;
    }
} // namespace

TEST_CASE ("Frequency shifter: the Hilbert pair is in quadrature, the shift is linear in Hz", "[ghost][freqshift]")
{
    dsp::FrequencyShifter fsh;
    fsh.prepare (kFs);
    for (const double hz : { 100.0, 1000.0, 8000.0 })
    {
        fsh.reset();
        // Correlate I and Q over whole periods: quadrature -> correlation ~ 0, equal power.
        double ii = 0.0, qq = 0.0, iq = 0.0;
        const auto n = static_cast<int> (kFs);
        for (int k = 0; k < n; ++k)
        {
            const auto r = fsh.hilbert (0, std::sin (2.0 * dsp::kPi * hz * k / kFs));
            if (k > n / 4)
            {
                ii += r.i * r.i;
                qq += r.q * r.q;
                iq += r.i * r.q;
            }
        }
        INFO (hz << " Hz: corr " << iq / std::sqrt (ii * qq) << " power ratio " << qq / ii);
        REQUIRE (std::abs (iq / std::sqrt (ii * qq)) < 0.02);
        REQUIRE (qq / ii == Catch::Approx (1.0).epsilon (0.02));
    }

    // Through the plugin: a 1 kHz tone shifted by +150 Hz comes out at 1150 Hz (not 1000 x ratio).
    auto p = test::cleanDigital (50.0f);
    p.mode = dsp::Mode::GhostShift;
    p.era = dsp::Era::Past; // no dispersion
    p.age = 0.0f;
    p.freqShiftHz = 150.0f;
    p.freqDetuneHz = 0.0f;
    const auto up = run (p, sine (test::samplesFor (1.0, kFs), 1000.0, 0.2f));
    const auto from = test::samplesFor (0.3, kFs), to = test::samplesFor (0.9, kFs);
    REQUIRE (tone (up.l, from, to, 1150.0) > 20.0 * tone (up.l, from, to, 850.0)); // single sideband
    REQUIRE (dominant (up.l, from, to, 900.0, 1400.0) == Catch::Approx (1150.0).epsilon (0.01));

    p.freqShiftHz = -300.0f;
    const auto down = run (p, sine (test::samplesFor (1.0, kFs), 1000.0, 0.2f));
    REQUIRE (dominant (down.l, from, to, 500.0, 1200.0) == Catch::Approx (700.0).epsilon (0.01));

    // Detune: L and R are shifted by +-detune/2.
    p.freqShiftHz = 100.0f;
    p.freqDetuneHz = 40.0f;
    const auto det = run (p, sine (test::samplesFor (1.0, kFs), 1000.0, 0.2f));
    REQUIRE (dominant (det.l, from, to, 1000.0, 1250.0) == Catch::Approx (1120.0).epsilon (0.01));
    REQUIRE (dominant (det.r, from, to, 1000.0, 1250.0) == Catch::Approx (1080.0).epsilon (0.01));
}

TEST_CASE ("Ghost Shift Era: Present and Future add frequency-dependent dispersion (highs first)", "[ghost]")
{
    // Arrival time of a band = energy centroid of the echo of an impulse, band-passed.
    auto arrival = [] (dsp::Era era, double hz)
    {
        auto p = test::cleanDigital (300.0f);
        p.mode = dsp::Mode::GhostShift;
        p.era = era;
        p.age = 0.0f;
        p.freqShiftHz = 0.0f;
        p.freqDetuneHz = 0.0f;
        p.diffAmount = 0.6f;
        p.diffSize = 0.01f;
        const auto s = run (p, test::impulse (test::samplesFor (0.6, kFs)));
        dsp::Svf bp;
        bp.setup (hz, 4.0, kFs);
        double num = 0.0, den = 0.0;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const auto y = bp.bandpass (static_cast<double> (s.l[i]));
            num += y * y * static_cast<double> (i);
            den += y * y;
        }
        return num / den / kFs * 1000.0; // ms
    };

    const auto spreadPast = arrival (dsp::Era::Past, 200.0) - arrival (dsp::Era::Past, 6000.0);
    const auto spreadPresent = arrival (dsp::Era::Present, 200.0) - arrival (dsp::Era::Present, 6000.0);
    const auto spreadFuture = arrival (dsp::Era::Future, 200.0) - arrival (dsp::Era::Future, 6000.0);
    // Past has no extra dispersion: its "spread" is only the measuring band-pass's own delay.
    const auto extraPresent = spreadPresent - spreadPast;
    const auto extraFuture = spreadFuture - spreadPast;
    INFO ("extra low-vs-high delay: present " << extraPresent << " ms, future " << extraFuture << " ms");
    REQUIRE (extraPresent > 1.0);
    REQUIRE (extraFuture > 2.5 * extraPresent);
}

TEST_CASE ("Pitch: the first echo is shifted by the set interval; repeats keep shifting", "[pitch]")
{
    for (const auto st : { 12.0f, 7.0f, -12.0f })
    {
        auto p = pitchBase (dsp::Mode::Pitch, 200.0f);
        p.pitchShiftSt = st;
        const auto in = sine (test::samplesFor (1.2, kFs), 440.0, 0.2f);
        const auto out = run (p, in);
        const auto expected = 440.0 * std::exp2 (static_cast<double> (st) / 12.0);
        const auto got = dominant (out.l, test::samplesFor (0.4, kFs), test::samplesFor (1.1, kFs), 150.0, 2000.0);
        INFO (st << " st: expected " << expected << " got " << got);
        // Grain crossfades add small AM sidebands (~ the grain rate): 2 % tolerance.
        REQUIRE (got == Catch::Approx (expected).epsilon (0.02));
    }

    // Spiral: with feedback, the second repeat is shifted twice (+24 st).
    auto p = pitchBase (dsp::Mode::Pitch, 200.0f);
    p.pitchShiftSt = 12.0f;
    p.feedback = 0.8f;
    auto in = sine (test::samplesFor (1.0, kFs), 300.0, 0.2f);
    std::fill (in.l.begin() + static_cast<std::ptrdiff_t> (test::samplesFor (0.15, kFs)), in.l.end(), 0.0f); // 150 ms burst
    std::fill (in.r.begin() + static_cast<std::ptrdiff_t> (test::samplesFor (0.15, kFs)), in.r.end(), 0.0f);
    const auto out = run (p, in);
    const auto second = dominant (out.l, test::samplesFor (0.42, kFs), test::samplesFor (0.53, kFs), 300.0, 2000.0);
    REQUIRE (second == Catch::Approx (1200.0).epsilon (0.02));
}

TEST_CASE ("Pitch Detune: L and R are detuned in opposite directions (doubler)", "[pitch]")
{
    auto p = pitchBase (dsp::Mode::Pitch, 40.0f);
    p.pitchShiftSt = 0.0f;
    p.pitchDetuneCents = 40.0f; // L +20 ct, R -20 ct
    const auto out = run (p, sine (test::samplesFor (1.5, kFs), 1000.0, 0.2f));
    const auto from = test::samplesFor (0.3, kFs), to = test::samplesFor (1.4, kFs);
    REQUIRE (dominant (out.l, from, to, 950.0, 1050.0) == Catch::Approx (1000.0 * std::exp2 (20.0 / 1200.0)).epsilon (0.002));
    REQUIRE (dominant (out.r, from, to, 950.0, 1050.0) == Catch::Approx (1000.0 * std::exp2 (-20.0 / 1200.0)).epsilon (0.002));
}

TEST_CASE ("Pitch echoes stay on time (no added latency)", "[pitch]")
{
    auto p = pitchBase (dsp::Mode::Pitch, 400.0f);
    p.pitchShiftSt = 0.0f; // unity ratio: grains reproduce the input
    const auto out = run (p, test::impulse (test::samplesFor (0.8, kFs)));
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const auto e = static_cast<double> (out.l[i]) * static_cast<double> (out.l[i]);
        num += e * static_cast<double> (i);
        den += e;
    }
    REQUIRE (num / den / kFs == Catch::Approx (0.4).margin (0.002));
}

TEST_CASE ("Pitch Rich: Pitch Mix balances detuned (0 %) and shifted (100 %) paths", "[pitch]")
{
    auto measure = [] (float mix)
    {
        auto p = pitchBase (dsp::Mode::PitchRich, 150.0f);
        p.pitchShiftSt = 12.0f;
        p.pitchMix = mix;
        const auto out = run (p, sine (test::samplesFor (1.2, kFs), 500.0, 0.2f));
        const auto from = test::samplesFor (0.3, kFs), to = test::samplesFor (1.1, kFs);
        return std::pair { tone (out.l, from, to, 500.0), tone (out.l, from, to, 1000.0) };
    };
    const auto [lush0, shifted0] = measure (0.0f);
    const auto [lush1, shifted1] = measure (1.0f);
    const auto [lushH, shiftedH] = measure (0.5f);
    REQUIRE (lush0 > 10.0 * shifted0);
    REQUIRE (shifted1 > 10.0 * lush1);
    REQUIRE (lushH == Catch::Approx (shiftedH).epsilon (0.35)); // equal power at 50 %
}

TEST_CASE ("Pitch Reverse plays splices backwards (splice = Delay)", "[pitch][reverse]")
{
    // A rising chirp comes back falling.
    auto p = pitchBase (dsp::Mode::PitchReverse, 400.0f);
    p.pitchShiftSt = 0.0f;
    const auto n = test::samplesFor (2.0, kFs);
    test::Stereo in (n);
    double ph = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto t = static_cast<double> (i % test::samplesFor (0.4, kFs)) / kFs; // 400 ms rising sweeps
        ph += 2.0 * dsp::kPi * (300.0 + 3000.0 * t) / kFs;
        in.l[i] = in.r[i] = 0.2f * static_cast<float> (std::sin (ph));
    }
    const auto out = run (p, in);

    // Track the instantaneous frequency over one output splice: it must fall.
    auto freqAt = [&out] (double startS)
    {
        const auto from = test::samplesFor (startS, kFs);
        return dominant (out.l, from, from + test::samplesFor (0.03, kFs), 250.0, 1800.0);
    };
    int falling = 0, rising = 0;
    for (double t = 1.0; t < 1.8; t += 0.04)
    {
        const auto a = freqAt (t), b = freqAt (t + 0.04);
        if (b < a * 0.97)
            ++falling;
        else if (b > a * 1.03)
            ++rising;
    }
    INFO ("falling " << falling << " rising " << rising);
    REQUIRE (falling > rising * 2);
}

TEST_CASE ("[1] Stability: every Mode x Style x Era at Feedback 200 %, 10 s of white noise", "[spec][stability][slow]")
{
    const auto mode = GENERATE (Catch::Generators::range (0, dsp::kNumModes));
    const auto style = GENERATE (Catch::Generators::range (0, dsp::kNumStyles));
    const auto era = GENERATE (Catch::Generators::range (0, dsp::kNumEras));

    dsp::EngineParams p; // defaults otherwise (Age 20 %, Mod 10 %, Diffusion 0 %)
    p.mode = static_cast<dsp::Mode> (mode);
    p.style = static_cast<dsp::Style> (style);
    p.era = static_cast<dsp::Era> (era);
    p.feedback = 2.0f;
    p.mix = 1.0f;

    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);
    auto s = test::whiteNoise (test::samplesFor (10.0, kFs), 0.5f, static_cast<std::uint32_t> (1 + mode * 97 + style * 13 + era));
    test::render (e, s);

    INFO (dsp::kModeNames[static_cast<std::size_t> (mode)] << " / " << dsp::kStyleNames[static_cast<std::size_t> (style)]
                                                           << " / " << dsp::kEraNames[static_cast<std::size_t> (era)]);
    REQUIRE (test::allFinite (s));
    REQUIRE (test::peakAbs (s.l) <= 1.0f); // <= 0 dBFS
    REQUIRE (test::peakAbs (s.r) <= 1.0f);
    REQUIRE (e.getResetCount() == 1);      // never needed the NaN recovery
}
