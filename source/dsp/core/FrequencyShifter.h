#pragma once

#include <array>

namespace oe::dsp
{

/** Single-sideband frequency shifter (linear shift in Hz, not harmonic).

    A Hilbert transformer made of two parallel chains of four 2nd-order
    all-pass sections (coefficients of the classic 90° phase-difference
    network, accurate over practically the whole audio band) gives an in-phase and a
    quadrature signal; they are mixed with a double-precision quadrature
    oscillator: y = I·cos(φ) + Q·sin(φ) (upper sideband). Left and right shift by
    (shift ± detune / 2) Hz: small detunes widen the image, large ones turn
    into a fast auto-pan. */
class FrequencyShifter
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setShift (double shiftHz, double detuneHz) noexcept;

    void process (float& l, float& r) noexcept;

    /** Group delay of the in-phase path around 1 kHz (samples), for echo-time compensation. */
    double groupDelaySamples() const noexcept { return groupDelay; }

    /** Hilbert pair for one sample (exposed for tests): returns {I, Q}. */
    struct IQ
    {
        double i, q;
    };
    IQ hilbert (int channel, double x) noexcept;

private:
    struct Section
    {
        double a2 = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        double process (double x) noexcept
        {
            const auto y = a2 * (x + y2) - x2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    double fs = 48000.0;
    std::array<std::array<Section, 4>, 2> pathA, pathB;
    std::array<double, 2> delayedA {};
    std::array<double, 2> phase {};
    std::array<double, 2> increment {};
    double groupDelay = 0.0;
};

} // namespace oe::dsp
