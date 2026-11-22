#include "ui/PresetBar.h"

#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ui/LookAndFeel.h"

namespace oe::ui
{

PresetBar::PresetBar (OrbitEchoProcessor& p) : processor (p), presets (p.getPresetManager())
{
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    name.setTooltip ("Presets by category");
    save.setTooltip ("Save the current settings as a user preset");
    a.setTooltip ("A/B compare: settings A");
    b.setTooltip ("A/B compare: settings B (starts as a copy of A)");
    lock.setTooltip ("Mix Lock: loading presets keeps the current Mix (saved with the session, not in presets)");

    prev.onClick = [this] { presets.loadPrevious(); refresh(); };
    next.onClick = [this] { presets.loadNext(); refresh(); };
    name.onClick = [this] { showPresetMenu(); };
    save.onClick = [this] { showSaveDialog(); };
    a.onClick = [this] { presets.selectA(); refresh(); };
    b.onClick = [this] { presets.selectB(); refresh(); };
    lock.onClick = [this] { processor.mixLock = ! processor.mixLock.load(); refresh(); };

    for (auto* c : { &prev, &next, &name, &save, &a, &b, &lock })
        addAndMakeVisible (*c);

    refresh();
    startTimerHz (8);
}

PresetBar::~PresetBar() = default;

void PresetBar::timerCallback()
{
    refresh();
}

void PresetBar::refresh()
{
    auto text = presets.getCurrentName();
    if (presets.isModified())
        text << " *";
    name.setButtonText (text);
    a.setToggleState (! presets.isOnB(), juce::dontSendNotification);
    b.setToggleState (presets.isOnB(), juce::dontSendNotification);
    lock.setToggleState (processor.mixLock.load(), juce::dontSendNotification);
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    for (const auto& category : presets.getCategories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < presets.getNumPresets(); ++i)
            if (presets.getPreset (i).category == category)
                sub.addItem (i + 1, presets.getPreset (i).name, true, i == presets.getCurrentIndex());
        menu.addSubMenu (category, sub);
    }
    menu.addSeparator();
    menu.addItem (10000, "Save current as user preset...");
    menu.addItem (10001, "Rescan user presets");

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name), [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result == 10000)
            safe->showSaveDialog();
        else if (result == 10001)
            safe->presets.rescanUserPresets();
        else
            safe->presets.loadPreset (result - 1);
        safe->refresh();
    });
}

void PresetBar::showSaveDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", presets.getCurrentName().upToFirstOccurrenceOf (" *", false, false));
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->setLookAndFeel (&getLookAndFeel());

    juce::Component::SafePointer<PresetBar> safe (this);
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveWindow == nullptr)
            return;
        if (result == 1)
        {
            const auto presetName = safe->saveWindow->getTextEditorContents ("name");
            if (! safe->presets.saveUserPreset (presetName))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                        "The preset could not be saved to\n"
                                                            + PresetManager::getUserPresetFolder().getFullPathName());
        }
        safe->saveWindow.reset();
        safe->refresh();
    }), false);
}

void PresetBar::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<OrbitLookAndFeel*> (&getLookAndFeel());
    auto r = getLocalBounds().toFloat();

    // Orbit glyph: an ellipse with a satellite dot.
    const auto glyph = juce::Rectangle<float> (34.0f, 22.0f).withCentre ({ 30.0f, r.getCentreY() });
    g.setColour (colours::copper);
    g.drawEllipse (glyph, 1.8f);
    g.setColour (colours::amber);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ glyph.getRight() - 4.0f, glyph.getY() + 5.0f }));
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (glyph.getCentre()));

    g.setColour (colours::text);
    g.setFont (lf != nullptr ? lf->font (21.0f, true) : juce::Font (juce::FontOptions (21.0f)));
    g.drawText ("ORBIT ECHO", juce::Rectangle<float> (54.0f, 0.0f, 160.0f, r.getHeight()), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (lf != nullptr ? lf->font (10.5f) : juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("MULTIMODE DELAY", juce::Rectangle<float> (178.0f, 2.0f, 110.0f, r.getHeight()), juce::Justification::centredLeft);
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced (0, 8);
    r.removeFromLeft (290);
    prev.setBounds (r.removeFromLeft (28));
    r.removeFromLeft (4);
    name.setBounds (r.removeFromLeft (236));
    r.removeFromLeft (4);
    next.setBounds (r.removeFromLeft (28));
    r.removeFromLeft (8);
    save.setBounds (r.removeFromLeft (56));

    lock.setBounds (r.removeFromRight (96));
    r.removeFromRight (18);
    b.setBounds (r.removeFromRight (32));
    r.removeFromRight (4);
    a.setBounds (r.removeFromRight (32));
}

} // namespace oe::ui
