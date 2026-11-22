#include "PresetManager.h"

#include "Parameters.h"

#if OE_HAS_BINARY_DATA
 #include "BinaryData.h"
#endif

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, std::atomic<bool>& mixLockFlag)
    : apvts (state), mixLock (mixLockFlag)
{
    loadFactoryPresets();
    rescanUserPresets();
}

void PresetManager::loadFactoryPresets()
{
#if OE_HAS_BINARY_DATA
    // Embedded resources are named like "NN_Name_xml"; sort them by that order.
    juce::StringArray names;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]).endsWithIgnoreCase (".xml"))
            names.add (BinaryData::namedResourceList[i]);
    names.sort (true);

    for (const auto& resource : names)
    {
        int size = 0;
        const auto* data = BinaryData::getNamedResource (resource.toRawUTF8(), size);
        if (data == nullptr || size <= 0)
            continue;
        auto xml = juce::parseXML (juce::String::fromUTF8 (data, size));
        if (xml == nullptr || ! xml->hasTagName (kPresetTag))
            continue;
        auto* p = presets.add (new Preset());
        p->name = xml->getStringAttribute ("name", resource);
        p->category = xml->getStringAttribute ("category", "Factory");
        p->isFactory = true;
        p->xml = xmlStore.add (xml.release());
    }
#endif
}

void PresetManager::rescanUserPresets()
{
    for (int i = presets.size(); --i >= 0;)
        if (! presets[i]->isFactory)
        {
            xmlStore.removeObject (presets[i]->xml);
            presets.remove (i);
        }

    const auto folder = getUserPresetFolder();
    if (! folder.isDirectory())
        return;

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (const auto& f : files)
    {
        auto xml = juce::parseXML (f);
        if (xml == nullptr || ! xml->hasTagName (kPresetTag))
            continue;
        auto* p = presets.add (new Preset());
        p->name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p->category = "User";
        p->isFactory = false;
        p->file = f;
        p->xml = xmlStore.add (xml.release());
    }
}

juce::File PresetManager::getUserPresetFolder()
{
    // ~/Library/Application Support/... (macOS), %APPDATA%\... (Windows), ~/.config/... (Linux).
    // Sandboxed hosts (GarageBand) map this into their container automatically.
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Alejandro Audio")
        .getChildFile ("Orbit Echo")
        .getChildFile ("Presets");
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (const auto* p : presets)
        cats.addIfNotAlreadyThere (p->category);
    return cats;
}

juce::String PresetManager::getCurrentName() const
{
    const juce::ScopedLock sl (nameLock);
    return currentName;
}

void PresetManager::setCurrent (int index, const juce::String& name)
{
    const juce::ScopedLock sl (nameLock);
    currentIndex = index;
    currentName = name;
}

bool PresetManager::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (kPresetTag))
        return false;

    for (const auto& id : oe::params::allIds())
    {
        if (id == oe::params::id::mix && mixLock.load())
            continue; // Mix Lock: presets never change the Mix

        auto* param = apvts.getParameter (id);
        if (param == nullptr)
            continue;

        float value = param->convertFrom0to1 (param->getDefaultValue());
        for (const auto* child : xml.getChildWithTagNameIterator ("PARAM"))
            if (child->getStringAttribute ("id") == id)
            {
                value = static_cast<float> (child->getDoubleAttribute ("value", value));
                break;
            }

        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
    return true;
}

bool PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presets.size()))
        return false;
    if (! applyPresetXml (*presets[index]->xml))
        return false;
    setCurrent (index, presets[index]->name);
    modified = false;
    return true;
}

void PresetManager::loadNext()
{
    if (presets.isEmpty())
        return;
    loadPreset ((currentIndex + 1 + presets.size()) % presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.isEmpty())
        return;
    loadPreset ((currentIndex - 1 + presets.size()) % presets.size());
}

std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const juce::String& name, const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> (kPresetTag);
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("version", kPresetVersion);
    for (const auto& id : oe::params::allIds())
        if (auto* raw = apvts.getRawParameterValue (id))
        {
            auto* child = xml->createNewChildElement ("PARAM");
            child->setAttribute ("id", id);
            child->setAttribute ("value", static_cast<double> (raw->load()));
        }
    return xml;
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    const auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false; // e.g. a read-only / sandboxed location: fail gracefully

    const auto file = folder.getChildFile (name + ".xml");
    if (! createPresetXml (rawName.trim(), "User")->writeTo (file))
        return false;

    rescanUserPresets();
    for (int i = 0; i < presets.size(); ++i)
        if (! presets[i]->isFactory && presets[i]->file == file)
            setCurrent (i, presets[i]->name);
    modified = false;
    return true;
}

void PresetManager::restoreCurrentName (const juce::String& name)
{
    int index = -1;
    for (int i = 0; i < presets.size(); ++i)
        if (presets[i]->name == name)
        {
            index = i;
            break;
        }
    setCurrent (index, name);
    modified = false;
}

// ---------------------------------------------------------------------------
void PresetManager::captureSlot (std::unique_ptr<juce::XmlElement>& slot) const
{
    slot = createPresetXml ("AB", "AB");
}

void PresetManager::selectA()
{
    if (! onB)
        return;
    captureSlot (slotB);
    if (slotA != nullptr)
        applyPresetXml (*slotA);
    onB = false;
}

void PresetManager::selectB()
{
    if (onB)
        return;
    captureSlot (slotA);
    if (slotB != nullptr)
        applyPresetXml (*slotB);
    else
        slotB = createPresetXml ("AB", "AB"); // first visit: B starts as a copy of A
    onB = true;
}

void PresetManager::copyCurrentToOther()
{
    captureSlot (onB ? slotA : slotB);
}
