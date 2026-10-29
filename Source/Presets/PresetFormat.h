#pragma once

// Versioned preset representation shared by user preset files, the factory
// bank, the plug-in state and the tests. See docs/PRESET_FORMAT.md.

#include "../Parameters/ParamTable.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace zsynth
{

constexpr int kPresetFormatVersion = 1;
inline const char* const kPresetFileExtension = ".zspreset";

struct PresetData
{
    PresetData() { fillDefaults(values.data()); }

    juce::String name { "Init" };
    juce::String category { "Init" };
    juce::String author;
    juce::String comment;
    std::array<float, kNumParams> values {};
};

struct PresetLoadResult
{
    bool ok = false;
    juce::String error;
    juce::StringArray warnings;
};

std::unique_ptr<juce::XmlElement> presetToXml(const PresetData& preset);
PresetLoadResult presetFromXml(const juce::XmlElement& xml, PresetData& out);

PresetLoadResult loadPresetFile(const juce::File& file, PresetData& out);
juce::Result savePresetFile(const juce::File& file, const PresetData& preset);

/** Reads the current parameter values (real units) out of the APVTS. */
PresetData capturePreset(juce::AudioProcessorValueTreeState& state);
/** What a preset recall touches. Presets recall the *sound*; the "Global"
    group (master volume/tune, transpose, octave, tempo, quality, MIDI channel)
    belongs to the user's setup and is only restored with the full plug-in
    state (host session / standalone restart). */
enum class ApplyScope { SoundOnly, Everything };

/** Sets parameters (notifying the host). Call on the message thread. */
void applyPreset(const PresetData& preset, juce::AudioProcessorValueTreeState& state, ApplyScope scope = ApplyScope::SoundOnly);

/** True for parameters that presets do not recall (see ApplyScope). */
bool isGlobalSetupParam(int index);

/** Parses the human-editable factory bank text (see docs/PRESET_FORMAT.md). */
std::vector<PresetData> parsePresetBank(const juce::String& text, juce::StringArray& errors);

/** Maps parameter ids that were renamed in later versions to their current id. */
juce::String migrateParamId(const juce::String& id, int fromVersion);

} // namespace zsynth
