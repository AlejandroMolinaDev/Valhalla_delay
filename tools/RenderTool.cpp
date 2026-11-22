// oe_render: offline renderer and CPU profiler for the DSP engine (no JUCE).
//
//   oe_render --bench                  CPU cost of every mode (x styles) at 48 kHz
//   oe_render --wav out.wav [mode]     renders a test signal through a mode (16-bit WAV)

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "dsp/DelayEngine.h"
#include "dsp/core/Denormals.h"
#include "dsp/core/Random.h"

using namespace oe;

namespace
{
    constexpr double kFs = 48000.0;

    double benchmark (dsp::Mode mode, dsp::Style style, float diffusion, double seconds)
    {
        dsp::EngineParams p;
        p.mode = mode;
        p.style = style;
        p.feedback = 0.6f;
        p.diffAmount = diffusion;
        p.duck = 0.3f;
        p.syncL = p.syncR = 0;
        p.delayLMs = 330.0f;
        p.delayRMs = 410.0f;

        dsp::DelayEngine engine;
        engine.prepare (kFs);
        engine.setParameters (p);

        const int block = 256;
        std::vector<float> l (block), r (block);
        dsp::FastRandom rng (7);
        const auto blocks = static_cast<int> (seconds * kFs / block);

        dsp::ScopedFlushDenormals ftz;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                l[static_cast<std::size_t> (i)] = 0.2f * rng.nextBipolar();
                r[static_cast<std::size_t> (i)] = 0.2f * rng.nextBipolar();
            }
            engine.process (l.data(), r.data(), block);
        }
        const auto t1 = std::chrono::steady_clock::now();
        const auto cpu = std::chrono::duration<double> (t1 - t0).count();
        return 100.0 * cpu / (static_cast<double> (blocks) * block / kFs); // % of one core in real time
    }

    void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r)
    {
        std::ofstream f (path, std::ios::binary);
        const auto n = static_cast<std::uint32_t> (l.size());
        const std::uint32_t dataBytes = n * 4u, rate = static_cast<std::uint32_t> (kFs), byteRate = rate * 4u;
        const std::uint16_t fmt = 1, channels = 2, align = 4, bits = 16;
        const std::uint32_t fmtSize = 16, riffSize = 36u + dataBytes;
        f.write ("RIFF", 4);
        f.write (reinterpret_cast<const char*> (&riffSize), 4);
        f.write ("WAVEfmt ", 8);
        f.write (reinterpret_cast<const char*> (&fmtSize), 4);
        f.write (reinterpret_cast<const char*> (&fmt), 2);
        f.write (reinterpret_cast<const char*> (&channels), 2);
        f.write (reinterpret_cast<const char*> (&rate), 4);
        f.write (reinterpret_cast<const char*> (&byteRate), 4);
        f.write (reinterpret_cast<const char*> (&align), 2);
        f.write (reinterpret_cast<const char*> (&bits), 2);
        f.write ("data", 4);
        f.write (reinterpret_cast<const char*> (&dataBytes), 4);
        for (std::uint32_t i = 0; i < n; ++i)
            for (const auto* ch : { &l, &r })
            {
                const auto v = static_cast<std::int16_t> (std::lround (std::fmax (-1.0, std::fmin (1.0, static_cast<double> ((*ch)[i]))) * 32767.0));
                f.write (reinterpret_cast<const char*> (&v), 2);
            }
    }
} // namespace

int main (int argc, char** argv)
{
    if (argc >= 2 && std::strcmp (argv[1], "--bench") == 0)
    {
        std::printf ("CPU (%% of one core, 48 kHz stereo, 256-sample blocks)\n");
        std::printf ("%-15s %8s %8s %8s %8s\n", "mode", "Single", "Quad", "PingPong", "Single+Diff");
        double worst = 0.0;
        for (int m = 0; m < dsp::kNumModes; ++m)
        {
            const auto mode = static_cast<dsp::Mode> (m);
            const auto a = benchmark (mode, dsp::Style::Single, 0.0f, 4.0);
            const auto b = benchmark (mode, dsp::Style::Quad, 0.0f, 4.0);
            const auto c = benchmark (mode, dsp::Style::PingPong, 0.0f, 4.0);
            const auto d = benchmark (mode, dsp::Style::Single, 0.6f, 4.0);
            worst = std::fmax (worst, std::fmax (std::fmax (a, b), std::fmax (c, d)));
            std::printf ("%-15s %7.2f%% %7.2f%% %7.2f%% %7.2f%%\n", dsp::kModeNames[static_cast<std::size_t> (m)], a, b, c, d);
        }
        std::printf ("worst case: %.2f%% of one core\n", worst);
        return 0;
    }

    if (argc >= 3 && std::strcmp (argv[1], "--wav") == 0)
    {
        const auto mode = argc >= 4 ? std::atoi (argv[3]) : 0;
        dsp::EngineParams p;
        p.mode = static_cast<dsp::Mode> (mode);
        p.mix = 0.5f;
        p.feedback = 0.55f;
        dsp::DelayEngine engine;
        engine.prepare (kFs);
        engine.setParameters (p);
        const auto n = static_cast<std::size_t> (6.0 * kFs);
        std::vector<float> l (n, 0.0f), r (n, 0.0f);
        // Plucked test tones every 1.5 s.
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto t = static_cast<double> (i % static_cast<std::size_t> (1.5 * kFs)) / kFs;
            if (t < 0.4)
                l[i] = r[i] = static_cast<float> (0.4 * std::exp (-t * 9.0) * std::sin (2.0 * dsp::kPi * 220.0 * t));
        }
        dsp::ScopedFlushDenormals ftz;
        engine.process (l.data(), r.data(), static_cast<int> (n));
        writeWav (argv[2], l, r);
        std::printf ("wrote %s (%s)\n", argv[2], dsp::kModeNames[static_cast<std::size_t> (mode)]);
        return 0;
    }

    std::printf ("usage: oe_render --bench | --wav out.wav [modeIndex]\n");
    return 1;
}
