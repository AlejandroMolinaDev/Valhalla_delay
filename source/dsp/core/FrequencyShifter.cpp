#include "dsp/core/FrequencyShifter.h"

#include <cmath>
#include <complex>

#include "dsp/core/Lfo.h"

namespace oe::dsp
{

namespace
{
    // 90° phase-difference all-pass network (two chains of z^-2 sections).
    constexpr std::array<double, 4> kPathA { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    constexpr std::array<double, 4> kPathB { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };

    /** Group delay (samples) of path A + one sample of delay at angular frequency w. */
    double pathAGroupDelay (double w)
    {
        auto phaseAt = [] (double omega)
        {
            std::complex<double> h { 1.0, 0.0 };
            const auto z2 = std::polar (1.0, -2.0 * omega);
            for (const auto a : kPathA)
            {
                const auto a2 = a * a;
                h *= (a2 - z2) / (1.0 - a2 * z2);
            }
            h *= std::polar (1.0, -omega); // the extra unit delay on path A
            return std::arg (h);
        };
        constexpr double dw = 1.0e-4;
        auto dphi = phaseAt (w + dw) - phaseAt (w - dw);
        while (dphi > kPi)
            dphi -= kTwoPi;
        while (dphi < -kPi)
            dphi += kTwoPi;
        return -dphi / (2.0 * dw);
    }
} // namespace

void FrequencyShifter::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    for (int ch = 0; ch < 2; ++ch)
        for (std::size_t i = 0; i < 4; ++i)
        {
            pathA[static_cast<std::size_t> (ch)][i].a2 = kPathA[i] * kPathA[i];
            pathB[static_cast<std::size_t> (ch)][i].a2 = kPathB[i] * kPathB[i];
        }
    groupDelay = pathAGroupDelay (kTwoPi * 1000.0 / fs);
    reset();
}

void FrequencyShifter::reset() noexcept
{
    for (auto* paths : { &pathA, &pathB })
        for (auto& ch : *paths)
            for (auto& s : ch)
                s.x1 = s.x2 = s.y1 = s.y2 = 0.0;
    delayedA = { 0.0, 0.0 };
    phase = { 0.0, 0.25 };
}

void FrequencyShifter::setShift (double shiftHz, double detuneHz) noexcept
{
    increment[0] = (shiftHz + 0.5 * detuneHz) / fs;
    increment[1] = (shiftHz - 0.5 * detuneHz) / fs;
}

FrequencyShifter::IQ FrequencyShifter::hilbert (int channel, double x) noexcept
{
    const auto c = static_cast<std::size_t> (channel);
    auto a = x;
    for (auto& s : pathA[c])
        a = s.process (a);
    auto b = x;
    for (auto& s : pathB[c])
        b = s.process (b);
    const auto i = delayedA[c]; // path A delayed by one sample is in phase ...
    delayedA[c] = a;
    return { i, b };            // ... path B leads it by 90°
}

void FrequencyShifter::process (float& l, float& r) noexcept
{
    float* io[2] { &l, &r };
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto c = static_cast<std::size_t> (ch);
        const auto iq = hilbert (ch, static_cast<double> (*io[c]));
        const auto p = kTwoPi * phase[c];
        // With Q leading I by 90°, I·cos + Q·sin is the upper sideband (shift up for positive Hz).
        *io[c] = static_cast<float> (iq.i * std::cos (p) + iq.q * std::sin (p));
        phase[c] += increment[c];
        phase[c] -= std::floor (phase[c]);
    }
}

} // namespace oe::dsp
