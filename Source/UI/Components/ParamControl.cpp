#include "ParamControl.h"

namespace zsynth::ui
{

namespace
{
    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;
    constexpr float kDragPixels = 220.0f;

    juce::RangedAudioParameter& lookup(juce::AudioProcessorValueTreeState& s, int index)
    {
        auto* p = s.getParameter(paramDef(index).id);
        jassert(p != nullptr);
        return *p;
    }

    /** Screen-reader access to the parameter as a ranged value. */
    class ValueInterface : public juce::AccessibilityRangedNumericValueInterface
    {
    public:
        explicit ValueInterface(ParamControl& c) : control(c) {}
        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return control.getNormalised(); }
        void setValue(double v) override { control.setNormalisedAsGesture((float) v); }
        AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }

    private:
        ParamControl& control;
    };
}

ParamControl::ParamControl(juce::AudioProcessorValueTreeState& s, int paramIndex, ControlKind k,
                           const juce::String& labelText, MidiLearn* learn)
    : state(s), index(paramIndex), kind(k), label(labelText), midiLearn(learn),
      param(lookup(s, paramIndex)),
      attachment(param, [this](float v) { valueChanged(v); }, nullptr)
{
    attachment.sendInitialUpdate();
    if (kind == ControlKind::NoteStrip)
    {
        // "pseq.a.s3.pitch" -> "pseq.a.root" and "pseq.a.s3.gate".
        const juce::String id(paramDef(index).id);
        const auto row = id.upToLastOccurrenceOf(".s", false, false);
        linkedRoot = state.getRawParameterValue(row + ".root");
        linkedGate = state.getRawParameterValue(id.upToLastOccurrenceOf(".", false, false) + ".gate");
        gateParam = state.getParameter(id.upToLastOccurrenceOf(".", false, false) + ".gate");
    }
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle(paramDef(index).name);
    setDescription(paramDef(index).help);
    setRepaintsOnMouseActivity(false);
}

ParamControl::~ParamControl() = default;

void ParamControl::valueChanged(float v)
{
    value = v;
    repaint();
    if (auto* h = getAccessibilityHandler())
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

void ParamControl::setInactive(bool shouldBeInactive, const juce::String& reason)
{
    if (shouldBeInactive == inactive && reason == inactiveReason)
        return;
    inactive = shouldBeInactive;
    inactiveReason = reason;
    setAlpha(inactive ? 0.32f : 1.0f);
}

juce::String ParamControl::getTooltip()
{
    const auto& d = paramDef(index);
    juce::String t = juce::String(d.name) + ":  " + formatValue(index, value) + "\n" + d.help;
    if (inactive && inactiveReason.isNotEmpty())
        t << "\nINACTIVE: " << inactiveReason;
    if (midiLearn != nullptr)
    {
        const int cc = midiLearn->getControllerFor(index);
        if (cc >= 0)
            t << "\nMIDI CC " << cc;
    }
    t << "\nDouble-click: default.  Right-click: MIDI learn.";
    return t;
}

int ParamControl::numSteps() const
{
    const auto& d = paramDef(index);
    return d.type == ParamType::Float ? 0 : (int) std::lround(d.max - d.min);
}

bool ParamControl::isBipolar() const
{
    const auto& d = paramDef(index);
    return d.min < 0.0f && d.max > 0.0f && d.type == ParamType::Float;
}

bool ParamControl::isDragKind() const
{
    return kind == ControlKind::Knob || kind == ControlKind::LargeKnob || kind == ControlKind::SmallKnob
        || kind == ControlKind::VSlider || kind == ControlKind::HSlider
        || kind == ControlKind::Fader || kind == ControlKind::ShortFader;
}

void ParamControl::setNormalisedAsGesture(float normalised)
{
    attachment.setValueAsCompleteGesture(param.convertFrom0to1(juce::jlimit(0.0f, 1.0f, normalised)));
}

void ParamControl::resetToDefault()
{
    attachment.setValueAsCompleteGesture(paramDef(index).def);
}

void ParamControl::stepBy(int steps)
{
    const int n = numSteps();
    if (n > 0)
    {
        const float v = juce::jlimit(paramDef(index).min, paramDef(index).max, std::round(value) + (float) steps);
        attachment.setValueAsCompleteGesture(v);
    }
    else
    {
        setNormalisedAsGesture(getNormalised() + 0.01f * (float) steps);
    }
}

// ------------------------------------------------------------------ mouse/keys
void ParamControl::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    switch (kind)
    {
        case ControlKind::Switch:
        case ControlKind::StepToggle:
            attachment.setValueAsCompleteGesture(value >= 0.5f ? 0.0f : 1.0f);
            return;
        case ControlKind::Selector:
            showChoiceMenu();
            return;
        case ControlKind::Segmented:
        {
            const int n = numSteps() + 1;
            const int seg = juce::jlimit(0, n - 1, (int) ((float) e.x / (float) getWidth() * (float) n));
            attachment.setValueAsCompleteGesture((float) seg);
            return;
        }
        case ControlKind::NoteStrip:
            noteStripClick(e.position);
            return;
        case ControlKind::StepSlider:
            attachment.beginGesture();
            dragging = true;
            attachment.setValueAsPartOfGesture(param.convertFrom0to1(juce::jlimit(0.0f, 1.0f, 1.0f - ((float) e.y - 4.0f) / ((float) getHeight() - 8.0f))));
            return;
        default:
            break;
    }

    dragStartNorm = dragNorm = getNormalised();
    dragging = true;
    attachment.beginGesture();
    if (onDragStateChanged)
        onDragStateChanged(*this, true);
    repaint();
}

void ParamControl::mouseDrag(const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    if (kind == ControlKind::StepSlider)
    {
        attachment.setValueAsPartOfGesture(param.convertFrom0to1(juce::jlimit(0.0f, 1.0f, 1.0f - ((float) e.y - 4.0f) / ((float) getHeight() - 8.0f))));
        return;
    }

    const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
    const float delta = kind == ControlKind::HSlider ? (float) e.getDistanceFromDragStartX() : (float) -e.getDistanceFromDragStartY();
    const bool isFader = kind == ControlKind::VSlider || kind == ControlKind::Fader || kind == ControlKind::ShortFader;
    // Faders track the mouse 1:1 along their slot; knobs use a fixed throw.
    const float pixels = isFader ? juce::jmax(80.0f, (float) getHeight() - 36.0f) : kDragPixels;
    dragNorm = juce::jlimit(0.0f, 1.0f, dragStartNorm + delta / pixels * fine);
    attachment.setValueAsPartOfGesture(param.convertFrom0to1(dragNorm));
    if (onDragStateChanged)
        onDragStateChanged(*this, true);
}

void ParamControl::mouseUp(const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    attachment.endGesture();
    if (onDragStateChanged)
        onDragStateChanged(*this, false);
    repaint();
}

