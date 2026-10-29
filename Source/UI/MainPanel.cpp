#include "MainPanel.h"

#include "Layout/ActivityRules.h"

namespace zsynth::ui
{

MainPanel::MainPanel(ZSynthProcessor& p) : processor(p), browser(p)
{
    setSize(PanelLayout::kWidth, PanelLayout::kHeight);
    setOpaque(true);
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
        c->onDragStateChanged = [this](ParamControl& control, bool dragging) {
            if (! dragging)
            {
                valueBubble.setVisible(false);
                return;
            }
            valueBubble.setText(control.getValueText(), juce::dontSendNotification);
            const int w = juce::jmax(64, (int) juce::GlyphArrangement::getStringWidth(valueBubble.getFont(), control.getValueText()) + 16);
            auto b = control.getBounds();
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
        addAndMakeVisible(*c);
        controls.push_back(std::move(c));
    }
}

void MainPanel::buildUtilities()
{
    addUtility<Logo>("logo");

    auto& display = addUtility<PresetDisplay>("presetDisplay", processor);
    display.onClick = [this] { showPresetBrowser(! browser.isVisible()); };
    displays.push_back(&display);

    auto button = [this](const juce::String& id, const juce::String& text, const juce::String& tip, std::function<void()> fn) -> juce::TextButton& {
        auto& b = addUtility<juce::TextButton>(id, text);
        b.setTooltip(tip);
        b.onClick = std::move(fn);
        return b;
    };
    button("presetPrev", "<", "Previous preset", [this] { processor.getPresets().previous(); });
    button("presetNext", ">", "Next preset", [this] { processor.getPresets().next(); });
    button("presetBrowse", "BROWSE", "Open the preset browser (search, categories, favourites)", [this] { showPresetBrowser(! browser.isVisible()); });
    button("presetSave", "SAVE", "Save: overwrites the current user preset, or asks for a name", [this] { savePreset(); });
    button("presetSaveAs", "SAVE AS", "Save the current sound as a new user preset", [this] { showSaveAsDialog(processor, this); });
    button("presetInit", "INIT", "Load the initial (basic) patch", [this] { processor.getPresets().loadInit(); });
    auto& panic = button("panic", "PANIC", "All notes off: silences every voice and effect tail immediately", [this] { processor.panic(); });
    panic.setColour(juce::TextButton::textColourOffId, Colours::red.brighter(0.3f));

    auto& version = addUtility<juce::Label>("version", "version", "v" + juce::String(JucePlugin_VersionString));
    version.setColour(juce::Label::textColourId, Colours::textDim);
    version.setFont(labelFont(10.0f));
    version.setJustificationType(juce::Justification::centredRight);

    displays.push_back(&addUtility<ActivityLed>("midiLed", processor, true));
    displays.push_back(&addUtility<ActivityLed>("audioLed", processor, false));
    displays.push_back(&addUtility<VoiceCountDisplay>("voiceCount", processor));
    displays.push_back(&addUtility<WavetableDisplay>("wtDisplay", processor));
    displays.push_back(&addUtility<FilterModelBadge>("filterDisplay", processor));
    displays.push_back(&addUtility<FilterCurveDisplay>("filterCurve", processor));
    for (int e = 0; e < 3; ++e)
        displays.push_back(&addUtility<EnvelopeDisplay>("env" + juce::String(e + 1) + "Display", processor, e));
    displays.push_back(&addUtility<FmAlgorithmDisplay>("fmAlgoDisplay", processor));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo1Display", processor, 0));
    displays.push_back(&addUtility<LfoShapeDisplay>("lfo2Display", processor, 1));
    displays.push_back(&addUtility<VoiceLeds>("voiceLeds", processor));
    displays.push_back(&addUtility<ArpDisplay>("arpDisplay", processor));
    displays.push_back(&addUtility<OutputInfo>("outputInfo", processor));
    displays.push_back(&addUtility<OutputMeter>("meter", processor));
    displays.push_back(&addUtility<StereoDisplay>("stereoScope", processor));
    seqLeds = &addUtility<SeqLeds>("seqLeds", processor);
    displays.push_back(seqLeds);

    wheels.push_back(&addUtility<PerformanceWheel>("pitchWheel", processor, true));
    wheels.push_back(&addUtility<PerformanceWheel>("modWheel", processor, false));
    keyboard = &addUtility<ZKeyboard>("keyboard", processor.getKeyboardState());
    button("kbdOctDown", "OCT -", "Shift the on-screen keyboard down one octave", [this] { keyboard->shiftOctave(-1); });
    button("kbdOctUp", "OCT +", "Shift the on-screen keyboard up one octave", [this] { keyboard->shiftOctave(1); });
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
        browser.toFront(true);
}

// ------------------------------------------------------------------ refresh
void MainPanel::timerCallback()
{
    for (auto* d : displays)
        d->refresh();
    for (auto* w : wheels)
        w->refresh();

    const int step = processor.getEngine().getSeqStep();
    const bool seqRunning = std::lround(processor.getValue(P::SeqMode)) != 0;
    for (size_t i = 0; i < stepSliders.size(); ++i)
        stepSliders[i]->setHighlighted(seqRunning && (int) i == step);

    // MIDI-learn outline pulses; repaint the target only.
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
    paintBackground(g);
}

void MainPanel::paintBackground(juce::Graphics& g)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) PanelLayout::kWidth, (float) PanelLayout::kHeight);
    juce::ColourGradient bg(Colours::panelTop, 0.0f, 0.0f, Colours::panel, 0.0f, bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);

    // Fine horizontal brushing.
    for (int y = 0; y < PanelLayout::kHeight; y += 3)
    {
        g.setColour(juce::Colours::white.withAlpha((y / 3) % 2 == 0 ? 0.012f : 0.0f));
        g.drawHorizontalLine(y, 0.0f, bounds.getRight());
    }

    const auto& layout = PanelLayout::get();
    for (const auto& s : layout.sections())
    {
        const auto r = s.bounds.toFloat();
        if (s.id == "header")
        {
            juce::ColourGradient hg(juce::Colour(0xff26282d), 0.0f, 0.0f, juce::Colour(0xff1a1b1f), 0.0f, r.getBottom(), false);
            g.setGradientFill(hg);
            g.fillRect(r);
            g.setColour(Colours::accent.withAlpha(0.8f));
            g.fillRect(r.getX(), r.getBottom() - 2.0f, r.getWidth(), 2.0f);
            g.setColour(juce::Colours::black);
            g.fillRect(r.getX(), r.getBottom(), r.getWidth(), 1.0f);
            continue;
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
                g.setColour(Colours::accent.withAlpha(0.85f));
                g.setFont(titleFont(10.5f));
                g.drawText(s.title, r.reduced(6.0f, 2.0f).removeFromTop(13.0f).toNearestInt(), juce::Justification::centredLeft, false);
            }
            continue;
        }

        // Main section: shadow, body, bevel, title bar and corner screws.
        g.setColour(Colours::sectionShadow.withAlpha(0.8f));
        g.fillRoundedRectangle(r.translated(0.0f, 3.0f), 7.0f);
        juce::ColourGradient body(Colours::section, 0.0f, r.getY(), Colours::sectionLow, 0.0f, r.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(Colours::sectionEdge);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 6.0f, r.getRight() - 6.0f);

        if (s.title.isNotEmpty())
        {
            const auto bar = r.withHeight(26.0f).reduced(1.0f, 1.0f);
            g.setColour(Colours::titleBar);
            g.fillRoundedRectangle(bar, 5.0f);
            g.fillRect(bar.withTrimmedTop(10.0f));
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawHorizontalLine((int) bar.getBottom(), r.getX() + 1.0f, r.getRight() - 1.0f);
            g.setColour(Colours::accent);
            g.fillRect(bar.getX() + 10.0f, bar.getCentreY() - 5.0f, 3.0f, 10.0f);
            g.setFont(titleFont(12.5f));
            g.drawText(s.title, bar.withTrimmedLeft(20.0f).toNearestInt(), juce::Justification::centredLeft, false);
        }

        for (auto corner : { r.getBottomLeft().translated(7.0f, -7.0f), r.getBottomRight().translated(-7.0f, -7.0f) })
        {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillEllipse(corner.x - 3.5f, corner.y - 3.0f, 7.0f, 7.0f);
            g.setColour(juce::Colour(0xff4b4e55));
            g.fillEllipse(corner.x - 3.0f, corner.y - 3.0f, 6.0f, 6.0f);
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawLine(corner.x - 2.0f, corner.y - 2.0f, corner.x + 2.0f, corner.y + 2.0f, 1.0f);
        }
    }

    // Static labels from the layout spec.
    for (const auto& l : layout.labels())
    {
        const bool heading = l.text.startsWith("OSC") || l.text.startsWith("LFO") || l.text.startsWith("OP")
                          || l.text.contains("AMP") || l.text.contains("FILTER") || l.text.contains("MOD");
        g.setColour(heading ? Colours::accent.withAlpha(0.9f) : Colours::text);
        g.setFont(labelFont(heading ? 11.0f : 9.5f));
        g.drawFittedText(l.text, l.bounds, l.justification, 1, 0.7f);
    }

    // Divider between the oscillator rows and the sub/noise row.
    const auto div = layout.utility("oscDivider").toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(div.withHeight(1.0f));
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRect(div.withHeight(1.0f).translated(0.0f, 1.0f));
}

} // namespace zsynth::ui
