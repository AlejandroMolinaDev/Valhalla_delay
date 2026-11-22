#pragma once

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

/** Peak-ish envelope follower with separate attack / release (double state). */
class EnvelopeFollower
{
public:
    void setup (double sampleRate, double attackMs, double releaseMs) noexcept
    {
        attack = 1.0 - std::exp (-1.0 / (attackMs * 0.001 * sampleRate));
        release = 1.0 - std::exp (-1.0 / (releaseMs * 0.001 * sampleRate));
    }

    void reset() noexcept { env = 0.0; }

    double process (float x) noexcept
    {
        const auto a = static_cast<double> (std::abs (x));
        env += (a > env ? attack : release) * (a - env);
        return env;
    }

    double get() const noexcept { return env; }

private:
    double attack = 0.1, release = 0.01, env = 0.0;
};

/** Compander pair as used around BBD chips (NE570-style) and tape noise
    reduction: a 2:1 compressor on the record side and a 1:2 expander on the
    playback side. Each side has its own envelope detector, so the two never
    track perfectly — the source of the characteristic "breathing", of the
    noise that follows the signal dynamics and of the ducking at high drive. */
class Compressor2to1
{
public:
    void setup (double sampleRate, double attackMs, double releaseMs, float referenceLevel) noexcept
    {
        env.setup (sampleRate, attackMs, releaseMs);
        ref = static_cast<double> (referenceLevel);
    }

    void reset() noexcept { env.reset(); }

    /** Returns the compressed sample; output level ~ sqrt(level * ref). */
    float process (float x) noexcept
    {
        const auto e = std::max (env.process (x), 1.0e-4);
        const auto g = std::clamp (std::sqrt (ref / e), 0.1, 10.0);
        return static_cast<float> (static_cast<double> (x) * g);
    }

    double envelope() const noexcept { return env.get(); }

private:
    EnvelopeFollower env;
    double ref = 0.1;
};

class Expander1to2
{
public:
    void setup (double sampleRate, double attackMs, double releaseMs, float referenceLevel) noexcept
    {
        env.setup (sampleRate, attackMs, releaseMs);
        ref = static_cast<double> (referenceLevel);
    }

    void reset() noexcept { env.reset(); }

    /** Returns the expanded sample; restores level = compressed^2 / ref. */
    float process (float x) noexcept
    {
        const auto e = std::max (env.process (x), 1.0e-5);
        const auto g = std::clamp (e / ref, 0.001, 4.0);
        return static_cast<float> (static_cast<double> (x) * g);
    }

private:
    EnvelopeFollower env;
    double ref = 0.1;
};

} // namespace oe::dsp
