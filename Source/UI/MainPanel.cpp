#include "MainPanel.h"

#include "Components/AuditionBar.h"
#include "Components/Materials.h"
#include "Components/Skins.h"
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
        if (section.startsWith("smooth"))                           return juce::Colour(0xff8fd6c4); // teal
        if (section.startsWith("peq"))                              return juce::Colour(0xffe8c26a); // brass
        if (section.startsWith("tape"))                             return juce::Colour(0xffd9a66b); // tape brown
        if (section.startsWith("comp"))                             return juce::Colour(0xff78d4e6); // cyan
        if (section.startsWith("delay") || section.startsWith("reverb")) return juce::Colour(0xfff08fb0); // pink (effects)
        if (section.startsWith("valve") || section.startsWith("xfmr"))                                     return juce::Colour(0xffff8c3a); // valve orange
        if (section == "output")                                    return juce::Colour(0xffffffff);
        return Colours::accent;
    }

    const Skin* skinForSection(const juce::String& section, const SkinModels& m)
    {
        if (section == "compModule" || section == "compPage")
            return &compSkin((CompModel) m.comp);
        if (section == "fxReverb" || section == "reverbPage")
            return reverbSkin((ReverbType) m.reverb);
        if (section == "peqModule" || section == "peqPage")
            return eqSkin((EqModel) m.eq);
        if (section == "tapeModule" || section == "tapePage")
            return &tapeSkin((TapeModel) m.tape);
        return nullptr;
    }

    // The models the faceplates show, and the views whose labels are drawn, while a
    // panel or page is being rendered (set by the owning panel just before; message thread only).
    SkinModels renderModels;
    juce::StringArray renderViews;
    const Skin* skinForSection(const juce::String& section) { return skinForSection(section, renderModels); }

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

        if (const auto* skin = skinForSection(s.id))
        {
            paintFaceplate(g, r, *skin, s.title, s.subPanel);
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

    // Text on a model faceplate is printed in its ink (not inside recessed sub-panels).
    const Skin* pageSkin = nullptr;
    for (const auto& s : layout.sections())
        if (s.page == page && ! s.subPanel)
            if (const auto* skin = skinForSection(s.id))
                pageSkin = skin;
    auto inSubPanel = [&](juce::Rectangle<int> b) {
        for (const auto& s : layout.sections())
            if (s.page == page && s.subPanel && s.bounds.contains(b.getCentre()))
                return true;
        return false;
    };
    for (const auto& l : layout.labels())
    {
        if (l.page != page || (l.view.isNotEmpty() && ! renderViews.contains(l.view)))
            continue;
        const bool printed = pageSkin != nullptr && ! inSubPanel(l.bounds);
        const auto colour = printed ? (l.heading ? pageSkin->ink : pageSkin->ink.withMultipliedAlpha(0.85f))
                                    : (l.heading ? Colours::accent.brighter(0.2f) : material::kLegend.withMultipliedBrightness(0.92f));
        material::engravedText(g, l.text, l.bounds.toFloat(), labelFont(l.heading ? 11.0f : 10.0f), l.justification, colour,
                               printed ? 0.2f : (l.heading ? 0.9f : 0.45f), juce::jmax(1, l.bounds.getHeight() / 13));
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
    home = spec.boundsOnCanvas;
    constrainer.setMinimumOnscreenAmounts(0xffffff, 0xffffff, 0xffffff, 0xffffff); // fully inside the panel
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void PopupPage::mouseDown(const juce::MouseEvent& e)
{
    toFront(false);
    dragger.startDraggingComponent(this, e);
}

void PopupPage::mouseDrag(const juce::MouseEvent& e) { dragger.dragComponent(this, e, &constrainer); }

void PopupPage::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.y < 28)
        setBounds(home); // double-click the title bar: back to the default place
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
        if (beforeRender)
            beforeRender();
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

// ======================================================= ModEffectSelector
const char* const ModEffectSelector::names[kCount] = { "Chorus", "Ensemble", "Phaser", "Flanger" };
const char* const ModEffectSelector::views[kCount] = { "mod:chorus", "mod:ensemble", "mod:phaser", "mod:flanger" };
const int ModEffectSelector::onParams[kCount] = { P::ChorusOn, P::EnsOn, P::PhaserOn, P::FlangerOn };

ModEffectSelector::ModEffectSelector(ZSynthProcessor& p) : processor(p)
{
    setTooltip("Choose which modulation effect to edit. Each has its own ON, so they can be stacked.");
}

void ModEffectSelector::setCurrent(int index)
{
    current = juce::jlimit(0, kCount - 1, index);
    repaint();
}

void ModEffectSelector::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    legend.draw(g, "EFFECT", r.removeFromTop(14.0f), labelFont(10.0f), juce::Justification::centredLeft, material::kLegend, 0.6f);
    r.removeFromTop(2.0f);
    drawLcd(g, r);
    auto arrow = r.withTrimmedLeft(r.getWidth() - 14.0f).reduced(3.0f, r.getHeight() * 0.36f);
    juce::Path p;
    p.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(Colours::lcdText.withAlpha(0.6f));
    g.fillPath(p);
    material::glowText(g, names[current], r.withTrimmedRight(14.0f).reduced(6.0f, 0.0f), lcdFont(13.0f), Colours::lcdText,
                       juce::Justification::centredLeft);
    material::glassSheen(g, r, 3.0f, 1.0f);
}

