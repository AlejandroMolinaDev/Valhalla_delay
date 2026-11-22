#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

/** Factory presets (embedded XML, by category), user presets (XML files in the
    user's application-data folder), A/B comparison and Mix Lock.

    All methods run on the message thread. Loading a preset sets every
    parameter through the host-notifying path (so automation and undo in the
    DAW see it) and keeps the current Mix when Mix Lock is on. */
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category;
        juce::XmlElement* xml = nullptr; // owned by the manager
        bool isFactory = true;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState& state, std::atomic<bool>& mixLockFlag);

    static constexpr const char* kPresetTag = "OrbitEchoPreset";
    static constexpr int kPresetVersion = 1;

    int getNumPresets() const noexcept { return presets.size(); }
    const Preset& getPreset (int index) const { return *presets[index]; }
    juce::StringArray getCategories() const;

    int getCurrentIndex() const noexcept { return currentIndex; }
    /** Thread-safe (the host may save the plugin state from another thread). */
    juce::String getCurrentName() const;

    bool loadPreset (int index);
    void loadNext();
    void loadPrevious();

    /** Applies a preset XML to the parameters (respecting Mix Lock). */
    bool applyPresetXml (const juce::XmlElement& xml);

    /** Serialises the current parameters as a preset. */
    std::unique_ptr<juce::XmlElement> createPresetXml (const juce::String& name, const juce::String& category) const;

    /** Saves the current settings as a user preset (overwrites same name). */
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    static juce::File getUserPresetFolder();

    // A/B comparison ---------------------------------------------------------
    bool isOnB() const noexcept { return onB; }
    void selectA();
    void selectB();
    void copyCurrentToOther();

    // Restoring the session's preset name (plugin state) is not a user edit.
    void restoreCurrentName (const juce::String& name);
    void markModified() noexcept { modified = true; }
    bool isModified() const noexcept { return modified.load(); }

private:
    void loadFactoryPresets();
    void captureSlot (std::unique_ptr<juce::XmlElement>& slot) const;
    void setCurrent (int index, const juce::String& name);

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<bool>& mixLock;
    juce::OwnedArray<juce::XmlElement> xmlStore;
    juce::OwnedArray<Preset> presets;
    int currentIndex = -1;
    juce::String currentName { "Init" };
    juce::CriticalSection nameLock;
    std::atomic<bool> modified { false };

    bool onB = false;
    std::unique_ptr<juce::XmlElement> slotA, slotB;
};
