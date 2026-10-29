#include "Keyboard.h"

namespace zsynth::ui
{

ZKeyboard::ZKeyboard(juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setScrollButtonsVisible(false);
    setAvailableRange(lowestKey, lowestKey + 60);
    setOctaveForMiddleC(4);
    setKeyPressBaseOctave(4);
    setVelocity(0.8f, true);
    setColour(keySeparatorLineColourId, juce::Colour(0xff2a2a2c));
    setColour(mouseOverKeyOverlayColourId, Colours::accent.withAlpha(0.25f));
    setColour(keyDownOverlayColourId, Colours::accent.withAlpha(0.8f));
    setColour(shadowColourId, juce::Colours::black.withAlpha(0.5f));
    setTitle("Keyboard");
}

void ZKeyboard::shiftOctave(int delta)
{
    lowestKey = juce::jlimit(0, 60, lowestKey + delta * 12);
    setAvailableRange(lowestKey, lowestKey + 60);
    resized();
}

void ZKeyboard::resized()
{
    // 36 white keys always fill the fixed keyboard area exactly.
    setKeyWidth((float) getWidth() / 36.0f);
    setBlackNoteLengthProportion(0.62f);
    juce::MidiKeyboardComponent::resized();
}

void ZKeyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    const auto key = area.reduced(1.0f, 0.0f);
    juce::ColourGradient grad(isDown ? Colours::accent.brighter(0.4f) : Colours::white, key.getX(), key.getY(),
                              isDown ? Colours::accent : Colours::white.darker(0.12f), key.getX(), key.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(key.withTrimmedTop(-4.0f), 4.0f);
    if (isOver && ! isDown)
    {
        g.setColour(Colours::accent.withAlpha(0.18f));
        g.fillRect(key);
    }
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawRect(key.withTrimmedTop(-2.0f), 1.0f);
    if (note % 12 == 0)
    {
        g.setColour(juce::Colour(0xff6a655c));
        g.setFont(labelFont(10.0f));
        g.drawText("C" + juce::String(note / 12 - 1), key.withTrimmedTop(key.getHeight() - 18.0f).toNearestInt(), juce::Justification::centred, false);
    }
}

void ZKeyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    const auto key = area.reduced(1.0f, 0.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(key.translated(2.0f, 2.0f), 3.0f);
    juce::ColourGradient grad(isDown ? Colours::accent : juce::Colour(0xff3a3b40), key.getX(), key.getY(),
                              isDown ? Colours::accentDim : juce::Colour(0xff111113), key.getX(), key.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(key, 3.0f);
    if (isOver && ! isDown)
    {
        g.setColour(Colours::accent.withAlpha(0.25f));
        g.fillRoundedRectangle(key, 3.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillRect(key.getX() + 3.0f, key.getBottom() - 10.0f, key.getWidth() - 6.0f, 3.0f);
}

// ---------------------------------------------------------------------- wheels
PerformanceWheel::PerformanceWheel(ZSynthProcessor& p, bool isPitch) : processor(p), pitch(isPitch)
{
    setTooltip(pitch ? "Pitch bend (springs back). Range is set by BEND in the Voice section."
                     : "Modulation wheel: route it with the modulation matrix (source 'Mod Wheel').");
    setTitle(pitch ? "Pitch wheel" : "Modulation wheel");
}

void PerformanceWheel::refresh()
{
    if (dragging)
        return;
    const float v = pitch ? processor.getEngine().getPitchBend() : processor.getEngine().getModWheel();
    if (std::abs(v - position) > 0.002f)
    {
        position = v;
        repaint();
    }
}

void PerformanceWheel::send(float v)
{
    position = v;
    if (pitch)
        processor.setPitchWheel(v);
    else
        processor.setModWheel(v);
    repaint();
}

void PerformanceWheel::mouseDown(const juce::MouseEvent&)
{
    dragging = true;
    startPos = position;
}

void PerformanceWheel::mouseDrag(const juce::MouseEvent& e)
{
    const float range = (float) getHeight() * 0.8f;
    const float delta = -(float) e.getDistanceFromDragStartY() / range * (pitch ? 2.0f : 1.0f);
    send(juce::jlimit(pitch ? -1.0f : 0.0f, 1.0f, startPos + delta));
}

void PerformanceWheel::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    if (pitch)
        send(0.0f);
}

void PerformanceWheel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom(16.0f);
    g.setColour(Colours::text);
    g.setFont(labelFont(10.0f));
    g.drawText(pitch ? "PITCH" : "MOD", labelArea.toNearestInt(), juce::Justification::centred, false);

    // Slot
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(r, 6.0f);
    const auto wheel = r.reduced(6.0f, 6.0f);
    juce::ColourGradient body(juce::Colour(0xff202125), wheel.getX(), wheel.getY(), juce::Colour(0xff202125), wheel.getX(), wheel.getBottom(), false);
    body.addColour(0.5, juce::Colour(0xff4a4d55));
    g.setGradientFill(body);
    g.fillRoundedRectangle(wheel, 4.0f);

    // Ribs moving with the wheel.
    const float norm = pitch ? position * 0.5f + 0.5f : position;
    const float offset = norm * 40.0f;
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    for (float y = wheel.getY() + std::fmod(offset, 8.0f); y < wheel.getBottom(); y += 8.0f)
        g.drawHorizontalLine((int) y, wheel.getX() + 2.0f, wheel.getRight() - 2.0f);

    const float markerY = wheel.getBottom() - norm * wheel.getHeight();
    g.setColour(Colours::accent);
    g.fillRoundedRectangle(wheel.getX() + 2.0f, markerY - 3.0f, wheel.getWidth() - 4.0f, 6.0f, 2.0f);
    if (pitch)
    {
        g.setColour(Colours::textDim);
        g.drawHorizontalLine((int) wheel.getCentreY(), wheel.getX() - 4.0f, wheel.getX());
        g.drawHorizontalLine((int) wheel.getCentreY(), wheel.getRight(), wheel.getRight() + 4.0f);
    }
}

} // namespace zsynth::ui
