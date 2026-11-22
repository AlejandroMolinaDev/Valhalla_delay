#include "dsp/core/DiffusionNetwork.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

namespace
{
    // Stage delays as fractions of the total (mutually incommensurate, sum = 1).
    constexpr std::array<double, DiffusionNetwork::kStages> kRatios { 0.043, 0.067, 0.089, 0.113,
                                                                      0.131, 0.157, 0.181, 0.219 };
    // Right channel: slightly detuned ratios for stereo decorrelation.
    constexpr std::array<double, DiffusionNetwork::kStages> kRightSkew { 1.031, 0.973, 1.019, 0.987,
                                                                         1.023, 0.979, 1.011, 0.991 };
    constexpr double kMinTotalSeconds = 0.001;
    constexpr double kMaxTotalSeconds = 20.0;
} // namespace

double DiffusionNetwork::gainForAmount (double amount01) noexcept
{
    // Calibrated (tests/dsp/DiffusionTests.cpp): 68 % -> equal attack/decay,
    // 91 % -> dense reverb with Feedback 0 %.
    const auto a = std::clamp (amount01, 0.0, 1.0);
    return 0.866 * std::pow (a, 1.53); // g(0.68) = 0.48, g(0.91) = 0.75
}

void DiffusionNetwork::prepare (double sampleRate, double maxSeconds)
{
    fs = sampleRate;
    const auto total = maxSeconds * fs;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kStages; ++i)
        {
            auto& s = stages[static_cast<std::size_t> (ch)][static_cast<std::size_t> (i)];
            const auto ratio = kRatios[static_cast<std::size_t> (i)] * (ch == 1 ? kRightSkew[static_cast<std::size_t> (i)] : 1.0);
            s.maxDelay = std::max (4.0, ratio * total * 1.04);
            const auto size = static_cast<std::size_t> (std::ceil (s.maxDelay + 0.01 * fs)) + 4;
            if (s.buffer.size() != size)
                s.buffer.assign (size, 0.0f);
            s.lfo.setSeed (static_cast<std::uint32_t> (101 + 17 * i + 1009 * ch));
            // Advanced once every kModDecimation samples (slow drift, 0.2-0.8 Hz).
            s.lfo.setRate (0.21 + 0.07 * i + 0.03 * ch, fs / kModDecimation);
        }

    gCoeff = 1.0 - std::exp (-1.0 / (0.03 * fs));
    delayCoeff = 1.0 - std::exp (-1.0 / (0.12 * fs));
    mixStep = 1.0 / (0.03 * fs);
    reset();
}

void DiffusionNetwork::reset() noexcept
{
    for (auto& ch : stages)
        for (auto& s : ch)
        {
            std::fill (s.buffer.begin(), s.buffer.end(), 0.0f);
            s.write = 0;
            s.delay = s.target;
            s.lfo.reset();
            s.modNow = s.modStep = 0.0;
        }
    modCountdown = 0;
    g = gTarget;
    mix = mixTarget;
    active = mix > 0.0;
}

bool DiffusionNetwork::clearStep() noexcept
{
    if (clearIndex >= 2 * kStages)
    {
        clearIndex = 0;
        return true;
    }
    auto& s = stages[static_cast<std::size_t> (clearIndex / kStages)][static_cast<std::size_t> (clearIndex % kStages)];
    std::fill (s.buffer.begin(), s.buffer.end(), 0.0f);
    s.write = 0;
    ++clearIndex;
    return false;
}

void DiffusionNetwork::setParameters (float amount01, float size01, double referenceDelay, bool snap) noexcept
{
    gTarget = gainForAmount (static_cast<double> (amount01));
    mixTarget = amount01 > 0.0f ? 1.0 : 0.0;

    const auto size = static_cast<double> (std::clamp (size01, 0.0f, 1.0f));
    const auto frac = 0.01 + 0.99 * size * size;
    totalDelay = std::clamp (frac * referenceDelay, kMinTotalSeconds * fs, kMaxTotalSeconds * fs);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kStages; ++i)
        {
            auto& s = stages[static_cast<std::size_t> (ch)][static_cast<std::size_t> (i)];
            const auto ratio = kRatios[static_cast<std::size_t> (i)] * (ch == 1 ? kRightSkew[static_cast<std::size_t> (i)] : 1.0);
            s.target = std::clamp (ratio * totalDelay, 1.0, s.maxDelay);
            s.modDepth = std::min (0.0002 * fs + 0.002 * s.target, 0.25 * s.target);
            if (snap)
                s.delay = s.target;
        }

    if (snap)
    {
        g = gTarget;
        mix = mixTarget;
    }
    if (mixTarget > 0.0)
        active = true;
}

float DiffusionNetwork::processStage (Stage& s, float x, double gain) noexcept
{
    const auto size = static_cast<std::uint32_t> (s.buffer.size());
    s.delay += delayCoeff * (s.target - s.delay);
    s.modNow += s.modStep;
    const auto d = std::clamp (s.delay + s.modDepth * s.modNow, 1.0, s.maxDelay);

    // Fractional read of the delayed state v[n - d] (linear interpolation).
    // Exact-size buffers (no power-of-two padding) with branch wraps: less memory.
    const auto di = static_cast<std::uint32_t> (d);
    const auto f = static_cast<float> (d - static_cast<double> (di));
    const auto i0 = s.write >= di ? s.write - di : s.write + size - di;
    const auto i1 = i0 == 0 ? size - 1u : i0 - 1u;
    const auto delayed = s.buffer[i0] + f * (s.buffer[i1] - s.buffer[i0]);

    // Schroeder all-pass, single delay element form: v = x + g*v[n-d]; y = -g*v + v[n-d].
    const auto gf = static_cast<float> (gain);
    const auto v = x + gf * delayed;
    if (++s.write == size)
        s.write = 0;
    s.buffer[s.write] = v;
    return delayed - gf * v;
}

void DiffusionNetwork::process (float& l, float& r) noexcept
{
    if (! active)
        return;

    ++processCount;
    g += gCoeff * (gTarget - g);

    if (--modCountdown <= 0)
    {
        modCountdown = kModDecimation;
        for (auto& ch : stages)
            for (auto& s : ch)
            {
                s.lfo.advance();
                s.modStep = (s.lfo.value() - s.modNow) / static_cast<double> (kModDecimation);
            }
    }
    mix = mixTarget > mix ? std::min (mixTarget, mix + mixStep) : std::max (mixTarget, mix - mixStep);

    auto yl = l, yr = r;
    for (auto& s : stages[0])
        yl = processStage (s, yl, g);
    for (auto& s : stages[1])
        yr = processStage (s, yr, g);

    const auto m = static_cast<float> (mix);
    l += m * (yl - l);
    r += m * (yr - r);

    double sum = 0.0;
    for (const auto& s : stages[0])
        sum += s.delay;
    compensation = mix * sum;

    if (mix <= 0.0 && mixTarget <= 0.0)
    {
        active = false;
        compensation = 0.0;
    }
}

} // namespace oe::dsp
