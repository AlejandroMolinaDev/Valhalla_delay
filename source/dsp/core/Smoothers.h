#pragma once

#include <cmath>

namespace oe::dsp
{

/** Exponential one-pole parameter smoother (per-sample). */
template <typename T = float>
class OnePoleSmoother
{
public:
    void setTime (double sampleRate, double seconds) noexcept
    {
        coeff = seconds > 0.0 ? static_cast<T> (1.0 - std::exp (-1.0 / (seconds * sampleRate))) : T (1);
    }

    void reset (T value) noexcept { current = target = value; }
    void setTarget (T value) noexcept { target = value; }

    T next() noexcept
    {
        current += coeff * (target - current);
        return current;
    }

    T getCurrent() const noexcept { return current; }
    T getTarget() const noexcept { return target; }

private:
    T current {}, target {}, coeff = T (1);
};

/** Critically damped second-order smoother (two cascaded one-poles).

    Used for delay times: the read head accelerates and decelerates smoothly,
    like the capstan motor of a tape echo or the clock of a BBD, which produces
    the characteristic re-pitching when the delay time is changed. Runs in
    double precision because the value is a position in samples (up to ~4e6). */
class MotorSmoother
{
public:
    void setTime (double sampleRate, double seconds) noexcept
    {
        coeff = seconds > 0.0 ? 1.0 - std::exp (-1.0 / (seconds * sampleRate)) : 1.0;
    }

    void reset (double value) noexcept { stage1 = stage2 = target = value; }
    void setTarget (double value) noexcept { target = value; }

    double next() noexcept
    {
        stage1 += coeff * (target - stage1);
        stage2 += coeff * (stage1 - stage2);
        return stage2;
    }

    double getCurrent() const noexcept { return stage2; }
    double getTarget() const noexcept { return target; }
    bool isSettled (double tolerance) const noexcept { return std::abs (target - stage2) < tolerance; }

private:
    double stage1 = 0.0, stage2 = 0.0, target = 0.0, coeff = 1.0;
};

/** Linear 0 -> 1 ramp over a fixed number of samples (crossfades). */
class LinearRamp
{
public:
    void start (int lengthSamples) noexcept
    {
        position = 0.0f;
        increment = lengthSamples > 0 ? 1.0f / static_cast<float> (lengthSamples) : 1.0f;
        active = true;
    }

    float next() noexcept
    {
        if (! active)
            return 1.0f;
        position += increment;
        if (position >= 1.0f)
        {
            position = 1.0f;
            active = false;
        }
        return position;
    }

    bool isActive() const noexcept { return active; }
    void finish() noexcept { position = 1.0f; active = false; }

private:
    float position = 1.0f, increment = 1.0f;
    bool active = false;
};

} // namespace oe::dsp
