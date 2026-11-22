#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "dsp/DelayEngine.h"
#include "dsp/TempoSync.h"
#include "dsp/core/Denormals.h"
#include "dsp/core/Random.h"

namespace oe::test
{

struct Stereo
{
    std::vector<float> l, r;

    explicit Stereo (std::size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    std::size_t size() const { return l.size(); }
};

inline Stereo whiteNoise (std::size_t n, float amplitude, std::uint32_t seed = 1234)
{
    Stereo s (n);
    dsp::FastRandom rngL (seed), rngR (seed * 7919u + 13u);
    for (std::size_t i = 0; i < n; ++i)
    {
        s.l[i] = amplitude * rngL.nextBipolar();
        s.r[i] = amplitude * rngR.nextBipolar();
    }
    return s;
}

inline Stereo impulse (std::size_t n, float amplitude = 0.5f, std::size_t at = 0)
{
    Stereo s (n);
    s.l[at] = amplitude;
    s.r[at] = amplitude;
    return s;
}

/** Processes the whole signal in place, split into blocks of the given size(s) (cycled). */
inline void render (dsp::DelayEngine& engine, Stereo& s, const std::vector<int>& blockSizes = { 512 })
{
    dsp::ScopedFlushDenormals ftz;
    std::size_t pos = 0, k = 0;
    while (pos < s.size())
    {
        const auto want = static_cast<std::size_t> (blockSizes[k++ % blockSizes.size()]);
        const auto n = std::min (want, s.size() - pos);
        engine.process (s.l.data() + pos, s.r.data() + pos, static_cast<int> (n));
        pos += n;
    }
}

/** Parameters for a transparent, unmodulated digital delay (impulse tests). */
inline dsp::EngineParams cleanDigital (float delayMs = 250.0f)
{
    dsp::EngineParams p;
    p.mode = dsp::Mode::Digital;
    p.era = dsp::Era::Future;
    p.style = dsp::Style::Single;
    p.delayLMs = delayMs;
    p.delayRMs = delayMs;
    p.syncL = p.syncR = dsp::kSyncFree;
    p.feedback = 0.0f;
    p.diffAmount = 0.0f;
    p.age = 0.0f;
    p.modDepth = 0.0f;
    p.mix = 1.0f;
    p.width = 1.0f;
    p.lowCutHz = 10.0f;
    p.highCutHz = 20000.0f;
    p.driveDb = 0.0f;
    p.duck = 0.0f;
    return p;
}

inline std::size_t argmaxAbs (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    std::size_t best = from;
    float bestV = -1.0f;
    for (std::size_t i = from; i < to; ++i)
        if (std::abs (v[i]) > bestV)
        {
            bestV = std::abs (v[i]);
            best = i;
        }
    return best;
}

inline float peakAbs (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    float p = 0.0f;
    for (std::size_t i = from; i < to; ++i)
        p = std::max (p, std::abs (v[i]));
    return p;
}

inline double rms (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    if (to <= from)
        return 0.0;
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double> (v[i]) * static_cast<double> (v[i]);
    return std::sqrt (sum / static_cast<double> (to - from));
}

inline bool allFinite (const Stereo& s)
{
    for (std::size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
            return false;
    return true;
}

inline double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-12)); }

inline std::size_t samplesFor (double seconds, double sampleRate)
{
    return static_cast<std::size_t> (std::llround (seconds * sampleRate));
}

} // namespace oe::test