void ParamControl::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && kind != ControlKind::Switch && kind != ControlKind::StepToggle && kind != ControlKind::Selector)
        resetToDefault();
}

void ParamControl::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs(w.deltaY) > std::abs(w.deltaX) ? w.deltaY : -w.deltaX;
    if (d == 0.0f)
        return;
    if (numSteps() > 0)
        stepBy((d > 0.0f) != w.isReversed ? 1 : -1);
    else
        setNormalisedAsGesture(getNormalised() + d * (w.isReversed ? -1.0f : 1.0f) * (e.mods.isShiftDown() ? 0.01f : 0.06f));
}

bool ParamControl::keyPressed(const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) { stepBy(k.getModifiers().isShiftDown() ? 10 : 1); return true; }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) { stepBy(k.getModifiers().isShiftDown() ? -10 : -1); return true; }
    if (code == juce::KeyPress::pageUpKey)   { stepBy(10); return true; }
    if (code == juce::KeyPress::pageDownKey) { stepBy(-10); return true; }
    if (code == juce::KeyPress::homeKey)     { resetToDefault(); return true; }
    if (code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey)
    {
        if (kind == ControlKind::Switch || kind == ControlKind::StepToggle)
        {
            attachment.setValueAsCompleteGesture(value >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (kind == ControlKind::Selector)
        {
            showChoiceMenu();
            return true;
        }
    }
    return false;
}

void ParamControl::showChoiceMenu()
{
    juce::PopupMenu menu;
    const auto& d = paramDef(index);
    menu.addSectionHeader(d.name);
    const int lo = (int) d.min, hi = (int) d.max;
    const juce::String id(d.id);
    const bool tables = id == "wt.table" || id.endsWith(".wt.table");
    if (tables)
    {
        // Wavetables: grouped by category (in the order of first appearance).
        const int offset = id == "wt.table" ? 0 : 1; // per-osc lists start with SECTION
        if (offset == 1)
            menu.addItem(1, formatValue(index, 0.0f), true, (int) std::lround(value) == 0);
        juce::StringArray cats;
        for (int i = lo + offset; i <= hi; ++i)
            cats.addIfNotAlreadyThere(wavetableCategory(i - offset));
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            bool ticked = false;
            for (int i = lo + offset; i <= hi; ++i)
                if (cat == wavetableCategory(i - offset))
                {
                    const bool cur = (int) std::lround(value) == i;
                    ticked = ticked || cur;
                    sub.addItem(i - lo + 1, formatValue(index, (float) i).replace("ROM ", ""), true, cur);
                }
            menu.addSubMenu(cat, sub, true, nullptr, ticked);
        }
    }
    else
    {
        for (int i = lo; i <= hi; ++i)
            menu.addItem(i - lo + 1, formatValue(index, (float) i), true, (int) std::lround(value) == i);
    }

    juce::Component::SafePointer<ParamControl> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
                       [safe, lo](int result) {
                           if (safe != nullptr && result > 0)
                               safe->attachment.setValueAsCompleteGesture((float) (result - 1 + lo));
                       });
}

void ParamControl::showContextMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader(paramDef(index).name);
    if (midiLearn != nullptr)
    {
        const int cc = midiLearn->getControllerFor(index);
        menu.addItem(1, midiLearn->getLearningTarget() == index ? "Waiting for controller... (cancel)" : "MIDI Learn");
        menu.addItem(2, cc >= 0 ? "Clear MIDI CC " + juce::String(cc) : "Clear MIDI mapping", cc >= 0);
    }
    menu.addSeparator();
    menu.addItem(3, "Reset to default (" + formatValue(index, paramDef(index).def) + ")");

    juce::Component::SafePointer<ParamControl> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe](int result) {
        if (safe == nullptr)
            return;
        auto* learn = safe->midiLearn;
        if (result == 1 && learn != nullptr)
        {
            if (learn->getLearningTarget() == safe->index)
                learn->cancelLearning();
            else
                learn->startLearning(safe->index);
        }
        else if (result == 2 && learn != nullptr)
            learn->clearMapping(safe->index);
        else if (result == 3)
            safe->resetToDefault();
        safe->repaint();
    });
}

