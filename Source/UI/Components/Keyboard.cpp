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
    // Ivory-white keys running under the panel lip; a pressed key tilts away
    // from the light (darker, shorter front edge) and glows amber.
    const auto key = area.reduced(0.6f, 0.0f);
    const float front = isDown ? 5.0f : 9.0f;
    const auto top = key.withTrimmedBottom(front);
    juce::ColourGradient grad(isDown ? juce::Colour(0xffd9cfbf) : juce::Colour(0xfff6f1e7), top.getX(), top.getY(),
                              isDown ? juce::Colour(0xffc8bda9) : juce::Colour(0xffe4ddd0), top.getX(), top.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(top);
    // Rounded side edges.
    juce::ColourGradient sides(juce::Colours::black.withAlpha(0.18f), top.getX(), 0.0f, juce::Colours::transparentBlack, top.getX() + 4.0f, 0.0f, false);
    g.setGradientFill(sides);
    g.fillRect(top.withWidth(4.0f));
    juce::ColourGradient sidesR(juce::Colours::transparentBlack, top.getRight() - 4.0f, 0.0f, juce::Colours::black.withAlpha(0.22f), top.getRight(), 0.0f, false);
    g.setGradientFill(sidesR);
    g.fillRect(top.withLeft(top.getRight() - 4.0f));
    // Front face of the key.
    const auto face = key.withTop(top.getBottom());
    juce::ColourGradient fg(juce::Colour(0xffcfc6b5), 0.0f, face.getY(), juce::Colour(0xff8f877a), 0.0f, face.getBottom(), false);
    g.setGradientFill(fg);
    g.fillRoundedRectangle(face, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawHorizontalLine(juce::roundToInt(face.getY()), face.getX() + 1.0f, face.getRight() - 1.0f);
    // Shadow from the panel lip above the keys.
    juce::ColourGradient lip(juce::Colours::black.withAlpha(0.55f), 0.0f, key.getY(), juce::Colours::transparentBlack, 0.0f, key.getY() + 16.0f, false);
    g.setGradientFill(lip);
    g.fillRect(key.withHeight(16.0f));
    if (isDown)
    {
        g.setColour(Colours::accent.withAlpha(0.22f));
        g.fillRect(top);
    }
    else if (isOver)
    {
        g.setColour(Colours::accent.withAlpha(0.1f));
        g.fillRect(top);
    }
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawVerticalLine(juce::roundToInt(key.getRight()), key.getY(), key.getBottom());
    if (note % 12 == 0)
    {
        g.setColour(juce::Colour(0xff7a7264));
        g.setFont(labelFont(10.0f));
        g.drawText("C" + juce::String(note / 12 - 1), top.withTrimmedTop(top.getHeight() - 18.0f).toNearestInt(), juce::Justification::centred, false);
    }
}

void ZKeyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    // Glossy black keys: shadow on the whites, a sloped front and a lit top edge.
    const auto key = area.reduced(1.0f, 0.0f);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(key.expanded(2.5f, 0.0f).translated(3.0f, 4.0f), 4.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(key.translated(1.5f, 2.0f), 3.0f);
    g.setColour(juce::Colour(0xff050506));
    g.fillRoundedRectangle(key, 2.5f);
    const float front = isDown ? 6.0f : 11.0f;
    const auto top = key.reduced(2.0f, 0.0f).withTrimmedBottom(front);
    juce::ColourGradient grad(isDown ? juce::Colour(0xff201d1a) : juce::Colour(0xff2c2d31), top.getX(), top.getY(),
                              isDown ? juce::Colour(0xff0d0c0b) : juce::Colour(0xff121215), top.getX(), top.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(top, 2.0f);
    // Gloss streak along the top surface.
    g.setColour(juce::Colours::white.withAlpha(isDown ? 0.06f : 0.13f));
    g.fillRoundedRectangle(top.getX() + top.getWidth() * 0.18f, top.getY() + 10.0f, top.getWidth() * 0.22f, top.getHeight() - 16.0f, 1.5f);
    // Sloped front with a lit edge.
    const auto face = key.reduced(1.0f, 0.0f).withTop(top.getBottom());
    juce::ColourGradient fg(juce::Colour(0xff3d3e43), 0.0f, face.getY(), juce::Colour(0xff0b0b0c), 0.0f, face.getBottom(), false);
    g.setGradientFill(fg);
    g.fillRoundedRectangle(face, 2.0f);
    if (isDown)
    {
        g.setColour(Colours::accent.withAlpha(0.35f));
        g.fillRoundedRectangle(top, 2.0f);
    }
    else if (isOver)
    {
        g.setColour(Colours::accent.withAlpha(0.15f));
        g.fillRoundedRectangle(top, 2.0f);
    }
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
    legend.draw(g, pitch ? "PITCH" : "MOD", labelArea, labelFont(10.0f), juce::Justification::centred, material::kLegend, 0.6f);

    // The wheel sits in a slot cut into the panel: only its front arc shows,
    // lit from above and falling into shadow at the top and bottom.
    material::recess(g, r, 6.0f, juce::Colour(0xff030304), 1.2f);
    const auto wheel = r.reduced(6.0f, 6.0f);
    juce::ColourGradient body(juce::Colour(0xff0d0d0f), wheel.getX(), wheel.getY(), juce::Colour(0xff0d0d0f), wheel.getX(), wheel.getBottom(), false);
    body.addColour(0.35, juce::Colour(0xff4b4e56));
    body.addColour(0.5, juce::Colour(0xff3a3c42));
    body.addColour(0.8, juce::Colour(0xff1c1d20));
    g.setGradientFill(body);
    g.fillRoundedRectangle(wheel, 4.0f);

    // Rubber ribs, bunched towards the edges where the surface curves away.
    const float norm = pitch ? position * 0.5f + 0.5f : position;
    const float phase = std::fmod(norm * 5.0f, 1.0f);
    for (int k = -1; k < 24; ++k)
    {
        const float t = ((float) k + phase) / 23.0f;             // position around the visible arc
        if (t < 0.0f || t > 1.0f)
            continue;
        const float yy = wheel.getCentreY() - std::cos(t * juce::MathConstants<float>::pi) * wheel.getHeight() * 0.5f;
        const float facing = std::sin(t * juce::MathConstants<float>::pi);
        g.setColour(juce::Colours::black.withAlpha(0.25f + 0.35f * facing));
        g.drawHorizontalLine(juce::roundToInt(yy), wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f * facing));
        g.drawHorizontalLine(juce::roundToInt(yy) + 1, wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
    }

    // Lit position marker moulded into the wheel.
    const float markerY = wheel.getBottom() - norm * wheel.getHeight();
    const auto marker = juce::Rectangle<float>(wheel.getX() + 2.0f, markerY - 3.0f, wheel.getWidth() - 4.0f, 6.0f);
    g.setColour(Colours::accent.withAlpha(0.3f));
    g.fillRoundedRectangle(marker.expanded(1.5f, 2.5f), 3.0f);
    g.setColour(Colours::accent.brighter(0.2f));
    g.fillRoundedRectangle(marker, 2.0f);
    if (pitch)
    {
        material::engravedLine(g, { wheel.getX() - 5.0f, wheel.getCentreY(), wheel.getX() - 1.0f, wheel.getCentreY() });
        material::engravedLine(g, { wheel.getRight() + 1.0f, wheel.getCentreY(), wheel.getRight() + 5.0f, wheel.getCentreY() });
    }
}

} // namespace zsynth::ui
