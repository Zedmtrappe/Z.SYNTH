#include "MainPanel.h"

#include "Layout/ActivityRules.h"

namespace zsynth::ui
{

namespace
{
    /** Section colour coding (fader caps and title accents), JD-style. */
    juce::Colour sectionColour(const juce::String& section)
    {
        if (section.startsWith("osc"))                              return juce::Colour(0xff6fb3e8); // blue
        if (section == "wt")                                        return juce::Colour(0xff8fd6c4); // teal
        if (section == "filter")                                    return juce::Colour(0xfff0a53e); // amber
        if (section == "env")                                       return juce::Colour(0xffee6a50); // red
        if (section.startsWith("lfo") || section.startsWith("matrix")) return juce::Colour(0xffb28ce6); // violet
        if (section == "voice")                                     return juce::Colour(0xffd9d4ca); // white
        if (section.startsWith("fm") || section.startsWith("op"))   return juce::Colour(0xff7fd36b); // green
        if (section == "perf" || section.startsWith("seq"))         return juce::Colour(0xffe9d35a); // yellow
        if (section.startsWith("fx"))                               return juce::Colour(0xfff08fb0); // pink
        if (section == "valve")                                     return juce::Colour(0xffff8c3a); // valve orange
        if (section == "output")                                    return juce::Colour(0xffffffff);
        return Colours::accent;
    }

