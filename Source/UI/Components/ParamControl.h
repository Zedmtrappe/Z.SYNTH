#pragma once

// One custom-drawn control bound to one parameter. All kinds (knobs, faders,
// selectors, switches, step controls) share this class so that behaviour is
// identical across the panel:
//   * drag (vertical; Shift = fine), mouse wheel, double-click resets to default
//   * keyboard: Up/Right and Down/Left step, Page keys step x10, Home = default
//   * right-click: MIDI learn / clear mapping / reset
//   * tooltip with name, current value, help text and inactive reason
//   * screen-reader value interface
// The control never owns synthesis state: it reads and writes the APVTS
// parameter through a juce::ParameterAttachment.

#include "../../MIDI/MidiLearn.h"
#include "../../Parameters/ParameterLayout.h"
#include "../Layout/PanelLayout.h"
#include "Materials.h"
#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace zsynth::ui
{

class ParamControl : public juce::Component, public juce::TooltipClient
{
public:
    ParamControl(juce::AudioProcessorValueTreeState& state, int paramIndex, ControlKind kind,
                 const juce::String& label, MidiLearn* learn);
    ~ParamControl() override;

    int getParamIndex() const { return index; }
    ControlKind getKind() const { return kind; }
    float getValue() const { return value; }
    float getNormalised() const { return param.convertTo0to1(value); }

    void setInactive(bool inactive, const juce::String& reason);
    bool isInactive() const { return inactive; }

    /** Sequencer step currently playing (StepSlider only). */
    void setHighlighted(bool h) { if (h != highlighted) { highlighted = h; repaint(); } }
    /** Colour used by StepToggle LEDs. */
    void setLedColour(juce::Colour c) { ledColour = c; }
    /** Section colour used on fader caps (JD-style colour coding). */
    void setAccent(juce::Colour c) { accent = c; }

    juce::String getTooltip() override;
    juce::String getValueText() const { return formatValue(index, value); }

    /** Called while dragging so the panel can show a value bubble. */
    std::function<void(ParamControl&, bool dragging)> onDragStateChanged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    void setNormalisedAsGesture(float normalised);
    void resetToDefault();

private:
    void valueChanged(float newValue);
    void stepBy(int steps);
    int numSteps() const;
    bool isBipolar() const;
    bool isDragKind() const;
    void showContextMenu();
    void showChoiceMenu();

    void paintKnob(juce::Graphics&, float diameter, bool showLabel);
    void paintSelector(juce::Graphics&);
    void paintSwitch(juce::Graphics&);
    void paintVSlider(juce::Graphics&);
    void paintFader(juce::Graphics&);
    void paintStepSlider(juce::Graphics&);
    void paintStepToggle(juce::Graphics&);
    void paintHSlider(juce::Graphics&);
    void paintSegmented(juce::Graphics&);
    void paintLearnOutline(juce::Graphics&);

    juce::AudioProcessorValueTreeState& state;
    const int index;
    const ControlKind kind;
    const juce::String label;
    MidiLearn* midiLearn;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float value = 0.0f;

    bool inactive = false;
    juce::String inactiveReason;
    bool hovered = false, dragging = false, highlighted = false;
    float dragStartNorm = 0.0f, dragNorm = 0.0f;
    juce::Colour ledColour = Colours::accent;
    juce::Colour accent = Colours::accent;
    material::LegendCache legend;
};

} // namespace zsynth::ui
