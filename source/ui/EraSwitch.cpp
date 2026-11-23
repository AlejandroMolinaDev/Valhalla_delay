#include "ui/EraSwitch.h"

#include "Parameters.h"

namespace oe::ui
{

EraSwitch::EraSwitch (juce::AudioProcessorValueTreeState& state) : TextSelector ("ERA", state, params::id::era)
{
    setTooltip ("Era: Past = darker and dirtier, Present = in between, Future = cleaner, more linear and "
                "extended. Changes the hard-wired filters, saturation, modulation shapes and diffusion of "
                "the current mode.");
}

} // namespace oe::ui
