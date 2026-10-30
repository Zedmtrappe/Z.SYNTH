#include "PresetFormat.h"

#include "../Parameters/ParameterLayout.h"

namespace zsynth
{

juce::String migrateParamId(const juce::String& id, int fromVersion)
{
    // No identifiers have been renamed yet. When one is, add a mapping here:
    //   if (fromVersion < 2 && id == "old.id") return "new.id";
    juce::ignoreUnused(fromVersion);
    return id;
}

std::unique_ptr<juce::XmlElement> presetToXml(const PresetData& preset)
{
    auto xml = std::make_unique<juce::XmlElement>("ZSynthPreset");
    xml->setAttribute("formatVersion", kPresetFormatVersion);
    xml->setAttribute("name", preset.name);
    xml->setAttribute("category", preset.category);
    xml->setAttribute("author", preset.author);
    if (preset.comment.isNotEmpty())
        xml->setAttribute("comment", preset.comment);

    for (int i = 0; i < kNumParams; ++i)
    {
        auto* p = xml->createNewChildElement("Param");
        p->setAttribute("id", paramDef(i).id);
        p->setAttribute("value", valueToPresetText(i, preset.values[(size_t) i]));
    }
    return xml;
}

PresetLoadResult presetFromXml(const juce::XmlElement& xml, PresetData& out)
{
    PresetLoadResult result;
    if (! xml.hasTagName("ZSynthPreset"))
    {
        result.error = "Not a ZYNTH preset (root element is <" + xml.getTagName() + ">)";
        return result;
    }

    const int version = xml.getIntAttribute("formatVersion", 0);
    if (version <= 0)
    {
        result.error = "Preset has no valid formatVersion";
        return result;
    }
    if (version > kPresetFormatVersion)
        result.warnings.add("Preset was saved by a newer ZYNTH (format " + juce::String(version) + "); unknown settings were ignored");

    PresetData p;
    p.name = xml.getStringAttribute("name", "Untitled").substring(0, 64);
    p.category = xml.getStringAttribute("category", "User").substring(0, 32);
    p.author = xml.getStringAttribute("author").substring(0, 64);
    p.comment = xml.getStringAttribute("comment").substring(0, 512);

    for (auto* child : xml.getChildWithTagNameIterator("Param"))
    {
        const auto id = migrateParamId(child->getStringAttribute("id"), version);
        const int index = findParamIndex(id.toRawUTF8());
        if (index < 0)
        {
            result.warnings.add("Unknown parameter '" + id + "' ignored");
            continue;
        }
        float v;
        if (presetTextToValue(index, child->getStringAttribute("value"), v))
            p.values[(size_t) index] = v;
        else
            result.warnings.add("Invalid value for '" + id + "'; default used");
    }

    out = p;
    result.ok = true;
    return result;
}

PresetLoadResult loadPresetFile(const juce::File& file, PresetData& out)
{
    PresetLoadResult result;
    if (! file.existsAsFile())
    {
        result.error = "File not found: " + file.getFullPathName();
        return result;
    }
    if (file.getSize() > 4 * 1024 * 1024)
    {
        result.error = "File is too large to be a preset";
        return result;
    }
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
    {
        result.error = "Could not parse " + file.getFileName() + " (damaged or not XML)";
        return result;
    }
    return presetFromXml(*xml, out);
}

juce::Result savePresetFile(const juce::File& file, const PresetData& preset)
{
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not create folder " + file.getParentDirectory().getFullPathName());
    const auto xml = presetToXml(preset);
    // Write to a temporary file first so a failed write never destroys an existing preset.
    juce::TemporaryFile temp(file);
    if (! xml->writeTo(temp.getFile()))
        return juce::Result::fail("Could not write " + temp.getFile().getFullPathName());
    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not replace " + file.getFullPathName());
    return juce::Result::ok();
}

PresetData capturePreset(juce::AudioProcessorValueTreeState& state)
{
    PresetData p;
    for (int i = 0; i < kNumParams; ++i)
        if (auto* raw = state.getRawParameterValue(paramDef(i).id))
            p.values[(size_t) i] = raw->load();
    return p;
}

bool isGlobalSetupParam(int index)
{
    return juce::String(paramDef(index).group) == "Global";
}

void applyPreset(const PresetData& preset, juce::AudioProcessorValueTreeState& state, ApplyScope scope)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        if (scope == ApplyScope::SoundOnly && isGlobalSetupParam(i))
            continue;
        if (auto* param = state.getParameter(paramDef(i).id))
        {
            const float normalised = param->convertTo0to1(preset.values[(size_t) i]);
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalised);
            param->endChangeGesture();
        }
    }
}

std::vector<PresetData> parsePresetBank(const juce::String& text, juce::StringArray& errors)
{
    std::vector<PresetData> bank;
    PresetData* current = nullptr;
    juce::String base; // optional "inherit" source

    auto lines = juce::StringArray::fromLines(text);
    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        const auto line = lines[lineNumber].upToFirstOccurrenceOf("#", false, false).trim();
        if (line.isEmpty())
            continue;

        if (line.startsWith("[") && line.endsWith("]"))
        {
            bank.emplace_back();
            current = &bank.back();
            current->name = line.substring(1, line.length() - 1).trim();
            current->category = "Uncategorised";
            current->author = "ZYNTH Factory";
            continue;
        }

        if (current == nullptr || ! line.contains("="))
        {
            errors.add("Line " + juce::String(lineNumber + 1) + ": unexpected '" + line + "'");
            continue;
        }

        const auto key = line.upToFirstOccurrenceOf("=", false, false).trim();
        const auto value = line.fromFirstOccurrenceOf("=", false, false).trim();

        if (key == "category") { current->category = value; continue; }
        if (key == "author")   { current->author = value;   continue; }
        if (key == "comment")  { current->comment = value;  continue; }
        if (key == "inherit")
        {
            bool found = false;
            for (const auto& p : bank)
                if (p.name == value && &p != current)
                {
                    current->values = p.values;
                    found = true;
                    break;
                }
            if (! found)
                errors.add("Line " + juce::String(lineNumber + 1) + ": cannot inherit from unknown preset '" + value + "'");
            continue;
        }

        const int index = findParamIndex(key.toRawUTF8());
        if (index < 0)
        {
            errors.add("Line " + juce::String(lineNumber + 1) + ": unknown parameter '" + key + "' in '" + current->name + "'");
            continue;
        }
        float v;
        if (presetTextToValue(index, value, v))
            current->values[(size_t) index] = v;
        else
            errors.add("Line " + juce::String(lineNumber + 1) + ": bad value '" + value + "' for " + key + " in '" + current->name + "'");
    }
    return bank;
}

} // namespace zsynth