    void paintSection(juce::Graphics& g, const SectionSpec& s)
    {
        const auto r = s.bounds.toFloat();
        const auto colour = sectionColour(s.id);

        if (s.id == "header")
        {
            juce::ColourGradient hg(juce::Colour(0xff2a2c31), 0.0f, 0.0f, juce::Colour(0xff17181b), 0.0f, r.getBottom(), false);
            g.setGradientFill(hg);
            g.fillRect(r);
            g.setColour(Colours::accent);
            g.fillRect(r.getX(), r.getBottom() - 3.0f, r.getWidth(), 3.0f);
            g.setColour(juce::Colours::black);
            g.fillRect(r.getX(), r.getBottom(), r.getWidth(), 1.0f);
            return;
        }

        if (s.subPanel)
        {
            g.setColour(Colours::subPanel);
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine((int) r.getBottom(), r.getX() + 4.0f, r.getRight() - 4.0f);
            if (s.title.isNotEmpty())
            {
                g.setColour(colour.withAlpha(0.9f));
                g.setFont(titleFont(10.5f));
                g.drawText(s.title, r.reduced(6.0f, 2.0f).removeFromTop(13.0f).toNearestInt(), juce::Justification::centredLeft, false);
            }
            return;
        }

        if (s.id == "wheels" || s.id == "keys")
        {
            g.setColour(juce::Colour(0xff0d0d0f));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(Colours::sectionEdge.withAlpha(0.6f));
            g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
            return;
        }

        g.setColour(Colours::sectionShadow.withAlpha(0.8f));
        g.fillRoundedRectangle(r.translated(0.0f, 3.0f), 6.0f);
        juce::ColourGradient body(Colours::section, 0.0f, r.getY(), Colours::sectionLow, 0.0f, r.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(Colours::sectionEdge);
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);

        if (s.title.isNotEmpty())
        {
            const auto bar = r.withHeight(26.0f).reduced(1.0f, 1.0f);
            g.setColour(Colours::titleBar);
            g.fillRoundedRectangle(bar, 4.0f);
            g.fillRect(bar.withTrimmedTop(10.0f));
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawHorizontalLine((int) bar.getBottom(), r.getX() + 1.0f, r.getRight() - 1.0f);
            g.setColour(colour);
            g.fillRect(bar.getX() + 10.0f, bar.getCentreY() - 5.0f, 3.0f, 10.0f);
            g.setColour(Colours::text);
            g.setFont(titleFont(12.5f));
            g.drawText(s.title, bar.withTrimmedLeft(20.0f).toNearestInt(), juce::Justification::centredLeft, false);
        }
    }
}

void paintLayoutPage(juce::Graphics& g, const juce::String& page)
{
    const auto& layout = PanelLayout::get();
    for (const auto& s : layout.sections())
        if (s.page == page)
            paintSection(g, s);

    for (const auto& l : layout.labels())
    {
        if (l.page != page)
            continue;
        g.setColour(l.heading ? Colours::accent.withAlpha(0.95f) : Colours::text);
        g.setFont(labelFont(l.heading ? 11.0f : 10.0f));
        g.drawFittedText(l.text, l.bounds, l.justification, 1, 0.7f);
    }
}

// =============================================================== PopupPage
PopupPage::PopupPage(const PageSpec& spec) : id(spec.id)
{
    setBounds(spec.boundsOnCanvas);
    addAndMakeVisible(closeButton);
    closeButton.setBounds(PanelLayout::get().utility(id + "Close"));
    closeButton.setTooltip("Close (Esc)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    setTitle(spec.title);
}

void PopupPage::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(r, 8.0f);
    paintLayoutPage(g, id);
    g.setColour(sectionColour(id).withAlpha(0.8f));
    g.drawRoundedRectangle(r.reduced(0.75f), 6.0f, 1.5f);
}

// =============================================================== MainPanel
MainPanel::MainPanel(ZSynthProcessor& p) : processor(p), browser(p)
{
    setSize(PanelLayout::kWidth, PanelLayout::kHeight);
    setOpaque(true);
    setWantsKeyboardFocus(true);
    buildPages();
    buildControls();
    buildUtilities();

    valueBubble.setFont(lcdFont(13.0f));
    valueBubble.setJustificationType(juce::Justification::centred);
    valueBubble.setColour(juce::Label::backgroundColourId, Colours::lcdBack);
    valueBubble.setColour(juce::Label::textColourId, Colours::lcdText);
    valueBubble.setColour(juce::Label::outlineColourId, Colours::accentDim);
    valueBubble.setInterceptsMouseClicks(false, false);
    addChildComponent(valueBubble);

    addChildComponent(browser);
    browser.setBounds(460, 120, 1000, 760);
    browser.onClose = [this] { showPresetBrowser(false); };

    updateActivity();
    startTimerHz(30);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::buildPages()
{
    for (const auto& spec : PanelLayout::get().popupPages())
    {
        auto page = std::make_unique<PopupPage>(spec);
        page->onClose = [this] { showPage({}); };
        addChildComponent(*page);
        pages.push_back(std::move(page));
    }
}

juce::Component& MainPanel::parentForPage(const juce::String& page)
{
    for (auto& p : pages)
        if (p->getPageId() == page)
            return *p;
    return *this;
}

void MainPanel::buildControls()
{
    auto& state = processor.getState();
    for (const auto& spec : PanelLayout::get().controls())
    {
        const int index = findParamIndex(spec.paramId);
        jassert(index >= 0);
        if (index < 0)
            continue;
        auto c = std::make_unique<ParamControl>(state, index, spec.kind, spec.label, &processor.getMidiLearn());
        c->setBounds(spec.bounds);
        c->setAccent(sectionColour(spec.section));
        c->onDragStateChanged = [this](ParamControl& control, bool dragging) {
            if (! dragging)
            {
                valueBubble.setVisible(false);
                return;
            }
            const auto text = control.getValueText();
            valueBubble.setText(text, juce::dontSendNotification);
            const int w = juce::jmax(64, (int) juce::GlyphArrangement::getStringWidth(valueBubble.getFont(), text) + 16);
            const auto b = getLocalArea(&control, control.getLocalBounds());
            valueBubble.setBounds(b.getCentreX() - w / 2, b.getY() - 26, w, 22);
            valueBubble.setVisible(true);
            valueBubble.toFront(false);
        };
        if (spec.kind == ControlKind::StepToggle)
        {
            const juce::String id(spec.paramId);
            c->setLedColour(id.endsWith(".acc") ? Colours::ledOn : (id.endsWith(".slide") ? Colours::green : Colours::accent));
        }
        if (spec.kind == ControlKind::StepSlider)
            stepSliders.push_back(c.get());
        parentForPage(spec.page).addAndMakeVisible(*c);
        controls.push_back(std::move(c));
    }
}

void MainPanel::buildUtilities()
{
    addUtility<Logo>("logo");

    auto& display = addUtility<PresetDisplay>("presetDisplay", processor);
    display.onClick = [this] { showPresetMenu(); };
    display.setTooltip("Current preset. Click for the preset menu (bank > folder > preset).");
    displays.push_back(&display);

    auto button = [this](const juce::String& id, const juce::String& text, const juce::String& tip, std::function<void()> fn) -> juce::TextButton& {
        auto& b = addUtility<juce::TextButton>(id, text);
        b.setTooltip(tip);
        b.onClick = std::move(fn);
        return b;
    };
    button("presetPrev", "<", "Previous preset", [this] { processor.getPresets().previous(); });
    button("presetNext", ">", "Next preset", [this] { processor.getPresets().next(); });
    button("presetBrowse", "BROWSE", "Open the preset browser (search, folders, favourites)", [this] { showPresetBrowser(! browser.isVisible()); });
    button("presetSave", "SAVE", "Save: overwrites the current user preset, or asks for a name", [this] { savePreset(); });
    button("presetSaveAs", "SAVE AS", "Save the current sound as a new user preset", [this] { showSaveAsDialog(processor, this); });
    button("presetInit", "INIT", "Load the initial (basic) patch", [this] { processor.getPresets().loadInit(); });
    auto& panic = button("panic", "PANIC", "All notes off: silences every voice and effect tail immediately", [this] { processor.panic(); });
    panic.setColour(juce::TextButton::textColourOffId, Colours::red.brighter(0.3f));

    button("openMatrix", juce::String::fromUTF8("MOD MATRIX \xe2\x96\xb8"), "Open the 8-slot modulation matrix", [this] {
        showPage(getVisiblePage() == "matrix" ? juce::String() : juce::String("matrix")); });
    button("openFm", juce::String::fromUTF8("OPERATORS \xe2\x96\xb8"), "Edit the six FM operators (ratio, level, envelopes)", [this] {
        showPage(getVisiblePage() == "fm" ? juce::String() : juce::String("fm")); });
    button("openSeq", juce::String::fromUTF8("STEPS \xe2\x96\xb8"), "Edit the 16 sequencer steps", [this] {
        showPage(getVisiblePage() == "seq" ? juce::String() : juce::String("seq")); });
    button("openValve", juce::String::fromUTF8("ADVANCED â¸"), "Valve type, bias, sag, warmth and mix", [this] {
        showPage(getVisiblePage() == "valve" ? juce::String() : juce::String("valve")); });

    auto& version = addUtility<juce::Label>("version", "version", "v" + juce::String(JucePlugin_VersionString));
    version.setColour(juce::Label::textColourId, Colours::textDim);
    version.setFont(labelFont(10.0f));
    version.setJustificationType(juce::Justification::centredRight);

    displays.push_back(&addUtility<ActivityLed>("midiLed", processor, true));
    displays.push_back(&addUtility<ActivityLed>("audioLed", processor, false));
    displays.push_back(&addUtility<VoiceCountDisplay>("voiceCount", processor));
    displays.push_back(&addUtility<WavetableDisplay>("wtDisplay", processor));
    displays.push_back(&addUtility<FilterCurveDisplay>("filterCurve", processor));
    displays.push_back(&addUtility<EnvelopeDisplay>("env1Display", processor, (int) P::Env1A));
    displays.push_back(&addUtility<EnvelopeDisplay>("env2Display", processor, (int) P::Env2A));
    displays.push_back(&addUtility<EnvelopeDisplay>("env3Display", processor, (int) P::Env3A));
    displays.push_back(&addUtility<FmAlgorithmDisplay>("fmAlgoDisplay", processor));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo1Display", processor, 0));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo2Display", processor, 1));
    displays.push_back(&addUtility<VoiceLeds>("voiceLeds", processor));
    displays.push_back(&addUtility<OutputInfo>("outputInfo", processor));
    displays.push_back(&addUtility<ValveDisplay>("valveDisplay", processor));
    displays.push_back(&addUtility<OutputMeter>("meter", processor));
    for (int op = 0; op < kNumOperators; ++op)
        displays.push_back(&addUtility<EnvelopeDisplay>("op" + juce::String(op + 1) + "EnvDisplay", processor,
                                                        (int) P::Op1A + op * kOpParamStride));
    auto& seqLeds = addUtility<SeqLeds>("seqLeds", processor);
    seqLeds.setGeometry(62.0f, 44.0f);
    displays.push_back(&seqLeds);

