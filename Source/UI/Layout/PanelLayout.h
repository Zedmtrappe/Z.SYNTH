#pragma once

// =============================================================================
//  The canonical ZYNTH front-panel specification.
//
//  Every section, control and utility element has exactly one rectangle on a
//  fixed layout. The main panel is a 1920 x 1200 logical canvas; advanced
//  editors (modulation matrix, FM operators, sequencer steps) are fixed-layout
//  popup "pages" that float over it. The editor positions components *only*
//  from this table and scales the whole canvas uniformly, so nothing ever
//  reflows, rearranges or scrolls. Tests/StateTests verify containment,
//  no overlaps per page, and that every parameter appears exactly once.
// =============================================================================

#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace zsynth::ui
{

enum class ControlKind
{
    LargeKnob,   // 64 px knob + label
    Knob,        // 42 px knob + label
    SmallKnob,   // 28 px knob, label supplied separately
    Selector,    // LCD choice display (label above unless empty)
    Switch,      // LED push button (compact when less than 24 px tall)
    VSlider,     // short vertical fader
    Fader,       // full-height vertical fader with a coloured cap
    ShortFader,  // shorter fader (effects, wavetable)
    StepSlider,  // sequencer step value (bipolar bar)
    StepToggle,  // LED toggle (sequencer gate/accent/slide)
    HSlider,     // horizontal bipolar amount (mod matrix)
    Segmented    // segmented choice buttons (engine mode)
};

inline const juce::String kMainPage = "main";

struct ControlSpec
{
    const char* paramId;
    ControlKind kind;
    juce::Rectangle<int> bounds; // page-local coordinates
    juce::String label;
    juce::String section;
    juce::String page;
};

struct SectionSpec
{
    juce::String id;
    juce::String title;
    juce::Rectangle<int> bounds;
    bool subPanel = false;
    juce::String page;
};

struct UtilitySpec
{
    juce::String id;
    juce::Rectangle<int> bounds;
    juce::String page;
};

/** Static silkscreen text (column headers, row labels, headings). */
struct LabelSpec
{
    juce::String text;
    juce::Rectangle<int> bounds;
    juce::Justification justification = juce::Justification::centred;
    bool heading = false;
    juce::String page;
};

/** A popup editor page and where it floats on the main canvas. */
struct PageSpec
{
    juce::String id;
    juce::String title;
    juce::Rectangle<int> boundsOnCanvas;
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
    const std::vector<PageSpec>& popupPages() const { return pageList; }

    juce::Rectangle<int> section(const juce::String& id) const;
    juce::Rectangle<int> utility(const juce::String& id) const;
    juce::Rectangle<int> pageBounds(const juce::String& page) const; // size of a page (main = canvas)

private:
    PanelLayout();

    std::vector<SectionSpec> sectionList;
    std::vector<ControlSpec> controlList;
    std::vector<UtilitySpec> utilityList;
    std::vector<LabelSpec> labelList;
    std::vector<PageSpec> pageList;
};

/** Default size of each control kind. */
juce::Point<int> controlSize(ControlKind kind);

} // namespace zsynth::ui
