#include "dsp/core/Phaser.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

namespace
{
    constexpr double kMinHz = 180.0;
    constexpr double kMaxHz = 5200.0;
} // namespace

void Phaser::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    reset();
}

void Phaser::reset() noexcept
{
    for (auto& ch : state)
        ch.fill (0.0);
    lastOut = { 0.0, 0.0 };
    lfo.reset();
}

void Phaser::set (int stages, double rateHz, double depth01, double resonance01) noexcept
{
    numStages = std::clamp (stages, 2, kMaxStages);
    lfo.setRate (rateHz, fs);
    depth = std::clamp (depth01, 0.0, 1.0) / (1.0 + rateHz / 1.5);
    feedback = 0.85 * std::clamp (resonance01, 0.0, 1.0);
}

double Phaser::coefficientFor (double lfoValue) const noexcept
{
    // Exponential sweep around the geometric centre of [kMinHz, kMaxHz].
    const auto centre = std::sqrt (kMinHz * kMaxHz);
    const auto span = std::log (kMaxHz / kMinHz) * 0.5;
    const auto hz = std::min (centre * std::exp (span * depth * lfoValue), 0.45 * fs);
    const auto t = std::tan (kPi * hz / fs);
    return (t - 1.0) / (t + 1.0); // first-order all-pass coefficient
}

double Phaser::processChannel (int ch, double x, double a) noexcept
{
    auto& s = state[static_cast<std::size_t> (ch)];
    auto y = x + feedback * lastOut[static_cast<std::size_t> (ch)];
    for (int i = 0; i < numStages; ++i)
    {
        // Direct form I first-order all-pass: y = a*x + s; s = x - a*y.
        auto& z = s[static_cast<std::size_t> (i)];
        const auto out = a * y + z;
        z = y - a * out;
        y = out;
    }
    lastOut[static_cast<std::size_t> (ch)] = y;
    return 0.5 * (x + y);
}

void Phaser::process (float& l, float& r) noexcept
{
    lfo.advance();
    const auto aL = coefficientFor (lfo.value (LfoShape::Sine, 0.0));
    const auto aR = coefficientFor (lfo.value (LfoShape::Sine, 0.25));
    l = static_cast<float> (processChannel (0, static_cast<double> (l), aL));
    r = static_cast<float> (processChannel (1, static_cast<double> (r), aR));
}

} // namespace oe::dsp
