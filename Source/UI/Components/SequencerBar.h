#pragma once

// The PATTERN SEQUENCER transport above the keyboard: PLAY / STOP, the eight
// pattern memories, both rows' step lights, and EDIT to open the sequencer.

#include "../../Plugin/ZSynthProcessor.h"
#include "Materials.h"

namespace zsynth::ui
{

class SequencerBar : public juce::Component, private juce::Timer
{
public:
    explicit SequencerBar(ZSynthProcessor& p);
    ~SequencerBar() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onEdit;

private:
    void timerCallback() override;
    void updateButtons();
    juce::Rectangle<int> lightsArea() const;

    ZSynthProcessor& processor;
    juce::TextButton playButton, editButton;
    juce::TextButton patternButtons[ZSynthProcessor::kPatterns];
    material::LegendCache legend;
    int shownStep[2] { -2, -2 };
    int shownLength[2] {};
    bool shownRunning = false;
    int shownPattern = -1;
};

/** One row's step lights with numbers, on the sequencer page (above the knobs). */
class SequencerLights : public juce::Component, private juce::Timer
{
public:
    SequencerLights(ZSynthProcessor& p, int r) : processor(p), row(r)
    {
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }
    ~SequencerLights() override { stopTimer(); }
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override
    {
        int h = processor.getSeqStep(row) * 1000;
        for (int st = 0; st < kPseqSteps; ++st)
            h = h * 3 + (processor.getValue(P::PseqAGate1 + row * kPseqRowStride + st * kPseqStepStride) >= 0.5f ? 1 : 0);
        h += juce::roundToInt(processor.getValue(P::PseqALen + row * kPseqRowStride)) * 7
           + (processor.getValue(P::PseqAOn + row * kPseqRowStride) >= 0.5f ? 1 : 0);
        if (h != shown)
        {
            shown = h;
            repaint();
        }
    }
    ZSynthProcessor& processor;
    int row;
    int shown = -1;
};

/** SONG mode: a light under each song slot, the one playing lit. */
class SongLights : public juce::Component, private juce::Timer
{
public:
    explicit SongLights(ZSynthProcessor& p) : processor(p)
    {
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }
    ~SongLights() override { stopTimer(); }
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override
    {
        const int s = processor.getValue(P::PseqSongOn) >= 0.5f && processor.isSeqRunning() ? processor.getSongSlot() : -1;
        if (s != shown)
        {
            shown = s;
            repaint();
        }
    }
    ZSynthProcessor& processor;
    int shown = -2;
};

/** Draws the 16 step lights of the sequencer's rows (current step lit, steps past
    LENGTH dark, rests hollow). Used on the transport strip and on the sequencer page. */
void drawSequencerLights(juce::Graphics&, juce::Rectangle<float> area, ZSynthProcessor&, int row, bool numbers);

} // namespace zsynth::ui
