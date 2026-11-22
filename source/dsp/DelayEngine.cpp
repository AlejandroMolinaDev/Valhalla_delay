#include "dsp/DelayEngine.h"

namespace oe::dsp
{

void DelayEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

} // namespace oe::dsp
