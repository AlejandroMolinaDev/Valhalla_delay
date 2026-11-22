#include "ui/StyleMenu.h"

#include "Parameters.h"
#include "dsp/Types.h"

namespace oe::ui
{

StyleMenu::StyleMenu (juce::AudioProcessorValueTreeState& state)
{
    for (int i = 0; i < dsp::kNumStyles; ++i)
        addItem (dsp::kStyleNames[static_cast<std::size_t> (i)], i + 1);

    setTooltip ("Style: how the left and right delays are arranged. Single = one time (+ Spread), "
                "Dual = two independent delays, Ratio = R is a fraction of L, PingPong = bouncing, "
                "Quad = four-head tape echo.");
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, params::id::style, *this);
}

} // namespace oe::ui
