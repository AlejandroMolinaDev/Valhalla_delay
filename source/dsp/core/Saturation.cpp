#include "dsp/core/Saturation.h"

namespace oe::dsp
{

const char* curveName (SatCurve curve) noexcept
{
    switch (curve)
    {
        case SatCurve::Clean:     return "Clean";
        case SatCurve::Soft:      return "Soft";
        case SatCurve::Converter: return "Converter";
        case SatCurve::Tape:      return "Tape";
        case SatCurve::TapeHot:   return "Tape (hot)";
        case SatCurve::Bbd:       return "BBD";
        case SatCurve::Thick:     return "Thick";
    }
    return "?";
}

void saturateBlock (SatCurve curve, float* data, int numSamples, float drive) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        data[i] = saturate (curve, data[i] * drive);
}

} // namespace oe::dsp
