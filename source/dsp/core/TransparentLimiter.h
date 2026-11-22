#pragma once

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

/** Stereo-linked, program-dependent limiter without look-ahead (zero latency).

    Used in the loop of the Clean (program dependent, faster release on
    transients, slower on sustained material) and Crystal (very gentle,
    nearly colourless) modes. Gain changes are smoothed so it does not
    distort the way a clipper would. */
class TransparentLimiter
{
public:
    struct Settings
    {
        float thresholdDb = -3.0f;
        float attackMs = 1.0f;
        float releaseFastMs = 40.0f;
        float releaseSlowMs = 600.0f;
        float ceiling = 1.0f; // hard safety ceiling after the gain stage
    };

    void prepare (double sampleRate, const Settings& s) noexcept;
    void reset() noexcept;
    void process (float& l, float& r) noexcept;
    float getGain() const noexcept { return static_cast<float> (gain); }

private:
    double fs = 48000.0;
    double threshold = 0.7, ceiling = 1.0;
    double attackCoeff = 0.0, releaseFast = 0.0, releaseSlow = 0.0, rmsCoeff = 0.0;
    double gain = 1.0, rms = 0.0;
};

/** Final safety limiter on the wet signal: instant attack, smooth release,
    then a hard clip at -0.3 dBFS that guarantees the output stays bounded. */
class SafetyLimiter
{
public:
    static constexpr float kCeiling = 0.96605f; // -0.3 dBFS

    void prepare (double sampleRate) noexcept
    {
        release = 1.0 - std::exp (-1.0 / (0.08 * sampleRate));
        gain = 1.0;
    }

    void reset() noexcept { gain = 1.0; }

    void process (float& l, float& r) noexcept
    {
        constexpr double threshold = 0.93;
        const auto peak = static_cast<double> (std::max (std::abs (l), std::abs (r)));
        const auto target = peak > threshold ? threshold / peak : 1.0;
        gain = target < gain ? target : gain + release * (target - gain);
        l = std::clamp (static_cast<float> (static_cast<double> (l) * gain), -kCeiling, kCeiling);
        r = std::clamp (static_cast<float> (static_cast<double> (r) * gain), -kCeiling, kCeiling);
    }

private:
    double release = 0.0, gain = 1.0;
};

} // namespace oe::dsp
