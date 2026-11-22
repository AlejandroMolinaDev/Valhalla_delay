// Phase 2: tape and BBD families.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "TestHelpers.h"
#include "dsp/core/Filters.h"

using namespace oe;

namespace
{
    constexpr double kFs = 48000.0;

    dsp::EngineParams base (dsp::Mode mode, float delayMs)
    {
        auto p = test::cleanDigital (delayMs);
        p.mode = mode;
        p.era = dsp::Era::Present;
        p.wow = 0.0f;
        p.flutter = 0.0f;
        p.modDepth = 0.0f;
        p.age = 0.0f;
        return p;
    }

    test::Stereo render (const dsp::EngineParams& p, const test::Stereo& input)
    {
        dsp::DelayEngine e;
        e.prepare (kFs);
        e.setParameters (p);
        auto s = input;
        test::render (e, s);
        return s;
    }

    /** Fraction of the energy above `hz` in v[from, to). */
    double highFrequencyRatio (const std::vector<float>& v, std::size_t from, std::size_t to, double hz)
    {
        dsp::Svf hp;
        hp.setup (hz, 0.7071, kFs);
        double hi = 0.0, all = 0.0;
        for (std::size_t i = from; i < to; ++i)
        {
            const auto x = static_cast<double> (v[i]);
            const auto h = hp.highpass (x);
            hi += h * h;
            all += x * x;
        }
        return all > 0.0 ? hi / all : 0.0;
    }

    /** Echo of a white-noise burst: HF ratio of the first echo. */
    double echoBrightness (dsp::Mode mode, float delayMs, dsp::Era era = dsp::Era::Present)
    {
        auto p = base (mode, delayMs);
        p.era = era;
        const auto burst = test::samplesFor (0.05, kFs);
        auto in = test::Stereo (test::samplesFor (static_cast<double> (delayMs) * 0.001 + 0.2, kFs));
        const auto noise = test::whiteNoise (burst, 0.2f, 77);
        std::copy (noise.l.begin(), noise.l.end(), in.l.begin());
        std::copy (noise.r.begin(), noise.r.end(), in.r.begin());
        const auto out = render (p, in);
        const auto start = test::samplesFor (static_cast<double> (delayMs) * 0.001, kFs);
        return highFrequencyRatio (out.l, start, start + burst, 3000.0);
    }
} // namespace

TEST_CASE ("Tape EQ follows the delay time (slower tape = darker)", "[tape]")
{
    for (const auto mode : { dsp::Mode::Tape, dsp::Mode::TapeClean, dsp::Mode::TapeNR })
    {
        const auto shortDelay = echoBrightness (mode, 80.0f);
        const auto longDelay = echoBrightness (mode, 900.0f);
        INFO (dsp::kModeNames[static_cast<std::size_t> (mode)] << " short " << shortDelay << " long " << longDelay);
        REQUIRE (shortDelay > longDelay * 1.5);
    }

    // Reel HiFi is a fixed-speed machine: its bandwidth does not depend on the delay.
    const auto a = echoBrightness (dsp::Mode::ReelHiFi, 80.0f);
    const auto b = echoBrightness (dsp::Mode::ReelHiFi, 900.0f);
    REQUIRE (a == Catch::Approx (b).epsilon (0.15));
}

TEST_CASE ("Tape Clean and Tape NR are brighter than Tape; Era Future brighter than Past", "[tape]")
{
    const auto tape = echoBrightness (dsp::Mode::Tape, 300.0f);
    REQUIRE (echoBrightness (dsp::Mode::TapeClean, 300.0f) > tape);
    REQUIRE (echoBrightness (dsp::Mode::TapeNR, 300.0f) > tape);
    REQUIRE (echoBrightness (dsp::Mode::Tape, 300.0f, dsp::Era::Future)
             > echoBrightness (dsp::Mode::Tape, 300.0f, dsp::Era::Past));
}

TEST_CASE ("BBD family bandwidth: BBD dark, Analog+ ~2x, BBD Bright follows the clock", "[bbd]")
{
    const auto bbd = echoBrightness (dsp::Mode::BBD, 300.0f);
    const auto analog = echoBrightness (dsp::Mode::AnalogPlus, 300.0f);
    const auto digital = echoBrightness (dsp::Mode::Digital, 300.0f);
    INFO ("bbd " << bbd << " analog+ " << analog << " digital " << digital);
    REQUIRE (analog > bbd * 1.5);
    REQUIRE (digital > analog); // brighter than a real BBD, darker than digital

    const auto brightShort = echoBrightness (dsp::Mode::BBDBright, 40.0f);
    const auto brightLong = echoBrightness (dsp::Mode::BBDBright, 800.0f);
    REQUIRE (brightShort > brightLong * 2.0);

    // BBD Future = bright model.
    REQUIRE (echoBrightness (dsp::Mode::BBD, 300.0f, dsp::Era::Future) > bbd * 1.5);
}