std::unique_ptr<juce::AccessibilityHandler> ParamControl::createAccessibilityHandler()
{
    const auto role = (kind == ControlKind::Switch || kind == ControlKind::StepToggle) ? juce::AccessibilityRole::toggleButton
                    : (kind == ControlKind::Selector || kind == ControlKind::Segmented) ? juce::AccessibilityRole::comboBox
                    : juce::AccessibilityRole::slider;
    return std::make_unique<juce::AccessibilityHandler>(*this, role, juce::AccessibilityActions(),
                                                        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

// ------------------------------------------------------------------- painting
juce::Colour ParamControl::legendInk() const
{
    if (const auto* s = skin())
        return s->ink;
    if (hasInk)
        return inkOverride;
    return material::kLegend;
}

void ParamControl::paint(juce::Graphics& g)
{
    switch (kind)
    {
        case ControlKind::LargeKnob:  paintKnob(g, 62.0f, true); break;
        case ControlKind::Knob:       paintKnob(g, 40.0f, true); break;
        case ControlKind::SmallKnob:  paintKnob(g, 28.0f, false); break;
        case ControlKind::Selector:   paintSelector(g); break;
        case ControlKind::Switch:     paintSwitch(g); break;
        case ControlKind::VSlider:    paintVSlider(g); break;
        case ControlKind::Fader:
        case ControlKind::ShortFader: paintFader(g); break;
        case ControlKind::StepSlider: paintStepSlider(g); break;
        case ControlKind::StepToggle: paintStepToggle(g); break;
        case ControlKind::HSlider:    paintHSlider(g); break;
        case ControlKind::Segmented:  paintSegmented(g); break;
        case ControlKind::NoteStrip:  paintNoteStrip(g); break;
        default: break;
    }

    if (hasKeyboardFocus(false))
    {
        g.setColour(Colours::accent.withAlpha(0.55f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
    }
    paintLearnOutline(g);
}

void ParamControl::paintLearnOutline(juce::Graphics& g)
{
    if (midiLearn != nullptr && midiLearn->getLearningTarget() == index)
    {
        const float pulse = 0.5f + 0.5f * std::sin((float) juce::Time::getMillisecondCounter() * 0.012f);
        g.setColour(Colours::ledOn.withAlpha(0.4f + 0.6f * pulse));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 2.0f);
    }
}

namespace
{
    namespace m = material;

    /** Small LCD read-out drawn over a legend while a control is touched. */
    void valueReadout(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r)
    {
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRoundedRectangle(r, 2.5f);
        g.setColour(Colours::accentDim.withAlpha(0.6f));
        g.drawRoundedRectangle(r, 2.5f, 0.8f);
        m::glowText(g, text, r.reduced(2.0f, 0.0f), labelFont(9.5f), Colours::lcdText, juce::Justification::centred, 0.8f);
    }

    /** LED lens: dark red glass when off, glowing with a bloom when on. */
    void ledLens(juce::Graphics& g, juce::Rectangle<float> led, bool on, juce::Colour colour)
    {
        if (on)
        {
            juce::ColourGradient bloom(colour.withAlpha(0.55f), led.getCentreX(), led.getCentreY(), colour.withAlpha(0.0f),
                                       led.getCentreX() + led.getWidth() * 1.6f, led.getCentreY(), true);
            g.setGradientFill(bloom);
            g.fillEllipse(led.expanded(led.getWidth() * 1.6f));
        }
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillEllipse(led.expanded(1.0f));
        juce::ColourGradient lens(on ? colour.brighter(0.9f) : colour.withMultipliedBrightness(0.32f), led.getCentreX(), led.getCentreY(),
                                  on ? colour.darker(0.1f) : colour.withMultipliedBrightness(0.12f), led.getRight(), led.getBottom(), true);
        g.setGradientFill(lens);
        g.fillEllipse(led);
        g.setColour(juce::Colours::white.withAlpha(on ? 0.7f : 0.3f));
        g.fillEllipse(led.getX() + led.getWidth() * 0.22f, led.getY() + led.getHeight() * 0.16f, led.getWidth() * 0.32f, led.getHeight() * 0.26f);
    }

    /** A moulded plastic button cap standing on the panel. */
    void buttonCap(juce::Graphics& g, juce::Rectangle<float> r, float radius, bool lit, bool pressed)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(r.expanded(1.5f).translated(0.8f, pressed ? 1.5f : 3.0f), radius + 1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(r.translated(0.5f, pressed ? 0.8f : 1.6f), radius);
        const auto body = r.translated(0.0f, pressed ? 0.8f : 0.0f);
        juce::ColourGradient grad(juce::Colour(0xff4d5058).brighter(lit ? 0.12f : 0.0f), body.getX(), body.getY(),
                                  juce::Colour(0xff17181b), body.getX(), body.getBottom(), false);
        grad.addColour(0.5, juce::Colour(0xff2c2e33));
        g.setGradientFill(grad);
        g.fillRoundedRectangle(body, radius);
        m::raisedEdge(g, body, radius, 0.9f);
    }
}

void ParamControl::paintKnob(juce::Graphics& g, float diameter, bool showLabel)
{
    if (const auto* s = skin(); s != nullptr && s->knob != KnobStyle::Machined)
    {
        paintSkinnedKnob(g, diameter, showLabel, *s);
        return;
    }
    // A machined knob: black knurled skirt with a spun aluminium cap, standing
    // in a back-lit value ring engraved into the panel.
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = showLabel ? 2.0f + diameter * 0.5f : bounds.getCentreY();
    const float rOuter = diameter * 0.5f;
    const float rArc = rOuter - 1.5f;
    const float rBody = rOuter * 0.76f;
    const float norm = getNormalised();
    const float angle = kStartAngle + norm * (kEndAngle - kStartAngle);
    constexpr float lightAngle = -2.4f; // top-left

    if (diameter > 30.0f)
    {
        const int ticks = numSteps() > 0 && numSteps() <= 16 ? numSteps() + 1 : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = kStartAngle + (float) i / (float) (ticks - 1) * (kEndAngle - kStartAngle);
            const juce::Point<float> p1(cx + std::sin(a) * (rOuter + 1.5f), cy - std::cos(a) * (rOuter + 1.5f));
            const juce::Point<float> p2(cx + std::sin(a) * (rOuter + 4.5f), cy - std::cos(a) * (rOuter + 4.5f));
            m::engravedLine(g, { p1, p2 }, 1.0f);
        }
    }

    // Engraved ring groove and the back-lit value arc inside it.
    const float arcWidth = diameter > 50.0f ? 3.5f : (diameter > 30.0f ? 3.0f : 2.2f);
    juce::Path track;
    track.addCentredArc(cx, cy, rArc, rArc, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.strokePath(track, juce::PathStrokeType(arcWidth + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0.0f, 0.8f));
    g.setColour(juce::Colour(0xff060607));
    g.strokePath(track, juce::PathStrokeType(arcWidth + 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float from = isBipolar() ? (kStartAngle + kEndAngle) * 0.5f : kStartAngle;
    if (std::abs(angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, rArc, rArc, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
        g.setColour(accent.withAlpha(0.25f));
        g.strokePath(arc, juce::PathStrokeType(arcWidth + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accent.brighter(0.25f));
        g.strokePath(arc, juce::PathStrokeType(arcWidth - 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cast shadow, down and to the right of the light.
    const auto body = juce::Rectangle<float>(rBody * 2.0f, rBody * 2.0f).withCentre({ cx, cy });
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(body.expanded(3.0f).translated(2.0f, 4.5f));
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(body.expanded(0.8f).translated(1.0f, 2.2f));

    // Skirt: black moulding with knurled grip.
    juce::ColourGradient skirt(juce::Colour(0xff45474d).brighter(hovered ? 0.1f : 0.0f), body.getX(), body.getY(),
                               juce::Colour(0xff09090a), body.getRight(), body.getBottom(), false);
    g.setGradientFill(skirt);
    g.fillEllipse(body);
    const int knurls = diameter > 50.0f ? 60 : (diameter > 30.0f ? 44 : 30);
    for (int k = 0; k < knurls; ++k)
    {
        const float a = (float) k / (float) knurls * juce::MathConstants<float>::twoPi;
        const float facing = std::cos(a - lightAngle);
        const juce::Point<float> o(cx + std::cos(a) * (rBody - 0.5f), cy + std::sin(a) * (rBody - 0.5f));
        const juce::Point<float> i(cx + std::cos(a) * (rBody - 2.6f), cy + std::sin(a) * (rBody - 2.6f));
        g.setColour(k % 2 == 0 ? juce::Colours::white.withAlpha(0.05f + 0.13f * juce::jmax(0.0f, facing))
                               : juce::Colours::black.withAlpha(0.5f));
        g.drawLine({ i, o }, 0.9f);
    }

    // Spun aluminium cap: brightness follows the angle to the light twice per turn.
    const float rCap = rBody * 0.68f;
    const int slices = diameter > 30.0f ? 72 : 40;
    for (int k = 0; k < slices; ++k)
    {
        const float a0 = (float) k / (float) slices * juce::MathConstants<float>::twoPi;
        const float a1 = (float) (k + 1) / (float) slices * juce::MathConstants<float>::twoPi + 0.02f;
        const float mid = 0.5f * (a0 + a1);
        const float b = 0.5f + 0.2f * std::cos(2.0f * (mid - lightAngle)) + 0.1f * std::cos(mid - lightAngle);
        juce::Path wedge;
        wedge.startNewSubPath(cx, cy);
        wedge.lineTo(cx + std::cos(a0) * rCap, cy + std::sin(a0) * rCap);
        wedge.lineTo(cx + std::cos(a1) * rCap, cy + std::sin(a1) * rCap);
        wedge.closeSubPath();
        g.setColour(juce::Colour::greyLevel(juce::jlimit(0.0f, 1.0f, b)).withMultipliedBrightness(hovered ? 1.06f : 1.0f));
        g.fillPath(wedge);
    }
    const auto cap = juce::Rectangle<float>(rCap * 2.0f, rCap * 2.0f).withCentre({ cx, cy });
    juce::ColourGradient capEdge(juce::Colours::white.withAlpha(0.5f), cap.getX(), cap.getY(), juce::Colours::black.withAlpha(0.6f),
                                 cap.getRight(), cap.getBottom(), false);
    g.setGradientFill(capEdge);
    g.drawEllipse(cap, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(cap.expanded(1.0f), 0.8f);

    // Pointer: a black-filled groove across the cap, continued as a white line on the skirt.
    const auto dir = juce::Point<float>(std::sin(angle), -std::cos(angle));
    const juce::Point<float> c(cx, cy);
    g.setColour(juce::Colour(0xff121214));
    g.drawLine({ c + dir * (rCap * 0.15f), c + dir * (rCap - 1.0f) }, diameter > 30.0f ? 2.4f : 1.8f);
    g.setColour(Colours::white);
    g.drawLine({ c + dir * (rCap + 1.0f), c + dir * (rBody - 1.0f) }, diameter > 30.0f ? 2.0f : 1.5f);

    if (showLabel)
    {
        const auto textArea = juce::Rectangle<float>(-6.0f, cy + rOuter + 4.0f, bounds.getWidth() + 12.0f, 14.0f);
        if (dragging || hovered)
            valueReadout(g, getValueText(), textArea.reduced(6.0f, 0.0f));
        else
            legend.draw(g, label, textArea, labelFont(10.0f), juce::Justification::centred, legendInk(), printedLegend() ? 0.15f : 0.7f);
    }
}

void ParamControl::paintSkinnedKnob(juce::Graphics& g, float diameter, bool showLabel, const Skin& s)
{
    // Hardware knobs of the modelled units, over a scale printed on the plate.
    const auto bounds = getLocalBounds().toFloat();
    const bool big = diameter > 50.0f;
    // Big knobs shrink a little to leave room for the printed numbers.
    const float rOuter = diameter * (big ? 0.39f : 0.5f);
    const float cx = bounds.getCentreX();
    const float cy = showLabel ? (big ? 41.0f : 2.0f + rOuter) : bounds.getCentreY();
    const juce::Point<float> c(cx, cy);
    const float angle = kStartAngle + getNormalised() * (kEndAngle - kStartAngle);
    const auto dir = juce::Point<float>(std::sin(angle), -std::cos(angle));

    // Printed scale: ticks, and numbers 0..10 on the big knobs.
    const int steps = numSteps();
    const int ticks = steps > 0 && steps <= 16 ? steps + 1 : 11;
    g.setColour(s.ink.withAlpha(0.8f));
    for (int i = 0; i < ticks; ++i)
    {
        const float a = kStartAngle + (float) i / (float) (ticks - 1) * (kEndAngle - kStartAngle);
        const bool major = ticks != 11 || i % 2 == 0;
        const juce::Point<float> d(std::sin(a), -std::cos(a));
        g.drawLine({ c + d * (rOuter + 1.0f), c + d * (rOuter + (major ? 4.5f : 2.8f)) }, major ? 1.2f : 0.9f);
        if (big && ticks == 11 && major)
        {
            g.setFont(labelFont(8.5f));
            g.drawText(juce::String(i), juce::Rectangle<float>(16.0f, 10.0f).withCentre(c + d * (rOuter + 10.0f)).toNearestInt(),
                       juce::Justification::centred, false);
        }
    }

    // Cast shadow.
    const auto disc = [&](float r) { return juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(c); };
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillEllipse(disc(rOuter * 0.86f).translated(2.0f, 4.0f));
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(disc(rOuter * 0.84f).translated(1.0f, 2.0f));

    const float hover = hovered || dragging ? 0.08f : 0.0f;
    auto glossyDisc = [&](float r, juce::Colour col) {
        const auto d = disc(r);
        juce::ColourGradient body(col.brighter(0.55f + hover), d.getX() + r * 0.4f, d.getY() + r * 0.3f, col.darker(0.6f), d.getRight(), d.getBottom(), true);
        g.setGradientFill(body);
        g.fillEllipse(d);
        juce::ColourGradient rim(juce::Colours::white.withAlpha(0.35f), d.getX(), d.getY(), juce::Colours::black.withAlpha(0.6f), d.getRight(), d.getBottom(), false);
        g.setGradientFill(rim);
        g.drawEllipse(d.reduced(0.5f), 1.0f);
    };
    auto spunDisc = [&](float r, float lo, float hi) {
        // Polished / spun metal: brightness follows the angle to the light twice per turn.
        constexpr float lightAngle = -2.4f;
        const int slices = 56;
        for (int k = 0; k < slices; ++k)
        {
            const float a0 = (float) k / (float) slices * juce::MathConstants<float>::twoPi;
            const float a1 = (float) (k + 1) / (float) slices * juce::MathConstants<float>::twoPi + 0.02f;
            const float mid = 0.5f * (a0 + a1);
            const float b = lo + (hi - lo) * (0.5f + 0.35f * std::cos(2.0f * (mid - lightAngle)) + 0.15f * std::cos(mid - lightAngle));
            juce::Path w;
            w.startNewSubPath(c);
            w.lineTo(c.x + std::cos(a0) * r, c.y + std::sin(a0) * r);
            w.lineTo(c.x + std::cos(a1) * r, c.y + std::sin(a1) * r);
            w.closeSubPath();
            g.setColour(juce::Colour::greyLevel(juce::jlimit(0.0f, 1.0f, b + hover)));
            g.fillPath(w);
        }
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawEllipse(disc(r), 0.9f);
    };
    auto line = [&](float from, float to, juce::Colour col, float w) {
        g.setColour(col);
        g.drawLine({ c + dir * from, c + dir * to }, w);
    };

    switch (s.knob)
    {
        case KnobStyle::BlackRound:
        {
            const float r = rOuter * 0.86f;
            glossyDisc(r, s.knobColour);
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.fillEllipse(disc(r * 0.45f).translated(-r * 0.25f, -r * 0.3f));
            line(r * 0.2f, r - 2.0f, juce::Colours::white, big ? 2.6f : 2.0f);
            break;
        }
        case KnobStyle::Chrome:
        {
            const float rs = rOuter * 0.9f, rb = rOuter * 0.6f;
            spunDisc(rs, 0.45f, 0.95f);
            line(rb, rs - 1.5f, juce::Colour(0xff151515), big ? 2.2f : 1.6f);
            glossyDisc(rb, s.knobColour);
            line(rb * 0.25f, rb - 1.5f, juce::Colours::white, big ? 2.2f : 1.7f);
            break;
        }
        case KnobStyle::Ribbed:
        {
            const float rb = rOuter * 0.8f, rc = rb * 0.62f;
            glossyDisc(rb, juce::Colour(0xff3b3e43));
            const int ribs = big ? 36 : 28;
            for (int k = 0; k < ribs; ++k)
            {
                const float a = (float) k / (float) ribs * juce::MathConstants<float>::twoPi;
                const juce::Point<float> d(std::cos(a), std::sin(a));
                g.setColour(k % 2 == 0 ? juce::Colours::white.withAlpha(0.08f) : juce::Colours::black.withAlpha(0.35f));
                g.drawLine({ c + d * (rc + 1.0f), c + d * (rb - 0.8f) }, 1.0f);
            }
            const auto cap = s.capFor(paramDef(index).id);
            glossyDisc(rc, cap);
            line(rc * 0.15f, rb - 1.5f, cap.getPerceivedBrightness() > 0.6f ? juce::Colour(0xff151515) : juce::Colours::white, big ? 2.2f : 1.7f);
            break;
        }
        case KnobStyle::ChickenHead:
        {
            glossyDisc(rOuter * 0.55f, s.knobColour);
            juce::Path pointer;
            const float R = rOuter * 0.95f;
            pointer.startNewSubPath(0.0f, -R);
            pointer.lineTo(0.22f * R, -0.08f * R);
            pointer.lineTo(0.17f * R, 0.55f * R);
            pointer.lineTo(-0.17f * R, 0.55f * R);
            pointer.lineTo(-0.22f * R, -0.08f * R);
            pointer.closeSubPath();
            const auto place = juce::AffineTransform::rotation(angle).translated(cx, cy);
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillPath(pointer, place.translated(1.5f, 3.0f));
            juce::ColourGradient body(s.knobColour.brighter(0.7f + hover), cx - R * 0.4f, cy - R * 0.5f, s.knobColour.darker(0.5f), cx + R, cy + R, false);
            g.setGradientFill(body);
            g.fillPath(pointer, place);
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.strokePath(pointer, juce::PathStrokeType(0.8f), place);
            line(R * 0.1f, R * 0.85f, juce::Colours::white, big ? 2.0f : 1.5f);
            break;
        }
        case KnobStyle::BlackSkirt:
        {
            const float rs = rOuter * 0.92f, rt = rs * 0.62f;
            glossyDisc(rs, s.knobColour.brighter(0.15f));
            line(rt + 1.0f, rs - 1.5f, juce::Colours::white, big ? 2.2f : 1.7f);
            glossyDisc(rt, s.knobColour);
            const int flutes = big ? 24 : 18;
            for (int k = 0; k < flutes; ++k)
            {
                const float a = (float) k / (float) flutes * juce::MathConstants<float>::twoPi;
                const juce::Point<float> d(std::cos(a), std::sin(a));
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.drawLine({ c + d * (rt * 0.78f), c + d * (rt - 0.8f) }, 1.0f);
            }
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.fillEllipse(disc(rt * 0.5f).translated(-rt * 0.2f, -rt * 0.25f));
            break;
        }
        case KnobStyle::Silver:
        {
            const float rb = rOuter * 0.82f;
            spunDisc(rb, 0.55f, 0.92f);
            for (float k = 0.35f; k < 1.0f; k += 0.16f)
            {
                g.setColour(juce::Colours::black.withAlpha(0.06f));
                g.drawEllipse(disc(rb * k), 0.7f);
            }
            g.setColour(juce::Colour(0xff151515));
            g.fillEllipse(juce::Rectangle<float>(big ? 5.0f : 4.0f, big ? 5.0f : 4.0f).withCentre(c + dir * (rb * 0.72f)));
            break;
        }
        case KnobStyle::Machined:
        default:
            break;
    }

    g.setOpacity(1.0f); // images (the cached legend) are drawn at the current colour's opacity
    if (showLabel)
    {
        const float labelY = big ? bounds.getHeight() - 15.0f : cy + rOuter + 4.0f;
        const auto textArea = juce::Rectangle<float>(-6.0f, labelY, bounds.getWidth() + 12.0f, 14.0f);
        if (dragging || hovered)
            valueReadout(g, getValueText(), textArea.reduced(6.0f, 0.0f));
        else
            legend.draw(g, label, textArea, labelFont(10.0f), juce::Justification::centred, legendInk(), printedLegend() ? 0.15f : 0.7f);
    }
}

void ParamControl::paintSelector(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (label.isNotEmpty())
    {
        legend.draw(g, label, r.removeFromTop(14.0f), labelFont(10.0f), juce::Justification::centredLeft, legendInk(), 0.6f);
        r.removeFromTop(2.0f);
    }
    const auto* s = skin();
    const auto lcdText = s != nullptr ? s->lcdText : Colours::lcdText;
    drawLcd(g, r, s != nullptr ? s->lcdBack : Colours::lcdBack);
    auto arrow = r.withTrimmedLeft(r.getWidth() - 14.0f).reduced(3.0f, r.getHeight() * 0.36f);
    juce::Path p;
    p.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(lcdText.withAlpha(hovered ? 0.9f : 0.45f));
    g.fillPath(p);
    m::glowText(g, getValueText(), r.withTrimmedRight(14.0f).reduced(6.0f, 0.0f), lcdFont(r.getHeight() > 20.0f ? 13.0f : 12.0f),
                lcdText.brighter(hovered ? 0.2f : 0.0f), juce::Justification::centredLeft, 1.0f);
    m::glassSheen(g, r, 3.0f, 1.0f);
}

void ParamControl::paintSwitch(juce::Graphics& g)
{
    const bool on = (value >= 0.5f) != inverted;
    auto r = getLocalBounds().toFloat();
    const bool compact = getHeight() < 24;

    juce::Rectangle<float> button;
    if (compact)
    {
        button = r.reduced(1.0f, 1.5f).withTrimmedBottom(1.0f);
    }
    else
    {
        legend.draw(g, label, r.removeFromTop(14.0f), labelFont(10.0f), juce::Justification::centred, legendInk(), 0.6f);
        r.removeFromTop(2.0f);
        button = r.withSizeKeepingCentre(juce::jmin(r.getWidth(), 40.0f), 20.0f);
    }

    const bool glow = glowWhenOn && on;
    if (glow)
    {
        // A slow (about 2 s) orange pulse around the button.
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float pulse = 0.5f + 0.5f * (float) std::sin(t * juce::MathConstants<double>::twoPi * 0.5);
        for (int k = 3; k >= 1; --k)
        {
            g.setColour(Colours::accent.withAlpha((0.07f + 0.09f * pulse) * (float) (4 - k) / 3.0f));
            g.fillRoundedRectangle(button.expanded((float) k * 1.6f), 3.0f + (float) k);
        }
    }
    buttonCap(g, button, 3.0f, hovered, dragging);
    if (glow)
    {
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float pulse = 0.5f + 0.5f * (float) std::sin(t * juce::MathConstants<double>::twoPi * 0.5);
        g.setColour(Colours::accent.withAlpha(0.10f + 0.12f * pulse));
        g.fillRoundedRectangle(button.reduced(1.0f), 3.0f);
    }
    const float ledSize = compact ? 6.0f : 8.0f;
    const juce::Rectangle<float> led(button.getX() + (compact ? 4.5f : button.getWidth() * 0.5f - ledSize * 0.5f),
                                     button.getCentreY() - ledSize * 0.5f, ledSize, ledSize);
    ledLens(g, led, on, glowWhenOn ? Colours::accent : Colours::ledOn);

    if (compact && glowWhenOn)
    {
        // Plainly printed (not engraved): light grey when off, glowing orange when on.
        const auto area = button.withTrimmedLeft(12.0f).toNearestInt();
        g.setFont(labelFont(10.0f));
        if (on)
        {
            g.setColour(Colours::accent.withAlpha(0.35f));
            for (const auto& d : { juce::Point<int>(-1, 0), juce::Point<int>(1, 0), juce::Point<int>(0, -1), juce::Point<int>(0, 1) })
                g.drawText(label, area.translated(d.x, d.y), juce::Justification::centred, false);
        }
        g.setColour(on ? Colours::accent.brighter(0.25f) : juce::Colour(0xffc2beb6));
        g.drawText(label, area, juce::Justification::centred, false);
    }
    else if (compact)
    {
        // Plainly printed and the same whether on or off (the LED shows the state).
        const auto area = button.withTrimmedLeft(12.0f).toNearestInt();
        g.setFont(labelFont(9.5f));
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawFittedText(label, area.translated(0, 1), juce::Justification::centred, 1, 0.6f);
        g.setColour(m::kLegend);
        g.drawFittedText(label, area, juce::Justification::centred, 1, 0.6f);
    }
}

void ParamControl::paintVSlider(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float top = 6.0f, bottom = r.getHeight() - 22.0f;
    const float cx = r.getCentreX();
    const float norm = getNormalised();
    const float y = bottom - norm * (bottom - top);

    g.setColour(juce::Colour(0xff050506));
    g.fillRoundedRectangle(cx - 2.5f, top - 2.0f, 5.0f, bottom - top + 4.0f, 2.0f);
    g.setColour(accent.withAlpha(0.85f));
    g.fillRoundedRectangle(cx - 1.0f, y, 2.0f, bottom - y + 1.0f, 1.0f);
    buttonCap(g, juce::Rectangle<float>(cx - 11.0f, y - 6.0f, 22.0f, 12.0f), 2.0f, hovered || dragging, false);
    g.setColour(Colours::white);
    g.drawHorizontalLine((int) y, cx - 8.0f, cx + 8.0f);

    const auto area = juce::Rectangle<float>(-8.0f, bottom + 6.0f, r.getWidth() + 16.0f, 14.0f);
    if (dragging || hovered)
        valueReadout(g, getValueText(), area.reduced(8.0f, 0.0f));
    else
        legend.draw(g, label, area, labelFont(10.0f), juce::Justification::centred, m::kLegend, 0.6f);
}

void ParamControl::paintFader(juce::Graphics& g)
{
    // Hardware fader: a slot cut through the panel, lit from inside by the
    // value, with a moulded two-face cap carrying a coloured insert.
    const auto r = getLocalBounds().toFloat();
    const float labelH = 16.0f;
    const float capH = 20.0f;
    const float top = 4.0f + capH * 0.5f;
    const float bottom = r.getHeight() - labelH - 4.0f - capH * 0.5f;
    const float cx = r.getCentreX();
    const float norm = getNormalised();
    const float y = bottom - norm * (bottom - top);
    const auto* s = skin();
    const auto light = s != nullptr ? s->accent : accent;
    const bool lightPlate = s != nullptr && s->lightPlate();

    // Engraved scale either side of the slot (printed on light plates).
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = std::round(bottom - (float) i / 10.0f * (bottom - top)) + 0.5f;
        const float len = (i % 5 == 0) ? 5.0f : 3.0f;
        if (lightPlate)
        {
            g.setColour(s->ink.withAlpha(0.7f));
            g.drawHorizontalLine((int) ty, cx - 9.0f - len, cx - 8.0f);
            g.drawHorizontalLine((int) ty, cx + 8.0f, cx + 9.0f + len);
        }
        else
        {
            m::engravedLine(g, { cx - 9.0f - len, ty, cx - 8.0f, ty }, 1.0f);
            m::engravedLine(g, { cx + 8.0f, ty, cx + 9.0f + len, ty }, 1.0f);
        }
    }
    if (isBipolar())
    {
        const float mid = std::round((top + bottom) * 0.5f) + 0.5f;
        g.setColour(m::kLegend.withAlpha(0.5f));
        g.drawHorizontalLine((int) mid, cx - 15.0f, cx - 10.0f);
        g.drawHorizontalLine((int) mid, cx + 10.0f, cx + 15.0f);
    }

    // The slot: a deep cut with a shadowed upper end and a lit lower lip.
    const auto slot = juce::Rectangle<float>(cx - 2.5f, top - capH * 0.5f + 2.0f, 5.0f, bottom - top + capH - 4.0f);
    g.setColour(juce::Colour(0xff030304));
    g.fillRoundedRectangle(slot, 2.5f);
    juce::ColourGradient lip(juce::Colours::black, 0.0f, slot.getY(), juce::Colours::white.withAlpha(0.16f), 0.0f, slot.getBottom(), false);
    g.setGradientFill(lip);
    g.drawRoundedRectangle(slot.expanded(0.6f), 3.0f, 0.9f);

    // Light from inside the slot up to the value (from centre when bipolar).
    const float from = isBipolar() ? (top + bottom) * 0.5f : bottom;
    const auto trail = juce::Rectangle<float>(cx - 1.0f, juce::jmin(from, y), 2.0f, std::abs(from - y));
    g.setColour(light.withAlpha(0.18f));
    g.fillRect(trail.expanded(1.5f, 0.0f));
    g.setColour(light.withAlpha(0.8f));
    g.fillRect(trail);

    // Cap: soft cast shadow, then the moulding.
    const float capW = juce::jmin(r.getWidth() - 4.0f, 28.0f);
    const auto cap = juce::Rectangle<float>(capW, capH).withCentre({ cx, y });
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(cap.expanded(2.5f).translated(1.5f, 5.0f), 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(cap.expanded(0.5f).translated(0.8f, 2.6f), 3.0f);

    const float lift = hovered || dragging ? 0.12f : 0.0f;
    const auto upper = cap.withHeight(cap.getHeight() * 0.5f - 1.0f);
    const auto lower = cap.withTrimmedTop(cap.getHeight() * 0.5f + 1.0f);
    // Cap moulding: the house grey, or the model's cap colour.
    const auto mould = s != nullptr ? s->faderCap : juce::Colour(0xff4a4d54);
    const auto upTop = s != nullptr ? mould.brighter(0.5f) : juce::Colour(0xff72767e);
    const auto upBottom = s != nullptr ? mould.darker(0.1f) : juce::Colour(0xff3a3d43);
    const auto downTop = s != nullptr ? mould.darker(0.45f) : juce::Colour(0xff2b2d32);
    const auto downBottom = s != nullptr ? mould.darker(1.2f) : juce::Colour(0xff121316);
    g.setColour(s != nullptr ? mould.darker(1.6f) : juce::Colour(0xff101114));
    g.fillRoundedRectangle(cap, 2.5f);
    // Upper face tilts towards the light, the lower face away from it.
    juce::ColourGradient up(upTop.brighter(lift), 0.0f, upper.getY(), upBottom.brighter(lift), 0.0f, upper.getBottom(), false);
    g.setGradientFill(up);
    g.fillRoundedRectangle(upper.reduced(0.6f, 0.6f), 2.0f);
    juce::ColourGradient down(downTop.brighter(lift), 0.0f, lower.getY(), downBottom, 0.0f, lower.getBottom(), false);
    g.setGradientFill(down);
    g.fillRoundedRectangle(lower.reduced(0.6f, 0.6f), 2.0f);
    // Coloured insert in the centre groove (glowing on the house caps, a painted line on model caps).
    const auto insert = juce::Rectangle<float>(cap.getX() + 2.0f, cap.getCentreY() - 1.2f, cap.getWidth() - 4.0f, 2.4f);
    const auto insertColour = s != nullptr ? s->faderInsert : accent;
    if (s == nullptr)
    {
        g.setColour(insertColour.withAlpha(0.3f));
        g.fillRect(insert.expanded(0.0f, 1.2f));
    }
    g.setColour(insertColour.brighter(s == nullptr ? 0.15f : 0.0f));
    g.fillRect(insert);
    // Specular top edge and dark outline.
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawHorizontalLine((int) cap.getY() + 1, cap.getX() + 2.5f, cap.getRight() - 2.5f);
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(cap, 2.5f, 0.9f);

    // Back-lit legend, or the live value while hovering / dragging.
    const auto labelArea = juce::Rectangle<float>(0.0f, r.getHeight() - labelH, r.getWidth(), labelH);
    if (dragging || hovered)
        valueReadout(g, getValueText(), labelArea.reduced(0.0f, 1.0f));
    else
        legend.draw(g, label, labelArea, labelFont(9.0f), juce::Justification::centred, legendInk(), printedLegend() ? 0.15f : 0.6f);
}

void ParamControl::paintStepSlider(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(3.0f, 2.0f);
    g.setColour(highlighted ? Colours::accentDim.withAlpha(0.45f) : juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r, 2.0f);
    const float mid = r.getCentreY();
    g.setColour(Colours::textDim.withAlpha(0.4f));
    g.drawHorizontalLine((int) mid, r.getX(), r.getRight());

    const float v = juce::jlimit(-1.0f, 1.0f, value);
    const float y = mid - v * (r.getHeight() * 0.5f - 2.0f);
    g.setColour(highlighted ? Colours::lcdText : Colours::accent);
    g.fillRect(juce::Rectangle<float>(r.getX() + 4.0f, juce::jmin(y, mid), r.getWidth() - 8.0f, std::abs(y - mid) + 1.0f));
    g.setColour(Colours::white);
    g.fillRect(r.getX() + 2.0f, y - 1.0f, r.getWidth() - 4.0f, 2.0f);
    if (hovered || dragging)
    {
        g.setColour(Colours::lcdText);
        g.setFont(labelFont(9.0f));
        g.drawText(juce::String(v * 12.0f, 1), r.toNearestInt().removeFromTop(12), juce::Justification::centred, false);
    }
}

void ParamControl::paintStepToggle(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(4.0f, 2.0f);
    const bool on = value >= 0.5f;
    g.setColour(on ? ledColour : ledColour.withAlpha(0.12f));
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(on ? ledColour.brighter(0.4f) : Colours::sectionEdge);
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
    if (label.isNotEmpty())
    {
        g.setFont(labelFont(8.5f));
        g.setColour(on ? juce::Colours::black.withAlpha(0.8f) : ledColour.withAlpha(0.55f));
        g.drawFittedText(label, r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ParamControl::paintHSlider(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(2.0f, 3.0f);
    drawLcd(g, r);
    const float mid = r.getCentreX();
    const float v = paramDef(index).min < 0.0f ? value : value * 2.0f - 1.0f;
    const float x = mid + v * (r.getWidth() * 0.5f - 3.0f);
    g.setColour(Colours::textDim.withAlpha(0.5f));
    g.drawVerticalLine((int) mid, r.getY() + 2.0f, r.getBottom() - 2.0f);
    g.setColour(v >= 0.0f ? Colours::accent : Colours::accent.withSaturation(0.3f));
    g.fillRect(juce::Rectangle<float>(juce::jmin(x, mid), r.getY() + 4.0f, std::abs(x - mid), r.getHeight() - 8.0f));
    g.setColour(Colours::lcdText);
    g.setFont(lcdFont(11.0f));
    g.drawText(getValueText(), r.toNearestInt(), juce::Justification::centred, false);
}

void ParamControl::paintSegmented(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const int n = numSteps() + 1;
    const float w = r.getWidth() / (float) n;
    const int current = (int) std::lround(value);
    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float>(r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced(1.0f);
        const bool on = i == current;
        g.setColour(on ? Colours::accent : Colours::knobBottom);
        g.fillRoundedRectangle(seg, 3.0f);
        g.setColour(on ? Colours::accent.brighter(0.3f) : Colours::sectionEdge);
        g.drawRoundedRectangle(seg, 3.0f, 1.0f);
        g.setColour(on ? juce::Colours::black : Colours::textDim);
        g.setFont(labelFont(10.0f));
        g.drawText(formatValue(index, (float) i).toUpperCase(), seg.toNearestInt(), juce::Justification::centred, false);
    }
}

// ------------------------------------------------------------- note strip
// A pattern-sequencer step as a column of twelve lit note buttons (B at the
// top, C at the bottom) between octave up / down arrows, with the note shown
// below. The step's value stays semitones from the row's ROOT (-12..+24).
juce::Rectangle<float> ParamControl::noteArrow(bool up) const
{
    const auto r = getLocalBounds().toFloat().reduced(4.0f, 0.0f);
    return up ? r.withHeight(16.0f) : juce::Rectangle<float>(r.getX(), r.getBottom() - 34.0f, r.getWidth(), 16.0f);
}

juce::Rectangle<float> ParamControl::noteReadout() const
{
    const auto r = getLocalBounds().toFloat().reduced(4.0f, 0.0f);
    return { r.getX(), r.getBottom() - 15.0f, r.getWidth(), 15.0f };
}

juce::Rectangle<float> ParamControl::noteSegment(int pitchClass) const
{
    const auto r = getLocalBounds().toFloat().reduced(4.0f, 0.0f);
    const float top = 19.0f, bottom = r.getBottom() - 37.0f;
    const float h = (bottom - top) / 12.0f;
    return { r.getX(), top + (float) (11 - pitchClass) * h, r.getWidth(), h - 1.5f };
}

void ParamControl::refreshLinked()
{
    if (kind != ControlKind::NoteStrip)
        return;
    const float root = linkedRoot != nullptr ? linkedRoot->load() : 60.0f;
    const float gate = linkedGate != nullptr ? linkedGate->load() : 1.0f;
    if (root != shownRoot || gate != shownGate)
    {
        shownRoot = root;
        shownGate = gate;
        repaint();
    }
}

void ParamControl::noteStripClick(juce::Point<float> p)
{
    const int root = linkedRoot != nullptr ? juce::roundToInt(linkedRoot->load()) : 60;
    const int lo = juce::roundToInt(param.convertFrom0to1(0.0f)), hi = juce::roundToInt(param.convertFrom0to1(1.0f));
    const int note = root + juce::roundToInt(value);
    const bool gate = linkedGate == nullptr || linkedGate->load() >= 0.5f;
    auto setGate = [this](bool on) {
        if (gateParam != nullptr)
        {
            gateParam->beginChangeGesture();
            gateParam->setValueNotifyingHost(on ? 1.0f : 0.0f);
            gateParam->endChangeGesture();
        }
    };
    // The lit note again: the step goes blank (a rest). Any note on a blank step: it plays again.
    for (int pc = 0; pc < 12; ++pc)
        if (noteSegment(pc).expanded(0.0f, 0.75f).contains(p) && pc == ((note % 12) + 12) % 12)
        {
            setGate(! gate);
            if (! gate && onNoteChosen)
                onNoteChosen(note); // back from a rest: hear it
            return;
        }
    if (! gate && ! noteArrow(true).contains(p) && ! noteArrow(false).contains(p))
        setGate(true);
    auto chosen = [this](int n) {
        if (onNoteChosen)
            onNoteChosen(n);
    };
    int target = note;
    if (noteArrow(true).contains(p))
        target = note + 12;
    else if (noteArrow(false).contains(p))
        target = note - 12;
    else
        for (int pc = 0; pc < 12; ++pc)
            if (noteSegment(pc).expanded(0.0f, 0.75f).contains(p))
            {
                // That note, in the octave the step is in (kept within the knob's range).
                target = note - ((note % 12) + 12) % 12 + pc;
                while (target - root > hi)
                    target -= 12;
                while (target - root < lo)
                    target += 12;
            }
    if (target - root < lo || target - root > hi || target == note)
    {
        if (target == note && ! gate)
            chosen(note);
        return;
    }
    attachment.setValueAsCompleteGesture((float) (target - root));
    chosen(target);
}

void ParamControl::paintNoteStrip(juce::Graphics& g)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int root = linkedRoot != nullptr ? juce::roundToInt(linkedRoot->load()) : 60;
    const bool gate = linkedGate == nullptr || linkedGate->load() >= 0.5f;
    const int lo = juce::roundToInt(param.convertFrom0to1(0.0f)), hi = juce::roundToInt(param.convertFrom0to1(1.0f));
    const int note = root + juce::roundToInt(value);
    const int pc = ((note % 12) + 12) % 12;
    const juce::Colour led(0xffff4a2a), amber(0xffffb347);

    // Octave arrows: moulded buttons, dark when the step cannot go further.
    for (bool up : { true, false })
    {
        const auto a = noteArrow(up);
        const bool can = up ? (note + 12 - root <= hi) : (note - 12 - root >= lo);
        juce::ColourGradient body(juce::Colour(0xff45474c), 0.0f, a.getY(), juce::Colour(0xff17181b), 0.0f, a.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(a, 3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(a, 3.0f, 0.8f);
        juce::Path tri;
        const auto c = a.getCentre();
        if (up)
            tri.addTriangle(c.x - 6.0f, c.y + 3.0f, c.x + 6.0f, c.y + 3.0f, c.x, c.y - 4.0f);
        else
            tri.addTriangle(c.x - 6.0f, c.y - 3.0f, c.x + 6.0f, c.y - 3.0f, c.x, c.y + 4.0f);
        g.setColour(can ? amber.withAlpha(hovered ? 1.0f : 0.85f) : juce::Colour(0xff4a4a4a));
        g.fillPath(tri);
    }

    // Twelve note buttons: the step's note lit, the row's root marked.
    g.setFont(labelFont(8.5f));
    for (int k = 0; k < 12; ++k)
    {
        const auto r = noteSegment(k);
        const bool sharp = names[k][1] == '#';
        const bool lit = k == pc && gate; // a rest shows no note
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(r.expanded(0.8f), 2.0f);
        if (lit)
        {
            const auto c = gate ? led : led.withMultipliedBrightness(0.45f);
            g.setColour(c.withAlpha(0.35f));
            g.fillRoundedRectangle(r.expanded(2.0f), 3.0f);
            juce::ColourGradient lens(c.brighter(0.5f), r.getCentreX(), r.getY(), c.darker(0.2f), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill(lens);
            g.fillRoundedRectangle(r, 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.35f).reduced(2.0f, 0.5f), 1.5f);
        }
        else
        {
            g.setColour(sharp ? juce::Colour(0xff1c0f0d) : juce::Colour(0xff2a1512));
            g.fillRoundedRectangle(r, 2.0f);
        }
        g.setColour(lit ? juce::Colours::black.withAlpha(0.85f)
                        : (sharp ? juce::Colour(0xff8a6a60) : juce::Colour(0xffd8c8bc)).withAlpha(0.8f));
        g.drawText(names[k], r.toNearestInt(), juce::Justification::centred, false);
        if (k == ((root % 12) + 12) % 12)
        {
            g.setColour(amber.withAlpha(0.8f)); // the row's ROOT
            g.fillRect(r.getX() + 1.5f, r.getCentreY() - 2.0f, 2.0f, 4.0f);
        }
    }

    // The note itself, on a small red LED readout.
    const auto rd = noteReadout();
    drawLcd(g, rd, juce::Colour(0xff170504));
    material::glowText(g, gate ? juce::MidiMessage::getMidiNoteName(note, true, true, 4) : juce::String("REST"), rd, lcdFont(10.5f),
                gate ? led.brighter(0.2f) : led.withMultipliedBrightness(0.45f), juce::Justification::centred, 0.8f);
}

} // namespace zsynth::ui
