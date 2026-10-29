#pragma once

// Builds the JUCE AudioProcessorValueTreeState layout from the single
// parameter table (ParamTable.h) and provides value <-> text conversion that
// the host, the panel and the preset system all share.

#include "ParamTable.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace zsynth
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Normalisable range used for a parameter (log / skew / snapping included). */
juce::NormalisableRange<float> rangeFor(int index);

/** Human-readable value with units, e.g. "1.25 kHz", "350 ms", "+7 st", "Saw". */
juce::String formatValue(int index, float value);

/** Parses text typed by the user (units optional, "k" suffix allowed). */
float parseValue(int index, const juce::String& text);

/** Converts between a real value and the stored preset text (labels for choices). */
juce::String valueToPresetText(int index, float value);
bool presetTextToValue(int index, const juce::String& text, float& valueOut);

/** Snaps an FM coarse ratio to 0.5 or a whole number. */
float snapFmRatio(float r);

} // namespace zsynth