TEST_CASE ("Age noise follows the signal: present under the echoes, gone in silence", "[tape][bbd][age]")
{
    const auto mode = GENERATE (dsp::Mode::Tape, dsp::Mode::BBD, dsp::Mode::AnalogPlus, dsp::Mode::ReelHiFi);

    auto p = base (mode, 200.0f);
    p.feedback = 0.3f;
    auto in = test::Stereo (test::samplesFor (6.0, kFs));
    for (std::size_t i = 0; i < test::samplesFor (1.0, kFs); ++i) // 1 s of 300 Hz tone
        in.l[i] = in.r[i] = 0.3f * static_cast<float> (std::sin (2.0 * dsp::kPi * 300.0 * static_cast<double> (i) / kFs));

    p.age = 0.0f;
    const auto clean = render (p, in);
    p.age = 1.0f;
    const auto aged = render (p, in);

    // While the tone plays, Age adds noise under the echoes: the difference
    // between the aged and the clean render (same seeds) is that noise.
    const auto from = test::samplesFor (0.4, kFs), to = test::samplesFor (0.9, kFs);
    double noiseEnergy = 0.0;
    for (auto i = from; i < to; ++i)
    {
        const auto d = static_cast<double> (aged.l[i]) - static_cast<double> (clean.l[i]);
        noiseEnergy += d * d;
    }
    const auto noiseRms = std::sqrt (noiseEnergy / static_cast<double> (to - from));
    INFO (dsp::kModeNames[static_cast<std::size_t> (mode)] << " noise rms " << test::toDb (noiseRms) << " dBFS");
    REQUIRE (noiseRms > 3.0e-4); // > -70 dBFS at Age 100 %

    // Long after the input stops, nothing is left (no idle hiss: FL Studio's Smart Disable can sleep).
    REQUIRE (test::peakAbs (aged.l, test::samplesFor (5.0, kFs)) < 3.0e-5f);
}

TEST_CASE ("Splices: periodic artefacts in Tape only, scaled by Age", "[tape][splice]")
{
    auto measureDips = [] (dsp::Mode mode, float age)
    {
        auto p = base (mode, 250.0f);
        p.age = age;
        p.feedback = 0.0f;
        auto in = test::Stereo (test::samplesFor (5.0, kFs));
        for (std::size_t i = 0; i < in.size(); ++i) // steady tone through the loop
            in.l[i] = in.r[i] = 0.25f * static_cast<float> (std::sin (2.0 * dsp::kPi * 220.0 * static_cast<double> (i) / kFs));
        const auto out = render (p, in);

        // Envelope in 5 ms windows; a splice is a window clearly below the median level.
        std::vector<double> env;
        const auto w = test::samplesFor (0.005, kFs);
        for (std::size_t i = test::samplesFor (0.5, kFs); i + w < out.size(); i += w)
            env.push_back (test::rms (out.l, i, i + w));
        auto sorted = env;
        std::sort (sorted.begin(), sorted.end());
        const auto median = sorted[sorted.size() / 2];
        int dips = 0;
        for (const auto e : env)
            if (e < 0.85 * median)
                ++dips;
        return dips;
    };

    REQUIRE (measureDips (dsp::Mode::Tape, 1.0f) >= 2);     // loop period 8 x 250 ms = 2 s
    REQUIRE (measureDips (dsp::Mode::Tape, 0.0f) == 0);
    REQUIRE (measureDips (dsp::Mode::TapeClean, 1.0f) == 0);
    REQUIRE (measureDips (dsp::Mode::ReelHiFi, 1.0f) == 0);
}

