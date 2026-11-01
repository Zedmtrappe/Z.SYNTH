#pragma once

// The audition strip above the keyboard: choose a clip (built-in or a MIDI
// file), play/stop, loop, and watch the position while tweaking the panel.

#include "../../Plugin/ZSynthProcessor.h"
#include "Materials.h"

namespace zsynth::ui
{

class AuditionBar : public juce::Component, private juce::Timer
{
public:
    explicit AuditionBar(ZSynthProcessor& p);
    ~AuditionBar() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    /** Opens the clip menu (also used by tests). */
    void showClipMenu();

private:
    void timerCallback() override;
    void chooseFile();
    void updateButtons();
    juce::Rectangle<int> clipArea() const;
    juce::Rectangle<int> progressArea() const;
    juce::Rectangle<int> octaveArea() const;
    void shiftOctave(int delta);

    ZSynthProcessor& processor;
    juce::TextButton playButton, loopButton, loadButton, octDownButton, octUpButton;
    std::unique_ptr<juce::FileChooser> chooser;
    material::LegendCache legend, hint;
    juce::String shownName;
    float shownPosition = -1.0f;
    bool shownPlaying = false;
    int shownOctave = 0;
    juce::File lastFolder;
};

} // namespace zsynth::ui