void ModEffectSelector::mouseDown(const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int i = 0; i < kCount; ++i)
    {
        const bool on = processor.getValue(onParams[i]) >= 0.5f;
        menu.addItem(i + 1, juce::String(names[i]) + (on ? juce::String::fromUTF8("   \xe2\x97\x8f on") : juce::String()), true, i == current);
    }
    juce::Component::SafePointer<ModEffectSelector> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()), [safe](int result) {
        if (safe == nullptr || result <= 0)
            return;
        safe->setCurrent(result - 1);
        if (safe->onChange)
            safe->onChange(result - 1);
    });
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

    updateSkins();
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
        page->beforeRender = [this] {
            renderModels = shownModels();
            renderViews = activeViews;
        };
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
        if (juce::String(spec.paramId) == "trk.on")
        {
            c->setGlowWhenOn(true);
            trackSwitch = c.get();
        }
        c->setAccent(sectionColour(spec.section));
        {
            const juce::String section(spec.section);
            if (skinForSection(section, SkinModels {}) != nullptr || section.startsWith("peq") || section.startsWith("fxReverb")
                || section == "reverbPage")
                c->setSkinSource([this, section] { return skinForSection(section, shownModels()); });
        }
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
        controlViews.add(spec.view);
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
    button("presetPrev", "<", "Previous preset (within the favourites when one was chosen from the Favourites folder)", [this] { processor.getPresets().previous(); });
    button("presetNext", ">", "Next preset (within the favourites when one was chosen from the Favourites folder)", [this] { processor.getPresets().next(); });
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
    blockButtons.push_back({ P::DelayOn, nullptr });
    blockButtons.back().second = &button("openDelay", juce::String::fromUTF8("ADV \xe2\x96\xb8"), "Delay presets, wow, flutter, age, spread and ducking", [this] {
        showPage(getVisiblePage() == "delay" ? juce::String() : juce::String("delay")); });
    blockButtons.push_back({ P::ReverbOn, nullptr });
    blockButtons.back().second = &button("openReverb", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Reverb presets, decay, low cut, diffusion, modulation, early/late and width", [this] {
        showPage(getVisiblePage() == "reverb" ? juce::String() : juce::String("reverb")); });
    button("delayPresets", juce::String::fromUTF8("DELAY PRESETS \xe2\x96\xbe"), "Load a delay starting point (sets only the delay)", [this] {
        showFxPresetMenu(delayPresets(), "delayPresets", P::DelayOn); });
    button("reverbPresets", juce::String::fromUTF8("REVERB PRESETS \xe2\x96\xbe"), "Load a reverb starting point (sets only the reverb)", [this] {
        showFxPresetMenu(reverbPresets(), "reverbPresets", P::ReverbOn); });
    typingButton = &button("musicalTyping", "QWERTY", "QWERTY keyboard: play ZYNTH from your computer keyboard (floating window)", [this] {
        showMusicalTyping(! isMusicalTypingVisible()); });
    blockButtons.push_back({ P::CompOn, nullptr });
    blockButtons.back().second = &button("openComp", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Compressor ratio, attack, release, knee, side-chain and colour", [this] {
        showPage(getVisiblePage() == "comp" ? juce::String() : juce::String("comp")); });
    auto setValue = [this](int index, float value) {
        if (auto* param = processor.getState().getParameter(paramDef(index).id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(value));
            param->endChangeGesture();
        }
    };
    button("reverbEqFlat", "FLAT", "Reset the reverb EQ to flat (bands to 0 dB, HPF and LPF off)", [setValue] {
        for (int i : { (int) P::RevEqLowGain, (int) P::RevEqB1Gain, (int) P::RevEqB2Gain, (int) P::RevEqHighGain })
            setValue(i, 0.0f);
        setValue(P::ReverbLowCut, 20.0f);
        setValue(P::RevEqLpf, 20000.0f);
    });
    button("peqFlat", "FLAT", "Flatten the EQ model in use: every boost / cut to 0, filters off, drive and output to 0", [this, setValue] {
        auto* model = processor.getState().getRawParameterValue(paramDef(P::EqModel).id);
        const auto m = model != nullptr ? (EqModel) juce::roundToInt(model->load()) : EqModel::Pristine;
        // Gains back to their (flat) defaults; frequency choices are left where they are.
        auto toDefault = [&setValue](std::initializer_list<int> ids) {
            for (int i : ids)
                setValue(i, paramDef(i).def);
        };
        switch (m)
        {
            case EqModel::Pristine:
                for (int i = P::PeqB1Freq; i <= P::PeqB5Q; ++i)
                    if (juce::String(paramDef(i).id).endsWith(".g"))
                        setValue(i, 0.0f);
                setValue(P::PeqHpf, 20.0f);
                setValue(P::PeqLpf, 20000.0f);
                break;
            case EqModel::Pultec:
                toDefault({ P::PulLowBoost, P::PulLowAtten, P::PulHighBoost, P::PulHighAtten });
                break;
            case EqModel::Neve1073:
                toDefault({ P::NeveHpf, P::NeveLowGain, P::NeveMidGain, P::NeveHighGain });
                break;
            case EqModel::Api550A:
                toDefault({ P::A5LowGain, P::A5MidGain, P::A5HighGain, P::A5Bandpass });
                break;
            case EqModel::Api550B:
                toDefault({ P::B5LowGain, P::B5LowMidGain, P::B5HighMidGain, P::B5HighGain });
                break;
            case EqModel::Api560:
                for (int i = P::G560B1; i <= P::G560B10; ++i)
                    setValue(i, 0.0f);
                break;
            default:
                break;
        }
        setValue(P::EqDrive, 0.0f);
        setValue(P::PeqOutput, 0.0f);
    });
    blockButtons.push_back({ P::PeqOn, nullptr });
    blockButtons.back().second = &button("openPeq", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Edit the output EQ: five bands with variable Q, low cut, high cut, output", [this] {
        showPage(getVisiblePage() == "peq" ? juce::String() : juce::String("peq")); });
    button("openWtEngine", juce::String::fromUTF8("ENGINE \xe2\x96\xb8"), "Wavetable engine: a table, warp and stack per oscillator, EVOLVE and DRIFT", [this] {
        showPage(getVisiblePage() == "wtengine" ? juce::String() : juce::String("wtengine")); });
    button("openWave", juce::String::fromUTF8("SEQ / VECTOR \xe2\x96\xb8"), "Wave sequencing and vector mixing (Wavestation style)", [this] {
        showPage(getVisiblePage() == "wave" ? juce::String() : juce::String("wave")); });
    blockButtons.push_back({ P::ValveOn, nullptr });
    blockButtons.back().second = &button("openValve", juce::String::fromUTF8("ADV \xe2\x96\xb8"), "Valve type, bias, sag, warmth and mix", [this] {
        showPage(getVisiblePage() == "valve" ? juce::String() : juce::String("valve")); });

    auto& version = addUtility<juce::Label>("version", "version", "v" + juce::String(JucePlugin_VersionString));
    version.setColour(juce::Label::textColourId, Colours::textDim);
    version.setFont(labelFont(10.0f));
    version.setJustificationType(juce::Justification::centredRight);

    displays.push_back(&addUtility<ActivityLed>("midiLed", processor, true));
    displays.push_back(&addUtility<ActivityLed>("audioLed", processor, false));
    displays.push_back(&addUtility<VoiceCountDisplay>("voiceCount", processor));
    displays.push_back(&addUtility<WavetableDisplay>("wtDisplay", processor));
    displays.push_back(&addUtility<SplitMap>("splitMap", processor));
    for (int i = 0; i < 3; ++i)
        displays.push_back(&addUtility<WavetableView3D>("wt3d" + juce::String(i + 1), processor, i));
    displays.push_back(&addUtility<FilterCurveDisplay>("filterCurve", processor));
    displays.push_back(&addUtility<EnvelopeDisplay>("env1Display", processor, (int) P::Env1A));
    displays.push_back(&addUtility<EnvelopeDisplay>("env2Display", processor, (int) P::Env2A));
    displays.push_back(&addUtility<EnvelopeDisplay>("env3Display", processor, (int) P::Env3A));
    displays.push_back(&addUtility<FmAlgorithmDisplay>("fmAlgoDisplay", processor));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo1Display", processor, 0));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo2Display", processor, 1));
    displays.push_back(&addUtility<VoiceLeds>("voiceLeds", processor));
    displays.push_back(&addUtility<SmoothMeter>("smoothMeter", processor));
    displays.push_back(&addUtility<OutputInfo>("outputInfo", processor));
    auto& peqCurve = addUtility<EqCurve>("peqCurve", processor, EqCurve::Which::Output, false);
    peqCurve.onClick = [this] { showPage(getVisiblePage() == "peq" ? juce::String() : juce::String("peq")); };
    displays.push_back(&peqCurve);
    displays.push_back(&addUtility<EqCurve>("peqCurveBig", processor, EqCurve::Which::Output, true));
    displays.push_back(&addUtility<ValveDisplay>("valveDisplay", processor));
    displays.push_back(&addUtility<VectorPad>("vectorPad", processor));
    modSelector = &addUtility<ModEffectSelector>("modSelector", processor);
    modSelector->onChange = [this](int i) { showModView(ModEffectSelector::views[i]); };
    // Start on the first modulation effect that is switched on.
    for (int i = 0; i < ModEffectSelector::kCount; ++i)
        if (processor.getValue(ModEffectSelector::onParams[i]) >= 0.5f)
        {
            modSelector->setCurrent(i);
            break;
        }
    showModView(ModEffectSelector::views[modSelector->getCurrent()]);
    updateOscViews(true);
    displays.push_back(&addUtility<CompMeter>("compMeter", processor));
    displays.push_back(&addUtility<EqCurve>("reverbEqCurve", processor, EqCurve::Which::Reverb, false));
    displays.push_back(&addUtility<EqCurve>("reverbEqCurveBig", processor, EqCurve::Which::Reverb, true));
    displays.push_back(&addUtility<CompMeter>("compMeterBig", processor));
    displays.push_back(&addUtility<TrackDisplay>("trackDisplay", processor));
    blockButtons.push_back({ P::TapeOn, nullptr });
    blockButtons.back().second = &button("openTape", juce::String::fromUTF8("ADVANCED \xe2\x96\xb8"), "Tape speed, bias, hiss and output, with the transport", [this] {
        showPage(getVisiblePage() == "tape" ? juce::String() : juce::String("tape")); });
    // KEYBOARD split / layer: which part the panel edits.
    for (int part = 0; part < 2; ++part)
    {
        const bool lower = part == ZSynthProcessor::kLower;
        auto edit = [this, part] { processor.setEditPart(part); };
        auto& strip = button(lower ? "partLower" : "partUpper", lower ? "LOWER" : "UPPER",
                             lower ? "Edit the LOWER sound (the panel and preset loads go to it)"
                                   : "Edit the UPPER sound (the panel and preset loads go to it)", edit);
        strip.setClickingTogglesState(false);
        partButtons[part] = &strip;
        auto& big = button(lower ? "editLower" : "editUpper", lower ? "EDIT LOWER" : "EDIT UPPER",
                           lower ? "Put the LOWER sound on the panel" : "Put the UPPER sound on the panel", edit);
        partButtons[2 + part] = &big;
    }
    learnButton = &button("splitLearn", "LEARN", "LEARN: then play the key that should start the UPPER zone", [this] {
        processor.setLearningSplit(! processor.isLearningSplit()); });
    button("openSplit", juce::String::fromUTF8("SPLIT \xe2\x96\xb8"), "Keyboard split / layer: split point, octaves and levels of the two sounds", [this] {
        showPage(getVisiblePage() == "split" ? juce::String() : juce::String("split")); });
    button("openTrack", juce::String::fromUTF8("\xe2\x96\xb8"), "Open the PITCH TRACK window (input, gate, tracking, velocity, dry)", [this] {
        showPage(getVisiblePage() == "track" ? juce::String() : juce::String("track")); });
    displays.push_back(&addUtility<OutputMeter>("meter", processor));
    for (int op = 0; op < kNumOperators; ++op)
        displays.push_back(&addUtility<EnvelopeDisplay>("op" + juce::String(op + 1) + "EnvDisplay", processor,
                                                        (int) P::Op1A + op * kOpParamStride));
    auto& seqLeds = addUtility<SeqLeds>("seqLeds", processor);
    seqLeds.setGeometry(62.0f, 44.0f);
    displays.push_back(&seqLeds);

    wheels.push_back(&addUtility<PerformanceWheel>("pitchWheel", processor, true));
    keyboardParts.push_back(wheels.back()); // pitch bend comes from the strings while tracking
    wheels.push_back(&addUtility<PerformanceWheel>("modWheel", processor, false));
    keyboard = &addUtility<ZKeyboard>("keyboard", processor.getKeyboardState());
    keyboardParts.push_back(&addUtility<AuditionBar>("auditionBar", processor));
    keyboardParts.push_back(&button("kbdOctDown", "OCT -", "Shift the on-screen keyboard down one octave", [this] { keyboard->shiftOctave(-1); }));
    keyboardParts.push_back(&button("kbdOctUp", "OCT +", "Shift the on-screen keyboard up one octave", [this] { keyboard->shiftOctave(1); }));
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

