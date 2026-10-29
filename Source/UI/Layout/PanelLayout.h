#pragma once

// =============================================================================
//  The canonical Z.SYNTH front-panel specification.
//
//  Every section, control and utility element of the instrument has exactly
//  one rectangle on a fixed 1920 x 1200 logical canvas, defined in
//  PanelLayout.cpp. The editor positions components *only* from this table and
//  scales the whole canvas uniformly to the window, so the panel can never
//  reflow, rearrange, scroll or change with presets or synthesis modes.
//  Tests/StateTests verify: every control lies inside its section, no two
//  controls overlap, and every parameter appears on the panel exactly once.
// =============================================================================

#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace zsynth::ui
{

enum class ControlKind
{
    LargeKnob,   // 64 px knob + label
    Knob,        // 42 px knob + label
    SmallKnob,   // 28 px knob, label supplied by a column header
    Selector,    // LCD choice display with label above
    Switch,      // LED push button with label
    VSlider,     // vertical fader (envelopes)
    StepSlider,  // sequencer step value
    StepToggle,  // tiny LED toggle (sequencer gate/accent/slide)
    HSlider,     // horizontal bipolar amount (mod matrix)
    Segmented    // segmented choice buttons (engine mode)
};

struct ControlSpec
{
    const char* paramId;
    ControlKind kind;
    juce::Rectangle<int> bounds;
    juce::String label;
    juce::String section;
};

struct SectionSpec
{
    juce::String id;
    juce::String title;
    juce::Rectangle<int> bounds;
    bool subPanel = false; // drawn as an inset inside a parent section
};

struct UtilitySpec
{
    juce::String id;
    juce::Rectangle<int> bounds;
};

/** Static decorative text (column headers, row labels). */
struct LabelSpec
{
    juce::String text;
    juce::Rectangle<int> bounds;
    juce::Justification justification = juce::Justification::centred;
};

class PanelLayout
{
public:
    static constexpr int kWidth = 1920;
    static constexpr int kHeight = 1200;
    static constexpr float kMinScale = 0.5f;
    static constexpr float kMaxScale = 1.5f;

    static const PanelLayout& get();

    const std::vector<SectionSpec>& sections() const { return sectionList; }
    const std::vector<ControlSpec>& controls() const { return controlList; }
    const std::vector<UtilitySpec>& utilities() const { return utilityList; }
    const std::vector<LabelSpec>& labels() const { return labelList; }

    juce::Rectangle<int> section(const juce::String& id) const;
    juce::Rectangle<int> utility(const juce::String& id) const;

private:
    PanelLayout();

    std::vector<SectionSpec> sectionList;
    std::vector<ControlSpec> controlList;
    std::vector<UtilitySpec> utilityList;
    std::vector<LabelSpec> labelList;
};

/** Size of each control kind on the canvas. */
juce::Point<int> controlSize(ControlKind kind);

} // namespace zsynth::ui