TEST_CASE ("Wow and flutter move the echo pitch; rates follow the delay time", "[tape][wow]")
{
    // Measure the read-position deviation through the zero-crossing rate of a
    // steady tone: with wow/flutter at 100 % the echo period wanders.
    auto pitchWander = [] (dsp::Mode mode, float wow, float flutter)
    {
        auto p = base (mode, 300.0f);
        p.wow = wow;
        p.flutter = flutter;
        auto in = test::Stereo (test::samplesFor (4.0, kFs));
        for (std::size_t i = 0; i < in.size(); ++i)
            in.l[i] = in.r[i] = 0.2f * static_cast<float> (std::sin (2.0 * dsp::kPi * 1000.0 * static_cast<double> (i) / kFs));
        const auto out = render (p, in);
        std::vector<double> periods;
        double last = -1.0;
        for (std::size_t i = test::samplesFor (1.0, kFs); i + 1 < out.size(); ++i)
            if (out.l[i] <= 0.0f && out.l[i + 1] > 0.0f)
            {
                const auto t = static_cast<double> (i) + static_cast<double> (out.l[i] / (out.l[i] - out.l[i + 1]));
                if (last > 0.0)
                    periods.push_back (t - last);
                last = t;
            }
        double mn = 1e9, mx = 0.0;
        for (const auto x : periods)
        {
            mn = std::min (mn, x);
            mx = std::max (mx, x);
        }
        return (mx - mn) / 48.0; // relative period spread (1 kHz = 48 samples)
    };

    const auto still = pitchWander (dsp::Mode::Tape, 0.0f, 0.0f);
    const auto wobbly = pitchWander (dsp::Mode::Tape, 1.0f, 1.0f);
    INFO ("still " << still << " wobbly " << wobbly);
    REQUIRE (still < 0.002);
    REQUIRE (wobbly > 0.01);
}

TEST_CASE ("Feedback 200 % stays bounded in every implemented mode and style", "[stability][quick]")
{
    const auto mode = GENERATE (Catch::Generators::range (0, dsp::kNumModes));
    const auto style = GENERATE (dsp::Style::Single, dsp::Style::PingPong, dsp::Style::Quad);
    const auto era = GENERATE (dsp::Era::Past, dsp::Era::Future);

    dsp::EngineParams p;
    p.mode = static_cast<dsp::Mode> (mode);
    p.style = style;
    p.era = era;
    p.feedback = 2.0f;
    p.driveDb = 24.0f;
    p.age = 1.0f;
    p.mix = 1.0f;
    p.syncL = p.syncR = dsp::kSyncFree;
    p.delayLMs = 30.0f;
    p.delayRMs = 45.0f;
    p.quadFeedback = dsp::QuadFeedback::Swell;

    dsp::DelayEngine e;
    e.prepare (kFs);
    e.setParameters (p);
    auto s = test::whiteNoise (test::samplesFor (2.0, kFs), 0.5f);
    test::render (e, s);
    INFO (dsp::kModeNames[static_cast<std::size_t> (mode)]);
    REQUIRE (test::allFinite (s));
    REQUIRE (test::peakAbs (s.l) <= 1.0f);
    REQUIRE (test::peakAbs (s.r) <= 1.0f);
    REQUIRE (e.getResetCount() == 1); // only the reset from prepare(): no NaN recovery needed
}

TEST_CASE ("Mode and era changes crossfade without clicks", "[modes][crossfade]")
{
    auto p = base (dsp::Mode::Tape, 120.0f);
    p.feedback = 0.6f;
    p.mix = 0.5f;
    dsp::DelayEngine e;
    e.prepare (kFs);

    auto s = test::Stereo (test::samplesFor (6.0, kFs));
    for (std::size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = 0.3f * static_cast<float> (std::sin (2.0 * dsp::kPi * 150.0 * static_cast<double> (i) / kFs));

    const auto block = 256;
    float maxJump = 0.0f, prev = 0.0f;
    for (std::size_t pos = 0, n = 0; pos < s.size(); pos += block, ++n)
    {
        if (n % 60 == 0)
        {
            p.mode = static_cast<dsp::Mode> ((n / 60) % 7); // tape + BBD families + Digital
            p.era = static_cast<dsp::Era> ((n / 120) % 3);
        }
        e.setParameters (p);
        const auto len = std::min<std::size_t> (block, s.size() - pos);
        e.process (s.l.data() + pos, s.r.data() + pos, static_cast<int> (len));
        for (std::size_t i = pos; i < pos + len; ++i)
        {
            maxJump = std::max (maxJump, std::abs (s.l[i] - prev));
            prev = s.l[i];
        }
    }
    REQUIRE (test::allFinite (s));
    REQUIRE (maxJump < 0.1f); // a 150 Hz sine at 0.3 moves ~0.006 per sample
}