    wheels.push_back(&addUtility<PerformanceWheel>("pitchWheel", processor, true));
    wheels.push_back(&addUtility<PerformanceWheel>("modWheel", processor, false));
    keyboard = &addUtility<ZKeyboard>("keyboard", processor.getKeyboardState());
    button("kbdOctDown", "OCT -", "Shift the on-screen keyboard down one octave", [this] { keyboard->shiftOctave(-1); });
    button("kbdOctUp", "OCT +", "Shift the on-screen keyboard up one octave", [this] { keyboard->shiftOctave(1); });
}

// ------------------------------------------------------------------ pages
void MainPanel::showPage(const juce::String& page)
{
    for (auto& p : pages)
    {
        const bool show = p->getPageId() == page;
        p->setVisible(show);
        if (show)
            p->toFront(false);
    }
    if (page.isNotEmpty())
        browser.setVisible(false);
    valueBubble.toFront(false);
}

juce::String MainPanel::getVisiblePage() const
{
    for (const auto& p : pages)
        if (p->isVisible())
            return p->getPageId();
    return {};
}

bool MainPanel::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && getVisiblePage().isNotEmpty())
    {
        showPage({});
        return true;
    }
    return false;
}

// ----------------------------------------------------------------- presets
void MainPanel::showPresetMenu()
{
    auto& pm = processor.getPresets();
    const auto& entries = pm.getEntries();
    juce::PopupMenu menu;
    for (const auto& bank : pm.getBankNames())
    {
        juce::PopupMenu bankMenu;
        bool bankHasCurrent = false;
        for (const auto& category : pm.getCategories(bank))
        {
            juce::PopupMenu folder;
            bool folderHasCurrent = false;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                const auto& e = entries[(size_t) i];
                if (e.bank == bank && e.category == category)
                {
                    const bool current = i == pm.getCurrentIndex();
                    folder.addItem(i + 1, e.name, true, current);
                    folderHasCurrent = folderHasCurrent || current;
                }
            }
            bankMenu.addSubMenu(category, folder, true, nullptr, folderHasCurrent);
            bankHasCurrent = bankHasCurrent || folderHasCurrent;
        }
        if (bankMenu.getNumItems() == 0)
            bankMenu.addItem(-1, "(empty - use SAVE AS)", false);
        menu.addSubMenu(bank, bankMenu, true, nullptr, bankHasCurrent);
    }
    menu.addSeparator();
    menu.addItem(100000, "Init patch");
    menu.addItem(100001, "Open preset browser...");

    juce::Component::SafePointer<MainPanel> safe(this);
    const auto target = PanelLayout::get().utility("presetDisplay");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(target)).withMinimumWidth(target.getWidth()),
                       [safe](int result) {
                           if (safe == nullptr || result <= 0)
                               return;
                           if (result == 100000)
                               safe->processor.getPresets().loadInit();
                           else if (result == 100001)
                               safe->showPresetBrowser(true);
                           else
                               safe->processor.getPresets().load(result - 1);
                       });
}

