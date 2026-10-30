#pragma once

// The fixed ZYNTH front panel (1920 x 1200 logical canvas) and its popup
// editor pages. Every child is positioned from PanelLayout; nothing here
// computes positions of its own.

#include "../Plugin/ZSynthProcessor.h"
#include "Components/Displays.h"
#include "Components/Keyboard.h"
#include "Components/ParamControl.h"
#include "Components/PresetBrowser.h"
#include "Layout/PanelLayout.h"

namespace zsynth::ui
{

/** Draws the sections and silkscreen labels of one layout page. */
void paintLayoutPage(juce::Graphics& g, const juce::String& page);

/** A floating, fixed-layout editor page (matrix, FM operators, sequencer steps). */
class PopupPage : public juce::Component
{
public:
    explicit PopupPage(const PageSpec& spec);
    void paint(juce::Graphics&) override;
    const juce::String& getPageId() const { return id; }
    std::function<void()> onClose;

private:
    juce::String id;
    juce::TextButton closeButton { juce::String::fromUTF8("\xc3\x97") };
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel(ZSynthProcessor& p);
    ~MainPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override {}
    bool keyPressed(const juce::KeyPress&) override;

    const std::vector<std::unique_ptr<ParamControl>>& getControls() const { return controls; }
    void showPresetBrowser(bool show);
    bool isPresetBrowserVisible() const { return browser.isVisible(); }

    /** Shows one popup editor page ("matrix", "fm", "seq") or hides all (empty). */
    void showPage(const juce::String& page);
    juce::String getVisiblePage() const;

private:
    void timerCallback() override;
    void buildPages();
    void buildControls();
    void buildUtilities();
    void renderBackground(float scale);
    void updateActivity();
    void savePreset();
    void showPresetMenu();
    juce::Component& parentForPage(const juce::String& page);

    template <typename T, typename... Args>
    T& addUtility(const juce::String& id, Args&&... args)
    {
        auto c = std::make_unique<T>(std::forward<Args>(args)...);
        juce::String page = kMainPage;
        for (const auto& u : PanelLayout::get().utilities())
            if (u.id == id)
                page = u.page;
        c->setBounds(PanelLayout::get().utility(id));
        parentForPage(page).addAndMakeVisible(*c);
        auto& ref = *c;
        ownedUtilities.push_back(std::move(c));
        return ref;
    }

    ZSynthProcessor& processor;
    std::vector<std::unique_ptr<PopupPage>> pages;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<std::unique_ptr<juce::Component>> ownedUtilities;
    std::vector<Display*> displays;
    std::vector<PerformanceWheel*> wheels;
    std::vector<ParamControl*> stepSliders;
    ZKeyboard* keyboard = nullptr;

    PresetBrowser browser;
    juce::Label valueBubble;
    juce::Image background;
    float backgroundScale = 0.0f;
    int activityCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace zsynth::ui
