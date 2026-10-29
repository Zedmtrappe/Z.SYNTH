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
    for (int i = lo; i <= hi; ++i)
        menu.addItem(i - lo + 1, formatValue(index, (float) i), true, (int) std::lround(value) == i);

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

void ParamControl::paintKnob(juce::Graphics& g, float diameter, bool showLabel)
{
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = showLabel ? 2.0f + diameter * 0.5f : bounds.getCentreY();
    const float rOuter = diameter * 0.5f;
    const float rArc = rOuter - 1.5f;
    const float rBody = rOuter * 0.74f;
    const float norm = getNormalised();
    const float angle = kStartAngle + norm * (kEndAngle - kStartAngle);

    // Scale ticks.
    if (diameter > 30.0f)
    {
        g.setColour(Colours::textDim.withAlpha(0.55f));
        const int ticks = numSteps() > 0 && numSteps() <= 16 ? numSteps() + 1 : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = kStartAngle + (float) i / (float) (ticks - 1) * (kEndAngle - kStartAngle);
            const juce::Point<float> p1(cx + std::sin(a) * (rOuter + 1.5f), cy - std::cos(a) * (rOuter + 1.5f));
            const juce::Point<float> p2(cx + std::sin(a) * (rOuter + 4.0f), cy - std::cos(a) * (rOuter + 4.0f));
            g.drawLine({ p1, p2 }, 1.0f);
        }
    }

    // Track and value arc.
    const float arcWidth = diameter > 50.0f ? 3.5f : (diameter > 30.0f ? 3.0f : 2.2f);
    juce::Path track;
    track.addCentredArc(cx, cy, rArc, rArc, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour(Colours::track);
    g.strokePath(track, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = isBipolar() ? (kStartAngle + kEndAngle) * 0.5f : kStartAngle;
    if (std::abs(angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, rArc, rArc, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
        g.setColour(Colours::accent);
        g.strokePath(arc, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Body: shadow, metal cap, edge highlight.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - rBody, cy - rBody + 2.0f, rBody * 2.0f, rBody * 2.0f);
    juce::ColourGradient body(Colours::knobTop.brighter(hovered ? 0.12f : 0.0f), cx, cy - rBody,
                              Colours::knobBottom, cx, cy + rBody, false);
    g.setGradientFill(body);
    g.fillEllipse(cx - rBody, cy - rBody, rBody * 2.0f, rBody * 2.0f);
    g.setColour(Colours::knobEdge);
    g.drawEllipse(cx - rBody, cy - rBody, rBody * 2.0f, rBody * 2.0f, 1.0f);
    const float rCap = rBody * 0.72f;
    juce::ColourGradient cap(Colours::knobBottom.brighter(0.25f), cx, cy + rCap, Colours::knobTop.darker(0.1f), cx, cy - rCap, false);
    g.setGradientFill(cap);
    g.fillEllipse(cx - rCap, cy - rCap, rCap * 2.0f, rCap * 2.0f);

    // Pointer.
    const juce::Point<float> tip(cx + std::sin(angle) * (rBody - 2.0f), cy - std::cos(angle) * (rBody - 2.0f));
    const juce::Point<float> base(cx + std::sin(angle) * rBody * 0.25f, cy - std::cos(angle) * rBody * 0.25f);
    g.setColour(Colours::white);
    g.drawLine({ base, tip }, diameter > 30.0f ? 2.2f : 1.6f);

    if (showLabel)
    {
        const auto textArea = juce::Rectangle<float>(0.0f, cy + rOuter + 4.0f, bounds.getWidth(), 14.0f);
        if (dragging || hovered)
        {
            g.setColour(Colours::lcdText);
            g.setFont(labelFont(10.5f));
            g.drawFittedText(getValueText(), textArea.toNearestInt().expanded(6, 0), juce::Justification::centred, 1, 0.7f);
        }
        else
        {
            g.setColour(Colours::text);
            g.setFont(labelFont(10.0f));
            g.drawFittedText(label, textArea.toNearestInt().expanded(6, 0), juce::Justification::centred, 1, 0.7f);
        }
    }
}

void ParamControl::paintSelector(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (label.isNotEmpty())
    {
        g.setColour(Colours::text);
        g.setFont(labelFont(10.0f));
        g.drawText(label, r.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft, true);
        r.removeFromTop(2.0f);
    }
    drawLcd(g, r);
    if (hovered)
    {
        g.setColour(Colours::accent.withAlpha(0.35f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
    }
    auto arrow = r.removeFromRight(14.0f).reduced(3.0f, r.getHeight() * 0.36f);
    juce::Path p;
    p.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(Colours::lcdDim.brighter(0.4f));
    g.fillPath(p);
    g.setColour(Colours::lcdText);
    g.setFont(lcdFont(r.getHeight() > 20.0f ? 13.0f : 12.0f));
    g.drawFittedText(getValueText(), r.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
}

void ParamControl::paintSwitch(juce::Graphics& g)
{
    const bool on = value >= 0.5f;
    auto r = getLocalBounds().toFloat();
    const bool compact = getHeight() < 24;

    juce::Rectangle<float> button;
    if (compact)
    {
        button = r.reduced(0.5f);
    }
    else
    {
        g.setColour(Colours::text);
        g.setFont(labelFont(10.0f));
        g.drawFittedText(label, r.removeFromTop(14.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        r.removeFromTop(2.0f);
        button = r.withSizeKeepingCentre(juce::jmin(r.getWidth(), 40.0f), 20.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(button.translated(0.0f, 1.0f), 3.0f);
    juce::ColourGradient grad(Colours::knobTop.brighter(hovered ? 0.12f : 0.0f), button.getX(), button.getY(),
                              Colours::knobBottom, button.getX(), button.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(button, 3.0f);
    g.setColour(Colours::knobEdge);
    g.drawRoundedRectangle(button, 3.0f, 1.0f);

    // LED
    const float ledSize = compact ? 6.0f : 8.0f;
    const juce::Rectangle<float> led(button.getX() + (compact ? 4.0f : button.getWidth() * 0.5f - ledSize * 0.5f),
                                     button.getCentreY() - ledSize * 0.5f, ledSize, ledSize);
    if (on)
    {
        g.setColour(Colours::ledOn.withAlpha(0.35f));
        g.fillEllipse(led.expanded(3.0f));
    }
    g.setColour(on ? Colours::ledOn : Colours::ledOff);
    g.fillEllipse(led);

    if (compact)
    {
        g.setColour(on ? Colours::text : Colours::textDim);
        g.setFont(labelFont(9.0f));
        g.drawFittedText(label, button.withTrimmedLeft(12.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ParamControl::paintVSlider(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float top = 6.0f, bottom = r.getHeight() - 22.0f;
    const float cx = r.getCentreX();
    const float norm = getNormalised();
    const float y = bottom - norm * (bottom - top);

    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRoundedRectangle(cx - 2.5f, top - 2.0f, 5.0f, bottom - top + 4.0f, 2.0f);
    g.setColour(Colours::accent.withAlpha(0.85f));
    g.fillRoundedRectangle(cx - 1.5f, y, 3.0f, bottom - y + 1.0f, 1.5f);

    const juce::Rectangle<float> cap(cx - 11.0f, y - 6.0f, 22.0f, 12.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 2.0f);
    juce::ColourGradient grad(Colours::knobTop.brighter(hovered || dragging ? 0.2f : 0.05f), cap.getX(), cap.getY(), Colours::knobBottom, cap.getX(), cap.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(cap, 2.0f);
    g.setColour(Colours::white);
    g.drawHorizontalLine((int) cap.getCentreY(), cap.getX() + 3.0f, cap.getRight() - 3.0f);

    g.setColour(dragging || hovered ? Colours::lcdText : Colours::text);
    g.setFont(labelFont(10.0f));
    g.drawText(dragging || hovered ? getValueText() : label,
               juce::Rectangle<float>(-8.0f, bottom + 6.0f, r.getWidth() + 16.0f, 14.0f).toNearestInt(), juce::Justification::centred, false);
}

void ParamControl::paintFader(juce::Graphics& g)
{
    // Hardware-style fader: recessed slot with scale marks, a dark cap with a
    // coloured stripe (section colour), and the label (or value) below.
    const auto r = getLocalBounds().toFloat();
    const float labelH = 16.0f;
    const float capH = 18.0f;
    const float top = 4.0f + capH * 0.5f;
    const float bottom = r.getHeight() - labelH - 4.0f - capH * 0.5f;
    const float cx = r.getCentreX();
    const float norm = getNormalised();
    const float y = bottom - norm * (bottom - top);

    // Scale marks either side of the slot.
    g.setColour(Colours::textDim.withAlpha(0.45f));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = bottom - (float) i / 10.0f * (bottom - top);
        const float len = (i % 5 == 0) ? 5.0f : 3.0f;
        g.drawHorizontalLine((int) ty, cx - 9.0f - len, cx - 8.0f);
        g.drawHorizontalLine((int) ty, cx + 8.0f, cx + 9.0f + len);
    }
    if (isBipolar())
    {
        g.setColour(Colours::text.withAlpha(0.6f));
        const float mid = (top + bottom) * 0.5f;
        g.drawHorizontalLine((int) mid, cx - 15.0f, cx + 15.0f);
    }

    // Slot.
    const auto slot = juce::Rectangle<float>(cx - 2.5f, top - capH * 0.5f + 2.0f, 5.0f, bottom - top + capH - 4.0f);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(slot, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawVerticalLine((int) slot.getRight(), slot.getY() + 2.0f, slot.getBottom() - 2.0f);

    // Value trail in the slot (from zero, or from centre when bipolar).
    const float from = isBipolar() ? (top + bottom) * 0.5f : bottom;
    g.setColour(accent.withAlpha(0.55f));
    g.fillRect(juce::Rectangle<float>(cx - 1.0f, juce::jmin(from, y), 2.0f, std::abs(from - y)));

    // Cap.
    const float capW = juce::jmin(r.getWidth() - 4.0f, 28.0f);
    const auto cap = juce::Rectangle<float>(capW, capH).withCentre({ cx, y });
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(cap.translated(0.0f, 2.5f), 2.5f);
    juce::ColourGradient body(juce::Colour(0xff4a4d55).brighter(hovered || dragging ? 0.15f : 0.0f), cap.getX(), cap.getY(),
                              juce::Colour(0xff1c1d21), cap.getX(), cap.getBottom(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(cap, 2.5f);
    g.setColour(accent);
    g.fillRect(cap.getX() + 2.0f, cap.getCentreY() - 1.5f, cap.getWidth() - 4.0f, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawHorizontalLine((int) cap.getY() + 1, cap.getX() + 2.0f, cap.getRight() - 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);

    // Label, or the live value while hovering / dragging.
    // Labels stay inside the fader's own width so neighbours never collide;
    // the full value is also shown in the floating value bubble while dragging.
    const auto labelArea = juce::Rectangle<float>(0.0f, r.getHeight() - labelH, r.getWidth(), labelH).toNearestInt();
    const bool showValue = dragging || hovered;
    g.setColour(showValue ? Colours::lcdText : Colours::text);
    g.setFont(labelFont(9.0f));
    g.drawFittedText(showValue ? getValueText() : label, labelArea, juce::Justification::centred, 1, 0.5f);
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

} // namespace zsynth::ui
