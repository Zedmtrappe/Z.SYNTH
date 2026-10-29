#include "ParameterLayout.h"

namespace zsynth
{
namespace
{
    bool isFmRatio(int index)
    {
        return index >= P::Op1Ratio && index <= P::Op6R && (index - P::Op1Ratio) % kOpParamStride == 0;
    }

    juce::StringArray choicesOf(int index)
    {
        return juce::StringArray::fromTokens(paramDef(index).choices, "|", "");
    }

    bool isBipolarUnitless(const ParamDef& d) { return d.min < 0.0f && d.max <= 1.0f && d.min >= -1.0f; }
}

float snapFmRatio(float r)
{
    return r < 0.75f ? 0.5f : std::round(r);
}

juce::NormalisableRange<float> rangeFor(int index)
{
    const ParamDef& d = paramDef(index);
    if (d.type != ParamType::Float)
        return { d.min, d.max, 1.0f };

    if (isFmRatio(index))
    {
        // Equal spacing in log ratio, snapping to 0.5 and whole numbers.
        const float lo = d.min, hi = d.max;
        return { lo, hi,
                 [lo, hi](float, float, float x) { return snapFmRatio(lo * std::pow(hi / lo, x)); },
                 [lo, hi](float, float, float v) { return std::log(std::max(v, lo) / lo) / std::log(hi / lo); },
                 [](float, float, float v) { return snapFmRatio(v); } };
    }

    if (d.scale == LOG)
    {
        const float lo = d.min, hi = d.max;
        return { lo, hi,
                 [lo, hi](float, float, float x) { return lo * std::pow(hi / lo, x); },
                 [lo, hi](float, float, float v) { return std::log(juce::jlimit(lo, hi, v) / lo) / std::log(hi / lo); },
                 nullptr };
    }

    juce::NormalisableRange<float> r(d.min, d.max);
    if (d.scale > 0.0f)
        r.setSkewForCentre(d.scale);
    return r;
}

juce::String formatValue(int index, float v)
{
    const ParamDef& d = paramDef(index);
    const juce::String unit(d.unit);

    switch (d.type)
    {
        case ParamType::Bool:
            return v >= 0.5f ? "On" : "Off";
        case ParamType::Choice:
        {
            const auto c = choicesOf(index);
            return c[juce::jlimit(0, c.size() - 1, (int) std::lround(v))];
        }
        case ParamType::Int:
        {
            const int i = (int) std::lround(v);
            if (index == P::MidiChannel)
                return i == 0 ? "Omni" : "Ch " + juce::String(i);
            if (unit == "st" || unit == "oct")
                return (i > 0 ? "+" : "") + juce::String(i) + " " + unit;
            return juce::String(i);
        }
        case ParamType::Float:
        default:
            break;
    }

    if (unit == "Hz")
        return v >= 1000.0f ? juce::String(v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String(v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
    if (unit == "s")
    {
        if (v <= 0.0f && index == P::GlideTime)
            return "Off";
        return v < 1.0f ? juce::String(v * 1000.0f, v < 0.01f ? 1 : 0) + " ms" : juce::String(v, 2) + " s";
    }
    if (unit == "ms")  return juce::String(v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == "dB")  return v <= -59.9f ? "-inf dB" : juce::String(v, 1) + " dB";
    if (unit == "ct")  return (v > 0.0f ? "+" : "") + juce::String(v, 1) + " ct";
    if (unit == "%")   return juce::String(v, 0) + "%";
    if (unit == "deg") return juce::String(v, 0) + juce::String::fromUTF8("\xc2\xb0");
    if (unit == "BPM") return juce::String(v, 1) + " BPM";
    if (unit == "x")   return "x" + juce::String(v, v < 1.0f ? 2 : 0);

    if (index == P::Width)
        return juce::String(v * 100.0f, 0) + "%";
    if (isBipolarUnitless(d))
        return (v > 0.0f ? "+" : "") + juce::String(v * 100.0f, 0) + "%";
    if (index >= P::Op1Fine && index <= P::Op6R && (index - P::Op1Fine) % kOpParamStride == 0)
        return "+" + juce::String(v, 2);
    return juce::String(v * 100.0f, 0) + "%";
}

float parseValue(int index, const juce::String& text)
{
    const ParamDef& d = paramDef(index);
    const auto t = text.trim();

    if (d.type == ParamType::Bool)
        return (t.equalsIgnoreCase("on") || t == "1" || t.equalsIgnoreCase("true")) ? 1.0f : 0.0f;

    if (d.type == ParamType::Choice)
    {
        const auto c = choicesOf(index);
        const int i = c.indexOf(t, true);
        return i >= 0 ? (float) i : juce::jlimit(d.min, d.max, (float) t.getIntValue());
    }

    if (index == P::MidiChannel && t.equalsIgnoreCase("omni"))
        return 0.0f;
    if (t.containsIgnoreCase("inf"))
        return t.startsWithChar('-') ? d.min : d.max;

    float v = t.retainCharacters("0123456789.-+").getFloatValue();
    const auto lower = t.toLowerCase();
    const juce::String unit(d.unit);
    if (unit == "Hz" && lower.contains("k"))
        v *= 1000.0f;
    else if (unit == "s" && lower.contains("ms"))
        v *= 0.001f;
    else if (unit.isEmpty() && d.type == ParamType::Float && (lower.contains("%") || std::abs(v) > 1.0f))
        v *= 0.01f;
    return juce::jlimit(d.min, d.max, v);
}

juce::String valueToPresetText(int index, float value)
{
    const ParamDef& d = paramDef(index);
    switch (d.type)
    {
        case ParamType::Bool:   return value >= 0.5f ? "on" : "off";
        case ParamType::Choice: return choicesOf(index)[juce::jlimit(0, choiceCount(index) - 1, (int) std::lround(value))];
        case ParamType::Int:    return juce::String((int) std::lround(value));
        case ParamType::Float:
        default:                return juce::String(value, 6).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
    }
}

bool presetTextToValue(int index, const juce::String& text, float& out)
{
    const ParamDef& d = paramDef(index);
    const auto t = text.trim();
    if (t.isEmpty())
        return false;

    switch (d.type)
    {
        case ParamType::Bool:
            if (t.equalsIgnoreCase("on") || t == "1" || t.equalsIgnoreCase("true"))  { out = 1.0f; return true; }
            if (t.equalsIgnoreCase("off") || t == "0" || t.equalsIgnoreCase("false")) { out = 0.0f; return true; }
            return false;
        case ParamType::Choice:
        {
            const int i = choicesOf(index).indexOf(t, true);
            if (i >= 0) { out = (float) i; return true; }
            if (t.containsOnly("0123456789")) { out = juce::jlimit(d.min, d.max, (float) t.getIntValue()); return true; }
            return false;
        }
        case ParamType::Int:
        case ParamType::Float:
        default:
        {
            if (! t.containsOnly("0123456789.-+eE"))
                return false;
            const float v = t.getFloatValue();
            if (! std::isfinite(v))
                return false;
            out = juce::jlimit(d.min, d.max, d.type == ParamType::Int ? std::round(v) : v);
            return true;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamDef& d = paramDef(i);
        const juce::ParameterID pid { d.id, 1 };
        auto toText = [i](float v, int) { return formatValue(i, v); };
        auto fromText = [i](const juce::String& s) { return parseValue(i, s); };

        switch (d.type)
        {
            case ParamType::Bool:
                layout.add(std::make_unique<juce::AudioParameterBool>(pid, d.name, d.def >= 0.5f,
                    juce::AudioParameterBoolAttributes().withStringFromValueFunction([i](bool b, int) { return formatValue(i, b ? 1.0f : 0.0f); })));
                break;
            case ParamType::Choice:
                layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, choicesOf(i), (int) d.def));
                break;
            case ParamType::Int:
                layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, (int) d.min, (int) d.max, (int) d.def,
                    juce::AudioParameterIntAttributes()
                        .withStringFromValueFunction([i](int v, int) { return formatValue(i, (float) v); })
                        .withValueFromStringFunction([i](const juce::String& s) { return (int) std::lround(parseValue(i, s)); })));
                break;
            case ParamType::Float:
            default:
                layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, rangeFor(i), d.def,
                    juce::AudioParameterFloatAttributes()
                        .withLabel(d.unit)
                        .withStringFromValueFunction(toText)
                        .withValueFromStringFunction(fromText)));
                break;
        }
    }
    return layout;
}

} // namespace zsynth