void MainPanel::showModView(const juce::String& view)
{
    // Views are "group:name"; only controls of the same group are affected.
    const auto group = view.upToFirstOccurrenceOf(":", true, false);
    for (size_t i = 0; i < controls.size(); ++i)
        if (controlViews[i].startsWith(group))
            controls[i]->setVisible(controlViews[i] == view);
}

void MainPanel::updateSplit()
{
    const auto mode = (SplitMode) juce::roundToInt(processor.getValue(P::SplitMode));
    const int edit = processor.getEditPart();
    if (keyboard != nullptr)
        keyboard->setSplit(mode == SplitMode::Split ? juce::roundToInt(processor.getValue(P::SplitPoint)) : -1);
    for (int k = 0; k < 4; ++k)
        if (partButtons[k] != nullptr)
        {
            const int part = k % 2;
            partButtons[k]->setToggleState(part == edit, juce::dontSendNotification);
            // The strip buttons show which part is on the panel; dim when SINGLE (still selectable).
            partButtons[k]->setAlpha(mode == SplitMode::Single && part != edit ? 0.55f : 1.0f);
        }
    if (learnButton != nullptr)
    {
        const bool learning = processor.isLearningSplit();
        learnButton->setToggleState(learning && (juce::Time::getMillisecondCounter() / 300) % 2 == 0, juce::dontSendNotification);
        learnButton->setButtonText(learning ? "PLAY KEY" : "LEARN");
    }
}

