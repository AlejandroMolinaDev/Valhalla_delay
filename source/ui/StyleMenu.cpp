#include "ui/StyleMenu.h"

#include "Parameters.h"

namespace oe::ui
{

StyleMenu::StyleMenu (juce::AudioProcessorValueTreeState& state) : TextSelector ("STYLE", state, params::id::style)
{
    setTooltip ("Style: how the left and right delays are arranged. Single = one time (+ Spread), "
                "Dual = two independent delays, Ratio = R is a fraction of L, PingPong = bouncing, "
                "Quad = four-head tape echo.");
}

} // namespace oe::ui
