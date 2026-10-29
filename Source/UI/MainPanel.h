#pragma once

// The fixed Z.SYNTH front panel (1920 x 1200 logical canvas). Every child is
// positioned from PanelLayout; nothing here computes positions of its own.

#include "../Plugin/ZSynthProcessor.h"
#include "Components/Displays.h"
#include "Components/Keyboard.h"
#include "Components/ParamControl.h"
#include "Components/PresetBrowser.h"
#include "Layout/PanelLayout.h"

namespace zsynth::ui
{

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel(ZSynthProcessor& p);
    ~MainPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override {}

    const std::vector<std::unique_ptr<ParamControl>>& getControls() const { return controls; }
    void showPresetBrowser(bool show);
    bool isPresetBrowserVisible() const { return browser.isVisible(); }

private:
    void timerCallback() override;
    void buildControls();
    void buildUtilities();
    void renderBackground(float scale);
    void paintBackground(juce::Graphics&);
    void updateActivity();
    void savePreset();

    template <typename T, typename... Args>
    T& addUtility(const juce::String& id, Args&&... args)
    {
        auto c = std::make_unique<T>(std::forward<Args>(args)...);
        c->setBounds(PanelLayout::get().utility(id));
        addAndMakeVisible(*c);
        auto& ref = *c;
        ownedUtilities.push_back(std::move(c));
        return ref;
    }

    ZSynthProcessor& processor;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<std::unique_ptr<juce::Component>> ownedUtilities;
    std::vector<Display*> displays;
    std::vector<PerformanceWheel*> wheels;
    std::vector<ParamControl*> stepSliders;
    SeqLeds* seqLeds = nullptr;
    ZKeyboard* keyboard = nullptr;

    PresetBrowser browser;
    juce::Label valueBubble;
    juce::Image background;
    float backgroundScale = 0.0f;
    int activityCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace zsynth::ui