void MainPanel::savePreset()
{
    auto& pm = processor.getPresets();
    const int idx = pm.getCurrentIndex();
    if (idx >= 0 && idx < (int) pm.getEntries().size() && ! pm.getEntries()[(size_t) idx].factory)
    {
        const auto r = pm.saveUser(pm.getCurrentName(), pm.getCurrentCategory(), true);
        if (r.failed())
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                                             .withTitle("Save failed").withMessage(r.getErrorMessage()).withButton("OK"), nullptr);
    }
    else
    {
        showSaveAsDialog(processor, this);
    }
}

void MainPanel::showPresetBrowser(bool show)
{
    browser.setVisible(show);
    if (show)
    {
        showPage({});
        browser.toFront(true);
    }
}

// ------------------------------------------------------------------ refresh
void MainPanel::timerCallback()
{
    for (auto* d : displays)
        if (d->isShowing())
            d->refresh();
    for (auto* w : wheels)
        w->refresh();

    const int step = processor.getEngine().getSeqStep();
    const bool seqRunning = std::lround(processor.getValue(P::SeqMode)) != 0;
    for (size_t i = 0; i < stepSliders.size(); ++i)
        stepSliders[i]->setHighlighted(seqRunning && (int) i == step);

    const int learning = processor.getMidiLearn().getLearningTarget();
    for (auto& c : controls)
        if (c->getParamIndex() == learning)
            c->repaint();

    if (++activityCounter % 3 == 0)
        updateActivity();
}

void MainPanel::updateActivity()
{
    float values[kNumParams];
    for (int i = 0; i < kNumParams; ++i)
        values[i] = processor.getValue(i);
    const bool hostTempo = processor.getEngine().isHostTempo();
    for (auto& c : controls)
    {
        const auto a = activityFor(c->getParamIndex(), values, hostTempo);
        c->setInactive(! a.active, a.reason);
    }
}

// ----------------------------------------------------------------- painting
void MainPanel::paint(juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (background.isNull() || std::abs(scale - backgroundScale) > 0.01f)
        renderBackground(scale);
    g.drawImage(background, getLocalBounds().toFloat());
}

void MainPanel::renderBackground(float scale)
{
    backgroundScale = juce::jlimit(0.25f, 3.0f, scale);
    background = juce::Image(juce::Image::RGB, juce::roundToInt(PanelLayout::kWidth * backgroundScale),
                             juce::roundToInt(PanelLayout::kHeight * backgroundScale), true);
    juce::Graphics g(background);
    g.addTransform(juce::AffineTransform::scale(backgroundScale));

    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) PanelLayout::kWidth, (float) PanelLayout::kHeight);
    juce::ColourGradient bg(Colours::panelTop, 0.0f, 0.0f, Colours::panel, 0.0f, bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);
    for (int y = 0; y < PanelLayout::kHeight; y += 3)
    {
        g.setColour(juce::Colours::white.withAlpha((y / 3) % 2 == 0 ? 0.012f : 0.0f));
        g.drawHorizontalLine(y, 0.0f, bounds.getRight());
    }
    paintLayoutPage(g, kMainPage);
}

} // namespace zsynth::ui
