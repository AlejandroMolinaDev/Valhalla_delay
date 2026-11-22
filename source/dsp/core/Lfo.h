#pragma once

#include <cmath>

namespace oe::dsp
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

enum class LfoShape
{
    Sine,
    Triangle,
    SmoothTriangle, // triangle with rounded peaks (no "corner" pitch jumps)
    SmoothSquare,   // square with ~10% slewed edges: alternating pitch up/down
    Ramp,           // smoothed sawtooth: slow rise, fast smoothed fall
    Hump            // asymmetric "gravity" curve, mostly one-sided
};

/** Evaluates a periodic shape at a phase in [0, 1). Output in [-1, 1]. */
inline double lfoShape (LfoShape shape, double phase) noexcept
{
    phase -= std::floor (phase);

    switch (shape)
    {
        case LfoShape::Sine:
            return std::sin (kTwoPi * phase);

        case LfoShape::Triangle:
            return phase < 0.25 ? 4.0 * phase
                 : phase < 0.75 ? 2.0 - 4.0 * phase
                                : 4.0 * phase - 4.0;

        case LfoShape::SmoothTriangle:
        {
            const auto t = phase < 0.25 ? 4.0 * phase
                         : phase < 0.75 ? 2.0 - 4.0 * phase
                                        : 4.0 * phase - 4.0;
            // Soft-saturate the corners: f(t) = 1.5t - 0.5t^3 keeps ±1 at the peaks with zero slope.
            return 1.5 * t - 0.5 * t * t * t;
        }

        case LfoShape::SmoothSquare:
        {
            const auto s = std::sin (kTwoPi * phase);
            return std::tanh (4.0 * s) / std::tanh (4.0);
        }

        case LfoShape::Ramp:
        {
            constexpr double fall = 0.12;
            if (phase < 1.0 - fall)
                return -1.0 + 2.0 * phase / (1.0 - fall);
            const auto u = (phase - (1.0 - fall)) / fall;
            return 1.0 - 2.0 * (0.5 - 0.5 * std::cos (kPi * u));
        }

        case LfoShape::Hump:
        {
            const auto s = std::sin (kPi * phase);
            return 2.0 * s * s * s - 1.0;
        }
    }

    return 0.0;
}

/** Phase accumulator (double precision, so slow rates never drift). */
class Lfo
{
public:
    void setRate (double hz, double sampleRate) noexcept { increment = sampleRate > 0.0 ? hz / sampleRate : 0.0; }
    void reset (double startPhase = 0.0) noexcept { phase = startPhase - std::floor (startPhase); }

    void advance() noexcept
    {
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    double getPhase() const noexcept { return phase; }
    double value (LfoShape shape, double phaseOffset = 0.0) const noexcept { return lfoShape (shape, phase + phaseOffset); }

private:
    double phase = 0.0, increment = 0.0;
};

} // namespace oe::dsp
