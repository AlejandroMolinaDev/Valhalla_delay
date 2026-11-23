#include "ui/ModeMenu.h"

#include "Parameters.h"
#include "dsp/Types.h"

namespace oe::ui
{

ModeMenu::ModeMenu (juce::AudioProcessorValueTreeState& state) : TextSelector ("MODE", state, params::id::mode)
{
    setTooltip ("Mode: the delay algorithm (tape, BBD, digital, pitch, frequency shift, phaser). "
                "Values of controls that the new mode hides are kept.");
}

juce::PopupMenu ModeMenu::buildMenu()
{
    using dsp::Mode;
    juce::PopupMenu m;
    // Item ID = choice index + 1; the section headings only group the list.
    auto add = [this, &m] (Mode mode)
    {
        const auto index = static_cast<int> (mode);
        m.addItem (index + 1, dsp::kModeNames[static_cast<std::size_t> (index)], true, index == currentIndex);
    };

    m.addSectionHeader ("Tape");
    for (auto mode : { Mode::Tape, Mode::TapeClean, Mode::ReelHiFi, Mode::TapeNR })
        add (mode);
    m.addSectionHeader ("Bucket brigade");
    for (auto mode : { Mode::BBD, Mode::BBDBright, Mode::AnalogPlus })
        add (mode);
    m.addSectionHeader ("Digital");
    for (auto mode : { Mode::Digital, Mode::Clean, Mode::Crystal, Mode::LoFi })
        add (mode);
    m.addSectionHeader ("Shift & pitch");
    for (auto mode : { Mode::GhostShift, Mode::Pitch, Mode::PitchRich, Mode::PitchReverse, Mode::PitchDuck })
        add (mode);
    m.addSectionHeader ("Modulation");
    add (Mode::PhaserDDL);
    return m;
}

} // namespace oe::ui
