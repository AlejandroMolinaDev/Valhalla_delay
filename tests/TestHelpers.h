#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace oe::test
{

struct Stereo
{
    std::vector<float> l, r;

    explicit Stereo (std::size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    std::size_t size() const { return l.size(); }
};

inline float peakAbs (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    float p = 0.0f;
    for (std::size_t i = from; i < to; ++i)
        p = std::max (p, std::abs (v[i]));
    return p;
}

inline double rms (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    if (to <= from)
        return 0.0;
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double> (v[i]) * static_cast<double> (v[i]);
    return std::sqrt (sum / static_cast<double> (to - from));
}

inline double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-12)); }

} // namespace oe::test
