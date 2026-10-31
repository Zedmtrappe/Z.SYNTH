#include "MainPanel.h"

#include "Components/AuditionBar.h"
#include "Components/Materials.h"
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
        if (section.startsWith("comp"))                             return juce::Colour(0xff78d4e6); // cyan
        if (section.startsWith("delay") || section.startsWith("reverb")) return juce::Colour(0xfff08fb0); // pink (effects)
        if (section.startsWith("valve"))                                     return juce::Colour(0xffff8c3a); // valve orange
        if (section == "output")                                    return juce::Colour(0xffffffff);
        return Colours::accent;
    }

    // Panel materials (see Components/Materials.h for the lighting model).
    const juce::Colour kChassis   { 0xff0f1012 }; // black textured plastic body
    const juce::Colour kPlate     { 0xff2e3035 }; // anodised, brushed steel section plates
    const juce::Colour kSubPlate  { 0xff1d1f23 }; // recessed sub-panels
    const juce::Colour kHeader    { 0xff1a1b1e }; // header moulding

    void paintSection(juce::Graphics& g, const SectionSpec& s)
    {
        namespace m = material;
        const auto r = s.bounds.toFloat();
        const auto colour = sectionColour(s.id);

        if (s.id == "header")
        {
            m::texturedPlastic(g, r, 0.0f, kHeader, 1.2f);
            // Soft gloss on the moulded top edge.
            juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.07f), 0.0f, r.getY(), juce::Colours::transparentWhite, 0.0f, r.getY() + 34.0f, false);
            g.setGradientFill(gloss);
            g.fillRect(r.withHeight(34.0f));
            // Back-lit amber light bar along the bottom edge.
            const auto bar = juce::Rectangle<float>(r.getX(), r.getBottom() - 3.0f, r.getWidth(), 3.0f);
            m::castShadow(g, juce::Rectangle<float>(r.getX(), r.getBottom() - 2.0f, r.getWidth(), 6.0f), 0.0f, 6.0f, { 0, 3 }, 0.9f);
            juce::DropShadow(Colours::accent.withAlpha(0.55f), 7, {}).drawForRectangle(g, bar.toNearestInt());
            juce::ColourGradient light(Colours::accent.brighter(0.5f), 0.0f, bar.getY(), Colours::accent.darker(0.2f), 0.0f, bar.getBottom(), false);
            g.setGradientFill(light);
            g.fillRect(bar);
            for (auto p : { juce::Point<float>(9.0f, 12.0f), juce::Point<float>(9.0f, 82.0f),
                            juce::Point<float>(r.getRight() - 9.0f, 12.0f), juce::Point<float>(r.getRight() - 9.0f, 82.0f) })
                m::screw(g, p, 4.2f, 0.4f + p.x * 0.01f);
            return;
        }

        if (s.subPanel)
        {
            m::brushedMetal(g, r, 4.0f, kSubPlate, 0.55f);
            m::recess(g, r, 4.0f, juce::Colours::transparentBlack, 0.8f);
            if (s.title.isNotEmpty())
                m::engravedText(g, s.title, r.reduced(7.0f, 2.0f).removeFromTop(14.0f), titleFont(10.5f),
                                juce::Justification::centredLeft, colour.interpolatedWith(m::kLegend, 0.25f), 0.8f);
            return;
        }

        if (s.id == "wheels" || s.id == "keys")
        {
            m::recess(g, r, 5.0f, juce::Colour(0xff09090b), 1.4f);
            return;
        }

        // A raised steel plate standing proud of the chassis.
        m::castShadow(g, r, 6.0f, 12.0f, { 2, 7 }, 0.85f);
        m::brushedMetal(g, r, 6.0f, kPlate, 1.0f);
        m::raisedEdge(g, r, 6.0f, 1.0f);

        if (s.title.isNotEmpty())
        {
            // Coloured back-lit tag, engraved legend and an engraved rule.
            const auto tag = juce::Rectangle<float>(r.getX() + 11.0f, r.getY() + 9.0f, 4.0f, 11.0f);
            juce::DropShadow(colour.withAlpha(0.7f), 6, {}).drawForRectangle(g, tag.toNearestInt());
            g.setColour(colour.brighter(0.2f));
            g.fillRoundedRectangle(tag, 1.0f);
            m::engravedText(g, s.title, juce::Rectangle<float>(r.getX() + 23.0f, r.getY() + 3.0f, r.getWidth() - 30.0f, 23.0f),
                            titleFont(13.0f), juce::Justification::centredLeft, m::kLegend, 0.9f);
            m::engravedLine(g, { r.getX() + 8.0f, r.getY() + 27.0f, r.getRight() - 8.0f, r.getY() + 27.0f });
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
        material::engravedText(g, l.text, l.bounds.toFloat(), labelFont(l.heading ? 11.0f : 10.0f), l.justification,
                               l.heading ? Colours::accent.brighter(0.2f) : material::kLegend.withMultipliedBrightness(0.92f),
                               l.heading ? 0.9f : 0.45f, juce::jmax(1, l.bounds.getHeight() / 13));
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
    // The page is a separate steel plate lifted above the panel; its surface is
    // rendered once per scale like the main panel.
    const float scale = juce::jlimit(0.25f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (cache.isNull() || std::abs(scale - cacheScale) > 0.01f)
    {
        cacheScale = scale;
        cache = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt((float) getWidth() * scale)),
                            juce::jmax(1, juce::roundToInt((float) getHeight() * scale)), true);
        juce::Graphics cg(cache);
        cg.addTransform(juce::AffineTransform::scale(scale));
        paintLayoutPage(cg, id);
        const auto r = getLocalBounds().toFloat();
        cg.setColour(juce::Colours::black);
        cg.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        juce::DropShadow(sectionColour(id).withAlpha(0.35f), 4, {}).drawForPath(cg, [&] {
            juce::Path p;
            p.addRoundedRectangle(r.reduced(1.5f), 5.5f);
            juce::Path stroke;
            juce::PathStrokeType(1.0f).createStrokedPath(stroke, p);
            return stroke;
        }());
    }
    g.drawImage(cache, getLocalBounds().toFloat());
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
    button("openDelay", juce::String::fromUTF8("ADV \xe2\x96\xb8"), "Delay presets, wow, flutter, age, spread and ducking", [this] {
        showPage(getVisiblePage() == "delay" ? juce::String() : juce::String("delay")); });
    button("openReverb", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Reverb presets, decay, low cut, diffusion, modulation, early/late and width", [this] {
        showPage(getVisiblePage() == "reverb" ? juce::String() : juce::String("reverb")); });
    button("delayPresets", juce::String::fromUTF8("DELAY PRESETS \xe2\x96\xbe"), "Load a delay starting point (sets only the delay)", [this] {
        showFxPresetMenu(delayPresets(), "delayPresets", P::DelayOn); });
    button("reverbPresets", juce::String::fromUTF8("REVERB PRESETS \xe2\x96\xbe"), "Load a reverb starting point (sets only the reverb)", [this] {
        showFxPresetMenu(reverbPresets(), "reverbPresets", P::ReverbOn); });
    typingButton = &button("musicalTyping", "TYPING", "Musical typing: play ZYNTH from your computer keyboard (floating window)", [this] {
        showMusicalTyping(! isMusicalTypingVisible()); });
    button("openComp", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Compressor ratio, attack, release, knee, side-chain and colour", [this] {
        showPage(getVisiblePage() == "comp" ? juce::String() : juce::String("comp")); });
    button("openValve", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Valve type, bias, sag, warmth and mix", [this] {
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
    displays.push_back(&addUtility<CompMeter>("compMeter", processor));
    displays.push_back(&addUtility<CompMeter>("compMeterBig", processor));
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
    addUtility<AuditionBar>("auditionBar", processor);
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

void MainPanel::showMusicalTyping(bool show)
{
    if (show && typingWindow == nullptr)
    {
        typing = std::make_unique<MusicalTyping>(processor);
        typingWindow = std::make_unique<MusicalTypingWindow>(*typing);
    }
    if (typingWindow != nullptr)
        typingWindow->setVisible(show);
    if (typingButton != nullptr)
        typingButton->setToggleState(show, juce::dontSendNotification);
}

bool MainPanel::keyStateChanged(bool)
{
    // While the musical typing window is open the keys play notes from here too.
    return isMusicalTypingVisible() && typing->keyStateChanged();
}

bool MainPanel::keyPressed(const juce::KeyPress& k)
{
    if (isMusicalTypingVisible() && typing->isMapped(k))
        return true;
    if (k == juce::KeyPress::escapeKey && getVisiblePage().isNotEmpty())
    {
        showPage({});
        return true;
    }
    return false;
}

// ----------------------------------------------------------------- presets
void MainPanel::showFxPresetMenu(const std::vector<FxPreset>& list, const juce::String& anchor, int onParam)
{
    juce::PopupMenu menu;
    juce::StringArray groups;
    for (const auto& p : list)
        groups.addIfNotAlreadyThere(p.group);
    for (const auto& g : groups)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) list.size(); ++i)
            if (g == list[(size_t) i].group)
                sub.addItem(i + 1, list[(size_t) i].name);
        menu.addSubMenu(g, sub);
    }

    juce::Component::SafePointer<MainPanel> safe(this);
    const auto* listPtr = &list;
    // The anchor button sits on a popup page: offset it by the page's position on the canvas.
    juce::Rectangle<int> target;
    for (const auto& u : PanelLayout::get().utilities())
        if (u.id == anchor)
        {
            target = u.bounds;
            for (const auto& pg : PanelLayout::get().popupPages())
                if (pg.id == u.page)
                    target += pg.boundsOnCanvas.getPosition();
        }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(target)).withMinimumWidth(target.getWidth()),
                       [safe, listPtr, onParam](int result) {
                           if (safe == nullptr || result <= 0)
                               return;
                           auto values = parseFxPreset((*listPtr)[(size_t) (result - 1)]);
                           values.emplace_back(onParam, 1.0f);
                           auto& state = safe->processor.getState();
                           for (const auto& [index, value] : values)
                               if (auto* param = state.getParameter(paramDef(index).id))
                               {
                                   param->beginChangeGesture();
                                   param->setValueNotifyingHost(param->convertTo0to1(value));
                                   param->endChangeGesture();
                               }
                       });
}

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
    if (typingButton != nullptr && typingButton->getToggleState() != isMusicalTypingVisible())
        typingButton->setToggleState(isMusicalTypingVisible(), juce::dontSendNotification);
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

    // The chassis: black textured plastic, slightly darker towards the edges.
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) PanelLayout::kWidth, (float) PanelLayout::kHeight);
    material::texturedPlastic(g, bounds, 0.0f, kChassis, 1.4f);
    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY() * 0.8f,
                                  juce::Colours::black.withAlpha(0.45f), 0.0f, 0.0f, true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);
    paintLayoutPage(g, kMainPage);
}

} // namespace zsynth::ui
