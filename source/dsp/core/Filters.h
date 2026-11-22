#pragma once

#include <algorithm>
#include <cmath>

#include "dsp/core/Lfo.h"

namespace oe::dsp
{

/** Clamps a cutoff to a safe range below Nyquist for the bilinear prewarp. */
inline double safeCutoff (double hz, double sampleRate) noexcept
{
    return std::clamp (hz, 1.0, 0.49 * sampleRate);
}

/** DC group delay (samples) of a bilinear-transformed 2nd-order low-pass
    section: tau = 1 / (2 Q tan(pi fc / fs)). Used to keep echoes on time
    although the in-loop filters delay the signal by a few samples. */
inline double lowpassGroupDelay (double cutoffHz, double q, double sampleRate) noexcept
{
    return 1.0 / (2.0 * q * std::tan (kPi * safeCutoff (cutoffHz, sampleRate) / sampleRate));
}

/** Same for a first-order (6 dB/oct) low-pass: tau = 1 / (2 tan(pi fc / fs)). */
inline double onePoleGroupDelay (double cutoffHz, double sampleRate) noexcept
{
    return 0.5 / std::tan (kPi * safeCutoff (cutoffHz, sampleRate) / sampleRate);
}

/** Topology-preserving-transform state variable filter (Zavalishin / Simper).
    Stable under fast modulation; double state for the long recursive loop filters. */
class Svf
{
public:
    struct Outputs
    {
        double lp, bp, hp;
    };

    void setup (double cutoffHz, double q, double sampleRate) noexcept
    {
        g = std::tan (kPi * safeCutoff (cutoffHz, sampleRate) / sampleRate);
        k = 1.0 / std::max (q, 0.05);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

    Outputs process (double x) noexcept
    {
        const auto v3 = x - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

    double lowpass (double x) noexcept { return process (x).lp; }
    double highpass (double x) noexcept { return process (x).hp; }
    double bandpass (double x) noexcept { return process (x).bp; }

private:
    double g = 0.0, k = 1.4142, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;
};

/** TPT one-pole (6 dB/oct) low/high pass. */
class OnePole
{
public:
    void setup (double cutoffHz, double sampleRate) noexcept
    {
        const auto g = std::tan (kPi * safeCutoff (cutoffHz, sampleRate) / sampleRate);
        coeff = g / (1.0 + g);
    }

    void reset() noexcept { s = 0.0; }

    double lowpass (double x) noexcept
    {
        const auto v = coeff * (x - s);
        const auto y = v + s;
        s = y + v;
        return y;
    }

    double highpass (double x) noexcept { return x - lowpass (x); }

private:
    double coeff = 0.0, s = 0.0;
};

/** First-order DC blocker (~5 Hz), keeps the feedback loop centred. */
class DcBlocker
{
public:
    void setup (double sampleRate, double cutoffHz = 5.0) noexcept
    {
        r = std::exp (-kTwoPi * cutoffHz / sampleRate);
    }

    void reset() noexcept { x1 = y1 = 0.0; }

    double process (double x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    double r = 0.9995, x1 = 0.0, y1 = 0.0;
};

/** Butterworth low-pass of order 2 or 4 (cascaded SVFs). */
class ButterworthLowpass
{
public:
    void setup (double cutoffHz, int order, double sampleRate) noexcept
    {
        fourthOrder = order >= 4;
        if (fourthOrder)
        {
            s1.setup (cutoffHz, 0.5411961, sampleRate);
            s2.setup (cutoffHz, 1.3065630, sampleRate);
            groupDelay = lowpassGroupDelay (cutoffHz, 0.5411961, sampleRate)
                       + lowpassGroupDelay (cutoffHz, 1.3065630, sampleRate);
        }
        else
        {
            s1.setup (cutoffHz, 0.7071068, sampleRate);
            groupDelay = lowpassGroupDelay (cutoffHz, 0.7071068, sampleRate);
        }
    }

    double groupDelaySamples() const noexcept { return groupDelay; }

    void reset() noexcept
    {
        s1.reset();
        s2.reset();
    }

    double process (double x) noexcept
    {
        auto y = s1.lowpass (x);
        if (fourthOrder)
            y = s2.lowpass (y);
        return y;
    }

private:
    Svf s1, s2;
    bool fourthOrder = false;
    double groupDelay = 0.0;
};

} // namespace oe::dsp
