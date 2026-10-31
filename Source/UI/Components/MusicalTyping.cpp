#include "MusicalTyping.h"

namespace zsynth::ui
{

namespace
{
    using K = MusicalTyping::Kind;
    constexpr int kMidiChannel = 1;
    constexpr int kVelocityStep = 16;

    // Placeholder codes for drawn keys that do nothing (never real key codes).
    constexpr int kCaps = -10, kShiftL = -11, kShiftR = -12;
}

const std::vector<std::vector<MusicalTyping::Key>>& MusicalTyping::layout()
{
    static const std::vector<std::vector<Key>> rows = [] {
        auto ch = [](char c, float w = 1.0f, Kind k = K::None, int v = 0) {
            return Key { (int) c, juce::String::charToString((juce::juce_wchar) c), w, k, v };
        };
        std::vector<std::vector<Key>> r(4);
        r[0] = { ch('`'), ch('1', 1, K::BendDown), ch('2', 1, K::BendUp), ch('3', 1, K::Mod, 0), ch('4', 1, K::Mod, 1),
                 ch('5', 1, K::Mod, 2), ch('6', 1, K::Mod, 3), ch('7', 1, K::Mod, 4), ch('8', 1, K::Mod, 5), ch('9'), ch('0'),
                 ch('-'), ch('='), Key { juce::KeyPress::backspaceKey, "delete", 1.5f } };
        r[1] = { Key { juce::KeyPress::tabKey, "tab", 1.5f, K::Sustain }, ch('q'), ch('w', 1, K::Note, 1), ch('e', 1, K::Note, 3),
                 ch('r'), ch('t', 1, K::Note, 6), ch('y', 1, K::Note, 8), ch('u', 1, K::Note, 10), ch('i'),
                 ch('o', 1, K::Note, 13), ch('p', 1, K::Note, 15), ch('['), ch(']'), ch('\\') };
        r[2] = { Key { kCaps, "caps lock", 1.75f }, ch('a', 1, K::Note, 0), ch('s', 1, K::Note, 2), ch('d', 1, K::Note, 4),
                 ch('f', 1, K::Note, 5), ch('g', 1, K::Note, 7), ch('h', 1, K::Note, 9), ch('j', 1, K::Note, 11),
                 ch('k', 1, K::Note, 12), ch('l', 1, K::Note, 14), ch(';', 1, K::Note, 16), ch('\'', 1, K::Note, 17),
                 Key { juce::KeyPress::returnKey, "return", 1.75f } };
        r[3] = { Key { kShiftL, "shift", 2.25f }, ch('z', 1, K::OctaveDown), ch('x', 1, K::OctaveUp), ch('c', 1, K::VelocityDown),
                 ch('v', 1, K::VelocityUp), ch('b'), ch('n'), ch('m'), ch(','), ch('.'), ch('/'), Key { kShiftR, "shift", 2.25f } };
        return r;
    }();
    return rows;
}

MusicalTyping::MusicalTyping(ZSynthProcessor& p) : processor(p) {}

MusicalTyping::~MusicalTyping()
{
    allOff();
}

const MusicalTyping::Key* MusicalTyping::find(int keyCode) const
{
    for (const auto& row : layout())
        for (const auto& k : row)
            if (k.keyCode == keyCode && k.kind != K::None)
                return &k;
    return nullptr;
}

bool MusicalTyping::isMapped(const juce::KeyPress& kp) const
{
    if (kp.getModifiers().isCommandDown() || kp.getModifiers().isCtrlDown() || kp.getModifiers().isAltDown())
        return false; // leave shortcuts (cmd-Q, cmd-W ...) alone
    const int code = kp.getKeyCode();
    return find(code) != nullptr || find((int) juce::CharacterFunctions::toLowerCase((juce::juce_wchar) code)) != nullptr;
}

juce::String MusicalTyping::noteName(int offset) const
{
    return juce::MidiMessage::getMidiNoteName(juce::jlimit(0, 127, baseNote + offset), true, true, 4);
}

void MusicalTyping::act(const Key& k, bool pressed)
{
    auto& kb = processor.getKeyboardState();
    switch (k.kind)
    {
        case K::Note:
            if (pressed)
            {
                const int note = juce::jlimit(0, 127, baseNote + k.value);
                kb.noteOn(kMidiChannel, note, (float) velocity / 127.0f);
                down[k.keyCode] = note;
            }
            else if (auto it = down.find(k.keyCode); it != down.end())
            {
                kb.noteOff(kMidiChannel, it->second, 0.0f);
            }
            break;
        case K::OctaveDown: if (pressed) baseNote = juce::jmax(0, baseNote - 12); break;
        case K::OctaveUp: if (pressed) baseNote = juce::jmin(108, baseNote + 12); break;
        case K::VelocityDown: if (pressed) velocity = juce::jmax(1, velocity - kVelocityStep); break;
        case K::VelocityUp: if (pressed) velocity = juce::jmin(127, velocity + kVelocityStep); break;
        case K::Sustain:
            sustain = pressed;
            processor.setSustainPedal(pressed);
            break;
        case K::BendDown:
        case K::BendUp:
        {
            const int dir = k.kind == K::BendUp ? 1 : -1;
            if (pressed)
                bend = dir;
            else if (bend == dir)
                bend = 0;
            processor.setPitchWheel((float) bend);
            break;
        }
        case K::Mod:
            if (pressed)
            {
                modStep = k.value;
                processor.setModWheel((float) k.value / 5.0f);
            }
            break;
        case K::None:
        default:
            break;
    }
    if (pressed && k.kind != K::Note)
        down[k.keyCode] = -1;
    if (! pressed)
        down.erase(k.keyCode);
}

bool MusicalTyping::keyStateChanged()
{
    bool changed = false;
    for (const auto& row : layout())
        for (const auto& k : row)
        {
            if (k.kind == K::None)
                continue;
            const bool isDownNow = juce::KeyPress::isKeyCurrentlyDown(k.keyCode) || mouseDown.count(k.keyCode) > 0;
            const bool wasDown = down.count(k.keyCode) > 0;
            if (isDownNow != wasDown)
            {
                act(k, isDownNow);
                changed = true;
            }
        }
    if (changed && onChange)
        onChange();
    return changed;
}

void MusicalTyping::press(int keyCode)
{
    if (find(keyCode) == nullptr)
        return;
    mouseDown.insert(keyCode);
    keyStateChanged();
}

void MusicalTyping::release(int keyCode)
{
    mouseDown.erase(keyCode);
    keyStateChanged();
}

void MusicalTyping::allOff()
{
    mouseDown.clear();
    const auto held = down;
    for (const auto& [code, note] : held)
        if (const auto* k = find(code))
            act(*k, false);
    down.clear();
    if (sustain)
        processor.setSustainPedal(false);
    sustain = false;
    if (bend != 0)
        processor.setPitchWheel(0.0f);
    bend = 0;
    if (onChange)
        onChange();
}

// ===================================================================== drawing
MusicalTypingComponent::MusicalTypingComponent(MusicalTyping& t) : typing(t)
{
    setSize(kWidth, kHeight);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    typing.onChange = [this] { repaint(); };
}

MusicalTypingComponent::~MusicalTypingComponent()
{
    typing.onChange = nullptr;
}

std::vector<std::pair<juce::Rectangle<float>, const MusicalTyping::Key*>> MusicalTypingComponent::keyRects() const
{
    std::vector<std::pair<juce::Rectangle<float>, const MusicalTyping::Key*>> out;
    const float left = 24.0f, top = 96.0f, gap = 6.0f;
    const float unit = ((float) kWidth - 2.0f * left + gap) / 14.5f;
    const float keyH = 54.0f;
    float y = top;
    for (const auto& row : MusicalTyping::layout())
    {
        float x = left;
        for (const auto& k : row)
        {
            const float w = unit * k.width - gap;
            out.emplace_back(juce::Rectangle<float>(x, y, w, keyH), &k);
            x += unit * k.width;
        }
        y += keyH + gap;
    }
    return out;
}

int MusicalTypingComponent::keyAt(juce::Point<float> p) const
{
    for (const auto& [r, k] : keyRects())
        if (r.contains(p))
            return k->keyCode;
    return 0;
}

void MusicalTypingComponent::paint(juce::Graphics& g)
{
    namespace m = material;
    using Kind = MusicalTyping::Kind;
    const auto bounds = getLocalBounds().toFloat();

    // Silver aluminium keyboard body.
    juce::ColourGradient alu(juce::Colour(0xffe6e7ea), 0.0f, 0.0f, juce::Colour(0xffb9bbc0), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(alu);
    g.fillRect(bounds);
    m::brushedMetal(g, bounds.reduced(8.0f), 14.0f, juce::Colour(0xffcfd1d6), 0.6f);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds.reduced(8.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawRoundedRectangle(bounds.reduced(7.5f), 14.5f, 1.0f);

    // Status: range, velocity, sustain, mod, bend.
    const auto dark = juce::Colour(0xff2b2c30);
    g.setColour(dark);
    g.setFont(titleFont(13.0f));
    g.drawText("MUSICAL TYPING", 26, 18, 220, 20, juce::Justification::centredLeft, false);
    g.setFont(labelFont(11.0f));
    g.setColour(dark.withAlpha(0.7f));
    g.drawText("Play with your computer keyboard  -  Z/X octave, C/V velocity, Tab sustain, 1/2 bend, 3-8 mod",
               26, 40, 560, 16, juce::Justification::centredLeft, false);

    auto lcd = juce::Rectangle<float>(26.0f, 60.0f, 560.0f, 26.0f);
    drawLcd(g, lcd);
    const juce::String status = "RANGE " + typing.noteName(0) + "-" + typing.noteName(17)
                              + "    VELOCITY " + juce::String(typing.getVelocity())
                              + "    MOD " + juce::String(typing.getModStep() * 20) + "%"
                              + (typing.isSustaining() ? "    SUSTAIN" : "");
    m::glowText(g, status, lcd.reduced(10.0f, 0.0f), lcdFont(13.0f), Colours::lcdText, juce::Justification::centredLeft);
    m::glassSheen(g, lcd, 3.0f, 0.7f);

    // Mini piano of the playable range, lighting held notes.
    {
        const auto area = juce::Rectangle<float>(604.0f, 22.0f, 270.0f, 64.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(area.expanded(3.0f), 4.0f);
        std::set<int> held;
        for (const auto& row : MusicalTyping::layout())
            for (const auto& k : row)
                if (k.kind == Kind::Note && typing.isDown(k.keyCode))
                    held.insert(k.value);
        const int whites[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
        const float ww = area.getWidth() / 11.0f;
        for (int i = 0; i < 11; ++i)
        {
            const auto r = juce::Rectangle<float>(area.getX() + (float) i * ww, area.getY(), ww - 1.0f, area.getHeight());
            g.setColour(held.count(whites[i]) ? Colours::accent : juce::Colour(0xfff6f2ea));
            g.fillRect(r);
        }
        for (int i = 0; i < 10; ++i)
        {
            const int semis[] = { 1, 3, -1, 6, 8, 10, -1, 13, 15, -1 };
            if (semis[i] < 0)
                continue;
            const auto r = juce::Rectangle<float>(area.getX() + ((float) i + 0.68f) * ww, area.getY(), ww * 0.62f, area.getHeight() * 0.6f);
            g.setColour(held.count(semis[i]) ? Colours::accent.darker(0.2f) : juce::Colour(0xff1b1b1e));
            g.fillRoundedRectangle(r, 1.5f);
        }
    }

    // Keys.
    for (const auto& [r, k] : keyRects())
    {
        const bool isDown = typing.isDown(k->keyCode);
        const bool blackNote = k->kind == Kind::Note && juce::MidiMessage::isMidiNoteBlack(k->value);
        const bool mapped = k->kind != Kind::None;
        const auto cap = r.translated(0.0f, isDown ? 1.5f : 0.0f);

        g.setColour(juce::Colours::black.withAlpha(isDown ? 0.18f : 0.28f));
        g.fillRoundedRectangle(r.translated(0.0f, 2.5f), 7.0f);
        const auto top = blackNote ? juce::Colour(0xff3a3b40) : juce::Colour(0xfffdfdfd);
        const auto bottom = blackNote ? juce::Colour(0xff1d1e21) : juce::Colour(0xffe4e5e8);
        juce::ColourGradient body(isDown ? top.darker(0.08f) : top, 0.0f, cap.getY(), isDown ? bottom.darker(0.08f) : bottom, 0.0f,
                                  cap.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(cap, 7.0f);
        g.setColour(blackNote ? juce::Colours::black : juce::Colour(0xffc4c6cb));
        g.drawRoundedRectangle(cap, 7.0f, 1.0f);
        if (isDown)
        {
            g.setColour(Colours::accent.withAlpha(0.45f));
            g.fillRoundedRectangle(cap, 7.0f);
            g.setColour(Colours::accent);
            g.drawRoundedRectangle(cap.reduced(1.0f), 6.0f, 2.0f);
        }

        // Legend (Apple style: small, lower-case, top-left), and its function.
        const auto ink = blackNote ? juce::Colour(0xfff2f2f2) : juce::Colour(0xff3a3b3f);
        g.setColour(ink.withAlpha(mapped ? 1.0f : 0.45f));
        g.setFont(juce::Font(juce::FontOptions(k->cap.length() > 1 ? 11.0f : 15.0f)));
        const bool wide = k->cap.length() > 1;
        g.drawText(k->cap, cap.reduced(8.0f, 5.0f).toNearestInt(),
                   wide ? juce::Justification::bottomLeft : juce::Justification::topLeft, false);

        juce::String fn;
        switch (k->kind)
        {
            case Kind::Note: fn = typing.noteName(k->value); break;
            case Kind::OctaveDown: fn = "OCT -"; break;
            case Kind::OctaveUp: fn = "OCT +"; break;
            case Kind::VelocityDown: fn = "VEL -"; break;
            case Kind::VelocityUp: fn = "VEL +"; break;
            case Kind::Sustain: fn = "SUSTAIN"; break;
            case Kind::BendDown: fn = "BEND -"; break;
            case Kind::BendUp: fn = "BEND +"; break;
            case Kind::Mod: fn = "MOD " + juce::String(k->value * 20); break;
            case Kind::None:
            default: break;
        }
        if (fn.isNotEmpty())
        {
            const bool noteKey = k->kind == Kind::Note;
            g.setColour(noteKey ? (blackNote ? Colours::accent.brighter(0.3f) : Colours::accent.darker(0.35f)) : juce::Colour(0xff8a5a10));
            g.setFont(labelFont(noteKey ? 12.0f : 9.5f));
            g.drawFittedText(fn, cap.reduced(4.0f, 6.0f).toNearestInt(),
                             k->cap.length() > 1 ? juce::Justification::topRight : juce::Justification::bottomRight, 1, 0.6f);
        }
    }
}

bool MusicalTypingComponent::keyPressed(const juce::KeyPress& k)
{
    return typing.isMapped(k);
}

bool MusicalTypingComponent::keyStateChanged(bool)
{
    return typing.keyStateChanged();
}

void MusicalTypingComponent::mouseDown(const juce::MouseEvent& e)
{
    mouseKey = keyAt(e.position);
    if (mouseKey != 0)
        typing.press(mouseKey);
}

void MusicalTypingComponent::mouseUp(const juce::MouseEvent&)
{
    if (mouseKey != 0)
        typing.release(mouseKey);
    mouseKey = 0;
}

void MusicalTypingComponent::focusLost(FocusChangeType)
{
    // Keys released while another window has focus are never reported here.
    typing.keyStateChanged();
}

// ====================================================================== window
MusicalTypingWindow::MusicalTypingWindow(MusicalTyping& t)
    : juce::DocumentWindow("ZYNTH - Musical Typing", juce::Colour(0xffcfd1d6), juce::DocumentWindow::closeButton), typing(t)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new MusicalTypingComponent(t), true);
    setResizable(false, false);
    setAlwaysOnTop(true);
    centreWithSize(getWidth(), getHeight());
}

void MusicalTypingWindow::closeButtonPressed()
{
    setVisible(false);
}

void MusicalTypingWindow::setVisible(bool shouldBeVisible)
{
    juce::DocumentWindow::setVisible(shouldBeVisible);
    if (! shouldBeVisible)
        typing.allOff();
    else if (auto* c = getContentComponent())
    {
        toFront(true);
        c->grabKeyboardFocus();
    }
}

} // namespace zsynth::ui
