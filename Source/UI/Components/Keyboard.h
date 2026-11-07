#pragma once

// On-screen keyboard and performance wheels. The keyboard drives the
// processor's MidiKeyboardState, so its notes enter the audio thread through
// exactly the same path as external MIDI.

#include "../../Plugin/ZSynthProcessor.h"
#include "Materials.h"
#include "Theme.h"

namespace zsynth::ui
{

class ZKeyboard : public juce::MidiKeyboardComponent
{
public:
    ZKeyboard(juce::MidiKeyboardState& state);
    void shiftOctave(int delta);
    int getLowestKey() const { return lowestKey; }
    void resized() override;
    void paint(juce::Graphics&) override;
    /** Shows the split: keys below `point` tinted (LOWER zone) and a marker. point < 0 = no split.
        The keys of the part not being edited are greyed out. */
    void setSplit(int point, bool editingLower);
    /** Outlines the keys of a scale (bit n = pitch class n, 0 = C); tonic gets a stronger outline. mask 0 = none. */
    void setScale(uint16_t mask, int tonic);

protected:
    void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour noteFillColour) override;
    void drawUpDownButton(juce::Graphics&, int, int, bool, bool, bool) override {}

private:
    int lowestKey = 36; // C2, 61 keys shown
    int splitPoint = -1;
    bool editLower = false;
    bool greyed(int note) const { return splitPoint >= 0 && (editLower ? note >= splitPoint : note < splitPoint); }
    uint16_t scaleMask = 0;
    int scaleTonic = -1;
    void outlineKey(juce::Graphics&, int note, juce::Rectangle<float> top, bool black) const;
};

/** Pitch (spring-loaded, bipolar) or modulation (stays put, unipolar) wheel. */
class PerformanceWheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    PerformanceWheel(ZSynthProcessor& p, bool isPitch);
    void refresh();
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void send(float v);
    ZSynthProcessor& processor;
    const bool pitch;
    float position = 0.0f; // -1..1 (pitch) or 0..1 (mod)
    material::LegendCache legend;
    bool dragging = false;
    float startPos = 0.0f;
};

} // namespace zsynth::ui
