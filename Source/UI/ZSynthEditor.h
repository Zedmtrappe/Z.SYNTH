#pragma once

// Plug-in / standalone editor: hosts the fixed MainPanel and scales it
// uniformly with the window (aspect ratio locked to the 16:10 canvas).

#include "../Plugin/ZSynthProcessor.h"
#include "Components/Theme.h"
#include "MainPanel.h"

namespace zsynth
{

class ZSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZSynthEditor(ZSynthProcessor&);
    ~ZSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    ui::MainPanel& getPanel() { return panel; }

private:
    ZSynthProcessor& processor;
    ui::ZLookAndFeel lookAndFeel;
    ui::MainPanel panel;
    juce::TooltipWindow tooltips { this, 650 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZSynthEditor)
};

} // namespace zsynth