void MainPanel::updateOscViews(bool force)
{
    static constexpr int waves[3] = { P::Osc1Wave, P::Osc2Wave, P::Osc3Wave };
    for (int i = 0; i < 3; ++i)
    {
        const auto wave = (OscWave) (int) std::lround(processor.getValue(waves[i]));
        const int view = isSuperWave(wave) ? 1 : (wave == OscWave::Wavetable || isRomWave(wave) ? 2 : 0);
        if (force || view != oscViewShown[i])
        {
            oscViewShown[i] = view;
            static const char* names[3] = { ":std", ":super", ":wt" };
            showModView("osc" + juce::String(i + 1) + names[view]);
            showModView("osc" + juce::String(i + 1) + (view == 1 ? "r:mix" : "r:ring"));
        }
    }
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

namespace { constexpr int kFavouriteItemBase = 200000; }

void MainPanel::showPresetMenu()
{
    auto& pm = processor.getPresets();
    const auto& entries = pm.getEntries();
    juce::PopupMenu menu;
    // Favourites first: choosing one makes < > step through the favourites.
    {
        juce::PopupMenu favMenu;
        bool favHasCurrent = false;
        for (int i : pm.getFavouriteIndices())
        {
            const bool current = i == pm.getCurrentIndex();
            const auto& e = entries[(size_t) i];
            favMenu.addItem(kFavouriteItemBase + i, e.name + "   (" + e.category + ")", true, current);
            favHasCurrent = favHasCurrent || current;
        }
        if (favMenu.getNumItems() == 0)
            favMenu.addItem(-1, juce::String::fromUTF8("(none yet: use \"Add to favourites\" or the \xe2\x98\x86 in the browser)"), false);
        menu.addSubMenu(juce::String::fromUTF8("\xe2\x98\x85 Favourites"), favMenu, true, nullptr, favHasCurrent && pm.isFavouritesMode());
        menu.addSeparator();
    }
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
    const int cur = pm.getCurrentIndex();
    if (cur >= 0)
        menu.addItem(100002, pm.isFavourite(cur) ? juce::String::fromUTF8("\xe2\x98\x86 Remove \"") + pm.getCurrentName() + "\" from favourites"
                                                 : juce::String::fromUTF8("\xe2\x98\x85 Add \"") + pm.getCurrentName() + "\" to favourites");
    menu.addItem(100000, "Init patch");
    menu.addItem(100001, "Open preset browser...");

    juce::Component::SafePointer<MainPanel> safe(this);
    const auto target = PanelLayout::get().utility("presetDisplay");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(target)).withMinimumWidth(target.getWidth()),
                       [safe](int result) {
                           if (safe == nullptr || result <= 0)
                               return;
                           auto& presets = safe->processor.getPresets();
                           if (result == 100000)
                               presets.loadInit();
                           else if (result == 100001)
                               safe->showPresetBrowser(true);
                           else if (result == 100002)
                               presets.setFavourite(presets.getCurrentIndex(), ! presets.isFavourite(presets.getCurrentIndex()));
                           else if (result >= kFavouriteItemBase)
                           {
                               presets.setFavouritesMode(true);
                               presets.load(result - kFavouriteItemBase);
                           }
                           else
                           {
                               presets.setFavouritesMode(false);
                               presets.load(result - 1);
                           }
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
    updateOscViews(false);
    updateSplit();

    const int step = processor.getEngine().getSeqStep();
    const bool seqRunning = std::lround(processor.getValue(P::SeqMode)) != 0;
    for (size_t i = 0; i < stepSliders.size(); ++i)
        stepSliders[i]->setHighlighted(seqRunning && (int) i == step);

    const int learning = processor.getMidiLearn().getLearningTarget();
    for (auto& c : controls)
        if (c->getParamIndex() == learning)
            c->repaint();

    updateSkins();
    updateTracking();
    // ADVANCED buttons of switched-off blocks look greyed out (they still open the window).
    for (auto& [param, b] : blockButtons)
        if (b != nullptr)
            b->setAlpha(processor.getValue(param) >= 0.5f ? 1.0f : 0.32f);
    // PITCH TRACK pulses while it is on.
    if (trackSwitch != nullptr && processor.getValue(P::TrkOn) >= 0.5f)
        trackSwitch->repaint();
    if (++activityCounter % 3 == 0)
        updateActivity();
    if (modSelector != nullptr)
    {
        const bool on = processor.getValue(ModEffectSelector::onParams[modSelector->getCurrent()]) >= 0.5f;
        modSelector->setAlpha(on ? 1.0f : 0.32f); // matches the faded controls of a switched-off effect
    }
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

void MainPanel::updateTracking()
{
    const int on = processor.getValue(P::TrkOn) >= 0.5f ? 1 : 0;
    if (on == shownTracking)
        return;
    const bool first = shownTracking < 0;
    shownTracking = on;
    // Switching on opens the window; switching off closes it.
    if (! first)
    {
        if (on)
            showPage("track");
        else if (getVisiblePage() == "track")
            showPage({});
    }
    const bool locked = on && processor.hasAudioInput();
    if (keyboard != nullptr)
    {
        keyboard->setEnabled(! locked);
        keyboard->setAlpha(locked ? 0.35f : 1.0f);
    }
    for (auto* c : keyboardParts)
    {
        c->setEnabled(! locked);
        c->setAlpha(locked ? 0.35f : 1.0f);
    }
    if (locked)
    {
        showMusicalTyping(false);
        processor.getAudition().stop();
        processor.getKeyboardState().allNotesOff(0);
    }
    if (typingButton != nullptr)
    {
        typingButton->setEnabled(! locked);
        typingButton->setTooltip(locked ? "Off while PITCH TRACK plays the synth"
                                        : "QWERTY keyboard: play ZYNTH from your computer keyboard (floating window)");
    }
}

SkinModels MainPanel::shownModels() const
{
    return { juce::jmax(0, shownCompModel), juce::jmax(0, shownReverbType), juce::jmax(0, shownEqModel), juce::jmax(0, shownTapeModel) };
}

void MainPanel::updateSkins()
{
    auto model = [this](int param, int count) { return juce::jlimit(0, count - 1, (int) std::lround(processor.getValue(param))); };
    const int comp = model(P::CompModel, (int) CompModel::Count);
    const int reverb = model(P::ReverbType, (int) ReverbType::Count);
    const int eq = model(P::EqModel, (int) EqModel::Count);
    const int tapeModel = model(P::TapeModelP, (int) TapeModel::Count);
    if (comp == shownCompModel && reverb == shownReverbType && eq == shownEqModel && tapeModel == shownTapeModel)
        return;
    const bool first = shownCompModel < 0;
    const bool compChanged = comp != shownCompModel;
    const bool reverbChanged = first || reverbSkin((ReverbType) reverb) != reverbSkin((ReverbType) juce::jmax(0, shownReverbType));
    const bool eqChanged = eq != shownEqModel;
    const bool tapeChanged = tapeModel != shownTapeModel;
    shownCompModel = comp;
    shownReverbType = reverb;
    shownEqModel = eq;
    shownTapeModel = tapeModel;

    // The EQ model also picks which controls (and headings) its window shows.
    static const char* eqViews[(int) EqModel::Count] = { "eq:pristine", "eq:pultec", "eq:neve", "eq:api550a", "eq:api550b", "eq:api560" };
    if (eqChanged)
    {
        for (int k = activeViews.size(); --k >= 0;)
            if (activeViews[k].startsWith("eq:"))
                activeViews.remove(k);
        activeViews.add(eqViews[eq]);
        showModView(eqViews[eq]);
    }
    if (! compChanged && ! reverbChanged && ! eqChanged && ! tapeChanged)
        return; // e.g. Hall -> Room: both the house look

    // Re-render the faceplates (their area of the panel background, and the pages) and the controls on them.
    const std::pair<const char*, bool> modules[4] = { { "compModule", compChanged }, { "fxReverb", reverbChanged },
                                                      { "peqModule", eqChanged }, { "tapeModule", tapeChanged } };
    for (const auto& [id, changed] : modules)
    {
        if (! changed)
            continue;
        const auto area = PanelLayout::get().section(id).expanded(4);
        renderBackgroundArea(area);
        repaint(area);
    }
    for (auto& page : pages)
    {
        const auto& id = page->getPageId();
        if ((id == "comp" && compChanged) || (id == "reverb" && reverbChanged) || (id == "peq" && eqChanged) || (id == "tape" && tapeChanged))
            page->invalidate();
    }
    for (auto& c : controls)
        if (c->hasSkinSource())
            c->repaint();
}

void MainPanel::renderBackgroundArea(juce::Rectangle<int> area)
{
    if (background.isNull())
        return; // rendered in full on the next paint
    juce::Graphics g(background);
    g.addTransform(juce::AffineTransform::scale(backgroundScale));
    g.reduceClipRegion(area);
    renderModels = shownModels();
    renderViews = activeViews;
    // The module bays sit in the EFFECTS plate: repaint that plate under them, then the bay.
    const auto& layout = PanelLayout::get();
    for (const auto& s : layout.sections())
        if (s.page == kMainPage && s.bounds.intersects(area))
            paintSection(g, s);
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
    renderModels = shownModels();
    renderViews = activeViews;

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
