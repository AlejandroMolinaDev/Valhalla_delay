#pragma once

#include <cmath>

namespace oe::dsp
{

/** Saturation curves used inside the feedback loop. Every curve is bounded
    (|y| <= ~1.05), which is what keeps Feedback up to 200 % from blowing up:
    the loop self-oscillates into the curve instead of growing without limit. */
enum class SatCurve
{
    Clean,      // very soft, high threshold (modern digital, Future era)
    Soft,       // tanh-like rational curve
    Converter,  // 80s converter: linear until ~-1 dBFS then a short knee into clip
    Tape,       // asymmetric soft curve (even harmonics), progressive
    TapeHot,    // tape with lower knee and stronger compression (Past era)
    Bbd,        // op-amp style: soft but with a firmer ceiling, slight asymmetry
    Thick       // dense, early-compressing curve (Analog+)
};

namespace sat
{
    /** Rational approximation of tanh, exact limit at |x| >= 3. */
    inline float tanhApprox (float x) noexcept
    {
        if (x > 3.0f)
            return 1.0f;
        if (x < -3.0f)
            return -1.0f;
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float clean (float x) noexcept
    {
        // Linear up to 0.8, then a tanh knee asymptotic to 1.0.
        const auto a = std::abs (x);
        if (a <= 0.8f)
            return x;
        const auto y = 0.8f + 0.2f * tanhApprox ((a - 0.8f) * 5.0f);
        return x < 0.0f ? -y : y;
    }

    inline float converter (float x) noexcept
    {
        const auto a = std::abs (x);
        if (a <= 0.89f)
            return x;
        const auto over = a - 0.89f;
        const auto y = 0.89f + 0.11f * tanhApprox (over * 9.0f);
        return x < 0.0f ? -y : y;
    }

    inline float tape (float x, float bias) noexcept
    {
        // Small DC bias before a symmetric curve -> asymmetric transfer, even harmonics.
        return tanhApprox (x + bias) - tanhApprox (bias);
    }

    inline float bbd (float x) noexcept
    {
        const auto y = tanhApprox (1.1f * x + 0.04f * x * x);
        return y * 0.95f;
    }

    inline float thick (float x) noexcept
    {
        const auto y = tanhApprox (1.6f * x);
        return 0.92f * y + 0.08f * tanhApprox (4.0f * x);
    }
} // namespace sat

inline float saturate (SatCurve curve, float x) noexcept
{
    switch (curve)
    {
        case SatCurve::Clean:     return sat::clean (x);
        case SatCurve::Soft:      return sat::tanhApprox (x);
        case SatCurve::Converter: return sat::converter (x);
        case SatCurve::Tape:      return sat::tape (x, 0.12f);
        case SatCurve::TapeHot:   return sat::tape (1.35f * x, 0.2f) * 0.85f;
        case SatCurve::Bbd:       return sat::bbd (x);
        case SatCurve::Thick:     return sat::thick (x);
    }
    return x;
}

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }

} // namespace oe::dsp
