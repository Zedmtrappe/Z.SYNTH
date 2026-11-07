#pragma once

// The fixed ZYNTH front panel (1920 x 1200 logical canvas) and its popup
// editor pages. Every child is positioned from PanelLayout; nothing here
// computes positions of its own.

#include "../Plugin/ZSynthProcessor.h"
#include "Components/Displays.h"
#include "Components/GlamOverlay.h"
#include "Components/Keyboard.h"
#include "Components/MusicalTyping.h"
#include "Components/ParamControl.h"
#include "Components/PresetBrowser.h"
#include "Layout/PanelLayout.h"
#include "../Presets/FxPresets.h"

namespace zsynth::ui
{

/** Draws the sections and silkscreen labels of one layout page. */
void paintLayoutPage(juce::Graphics& g, const juce::String& page);

/** A floating, fixed-layout editor page (matrix, FM operators, sequencer steps). */
/** The models the faceplates currently show. */
struct SkinModels { int comp = 0, reverb = 0, eq = 0, tape = 0; };

class PopupPage : public juce::Component
{
public:
    explicit PopupPage(const PageSpec& spec);
    void paint(juce::Graphics&) override;
    const juce::String& getPageId() const { return id; }
    std::function<void()> onClose;
    /** Re-renders the plate (its faceplate follows the selected model). */
    void invalidate() { cache = {}; repaint(); }
    /** Called just before the plate is rendered (sets the faceplate models). */
    std::function<void()> beforeRender;

    // Drag the window by any empty part of its plate (it stays inside the panel).
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Rectangle<int> home;
    juce::String id;
    juce::Image cache;
    float cacheScale = 0.0f;
    juce::TextButton closeButton { juce::String::fromUTF8("\xc3\x97") };
};

/** The MODULATION block's EFFECT selector: picks which effect's controls are
    shown (each effect keeps its own ON, so several can run at once). */
class ModEffectSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModEffectSelector(ZSynthProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void setCurrent(int index);
    int getCurrent() const { return current; }
    std::function<void(int)> onChange;

    static constexpr int kCount = 4;
    static const char* const names[kCount];
    static const char* const views[kCount];
    static const int onParams[kCount];

private:
    ZSynthProcessor& processor;
    int current = 0;
    material::LegendCache legend;
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel(ZSynthProcessor& p);
    ~MainPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override {}
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void showMusicalTyping(bool show);
    bool isMusicalTypingVisible() const { return typingWindow != nullptr && typingWindow->isVisible(); }

    const std::vector<std::unique_ptr<ParamControl>>& getControls() const { return controls; }
    void showPresetBrowser(bool show);
    bool isPresetBrowserVisible() const { return browser.isVisible(); }

    /** Shows one popup editor page ("matrix", "fm", "seq") or hides all (empty). */
    void showPage(const juce::String& page);
    /** Follows the compressor model / reverb type with their faceplates (also run by the timer). */
    void updateSkins();
    /** One refresh tick (displays, activity, faceplates) without the timer: for tools and tests. */
    void refreshNow() { timerCallback(); }
    juce::String getVisiblePage() const;

private:
    void timerCallback() override;
    void buildPages();
    void buildControls();
    void buildUtilities();
    void renderBackground(float scale);
    /** Re-renders one area of the cached background (a faceplate that changed). */
    void renderBackgroundArea(juce::Rectangle<int> area);
    void updateActivity();
    void savePreset();
    void showPresetMenu();
    void showModView(const juce::String& view);
    void updateOscViews(bool force);
    void updateSplit();
    juce::TextButton* partButtons[4] {};
    juce::TextButton* learnButton = nullptr;
    GlamOverlay glamour;
    std::vector<std::pair<int, juce::TextButton*>> blockButtons; // (on param, ADVANCED button)
    ParamControl* trackSwitch = nullptr;
    ParamControl* harmSwitch = nullptr;
    juce::TextButton* detectButton = nullptr;
    int oscViewShown[3] {};
    void showFxPresetMenu(const std::vector<FxPreset>& list, const juce::String& anchor, int onParam);
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
    juce::StringArray controlViews; // parallel to controls: the view each belongs to ("" = always shown)
    std::vector<std::unique_ptr<juce::Component>> ownedUtilities;
    std::vector<Display*> displays;
    std::vector<PerformanceWheel*> wheels;
    std::vector<ParamControl*> stepSliders;
    ZKeyboard* keyboard = nullptr;

    PresetBrowser browser;
    std::unique_ptr<MusicalTyping> typing;
    std::unique_ptr<MusicalTypingWindow> typingWindow;
    juce::TextButton* typingButton = nullptr;
    ModEffectSelector* modSelector = nullptr;
    juce::Label valueBubble;
    juce::Image background;
    float backgroundScale = 0.0f;
    int activityCounter = 0;
    int shownCompModel = -1, shownReverbType = -1, shownEqModel = -1, shownTapeModel = -1;
    SkinModels shownModels() const;
    juce::StringArray activeViews; // views whose layout labels are drawn (e.g. "eq:pultec")
    // PITCH TRACK: the keyboards and the audition clip are disabled while the instrument plays.
    void updateTracking();
    int shownTracking = -1;
    std::vector<juce::Component*> keyboardParts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace zsynth::ui
