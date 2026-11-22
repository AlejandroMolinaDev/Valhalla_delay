#include "ui/ModeMenu.h"

#include "Parameters.h"
#include "dsp/Types.h"

namespace oe::ui
{

ModeMenu::ModeMenu (juce::AudioProcessorValueTreeState& state)
{
    using dsp::Mode;
    // Item IDs are choice index + 1 (what ComboBoxAttachment expects); section
    // headings only change how the list is presented.
    auto add = [this] (Mode m) { addItem (dsp::kModeNames[static_cast<std::size_t> (m)], static_cast<int> (m) + 1); };

    addSectionHeading ("Tape");
    for (auto m : { Mode::Tape, Mode::TapeClean, Mode::ReelHiFi, Mode::TapeNR })
        add (m);
    addSectionHeading ("Bucket brigade");
    for (auto m : { Mode::BBD, Mode::BBDBright, Mode::AnalogPlus })
        add (m);
    addSectionHeading ("Digital");
    for (auto m : { Mode::Digital, Mode::Clean, Mode::Crystal, Mode::LoFi })
        add (m);
    addSectionHeading ("Shift & pitch");
    for (auto m : { Mode::GhostShift, Mode::Pitch, Mode::PitchRich, Mode::PitchReverse, Mode::PitchDuck })
        add (m);
    addSectionHeading ("Modulation");
    add (Mode::PhaserDDL);

    setTooltip ("Mode: the delay algorithm (tape, BBD, digital, pitch, frequency shift, phaser). "
                "Values of controls that the new mode hides are kept.");
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, params::id::mode, *this);
}

} // namespace oe::ui
