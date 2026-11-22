#pragma once

#include <cmath>

namespace oe::dsp
{

/** Saturation curves used inside the feedback loop.

    Every curve is bounded (|y| <= ~1.05), which keeps Feedback up to 200 %
    from blowing up: the loop self-oscillates into the curve instead of
    growing without limit. Every curve also has a small-signal slope of exactly
    1, so the Feedback knob means the same thing in every mode (100 % = unity
    loop gain for quiet signals). */
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

    /** Slope of tanhApprox at x (for unity-gain normalisation). */
    inline float tanhApproxSlope (float x) noexcept
    {
        const auto x2 = x * x;
        const auto d = 27.0f + 9.0f * x2;
        return (27.0f + 3.0f * x2) * (27.0f + 9.0f * x2) / (d * d) - 18.0f * x2 * (27.0f + x2) / (d * d);
    }

    inline float tape (float x, float bias) noexcept
    {
        // Small DC bias before a symmetric curve -> asymmetric transfer, even harmonics.
        return (tanhApprox (x + bias) - tanhApprox (bias)) / tanhApproxSlope (bias);
    }

    /** Tape driven harder: earlier, more asymmetric compression. */
    inline float tapeHot (float x) noexcept
    {
        constexpr float c = 0.8f;
        return c * tape (x / c, 0.2f);
    }

    inline float bbd (float x) noexcept
    {
        // Op-amp / chip: soft, slightly asymmetric (x|x| term), unity slope.
        return tanhApprox (x + 0.04f * x * std::abs (x));
    }

    inline float thick (float x) noexcept
    {
        // Earlier, denser compression (lower ceiling) with a touch of even harmonics.
        constexpr float c = 0.75f;
        return c * tanhApprox ((x + 0.08f * x * std::abs (x)) / c);
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
        case SatCurve::TapeHot:   return sat::tapeHot (x);
        case SatCurve::Bbd:       return sat::bbd (x);
        case SatCurve::Thick:     return sat::thick (x);
    }
    return x;
}

/** Human-readable curve name (documentation, debugging). */
const char* curveName (SatCurve curve) noexcept;

/** Applies a curve to a buffer in place (offline tools and tests). */
void saturateBlock (SatCurve curve, float* data, int numSamples, float drive) noexcept;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }

} // namespace oe::dsp
