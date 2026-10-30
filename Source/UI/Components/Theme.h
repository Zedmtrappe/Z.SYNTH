#pragma once

// ZYNTH visual identity: graphite panel, warm amber accent, LCD displays.

#include <juce_gui_basics/juce_gui_basics.h>

namespace zsynth::ui
{

namespace Colours
{
    const juce::Colour panel        { 0xff141518 };
    const juce::Colour panelTop     { 0xff1b1c20 };
    const juce::Colour section      { 0xff222429 };
    const juce::Colour sectionLow   { 0xff1c1e22 };
    const juce::Colour sectionEdge  { 0xff383b42 };
    const juce::Colour sectionShadow{ 0xff09090b };
    const juce::Colour subPanel     { 0xff1a1b1f };
    const juce::Colour titleBar     { 0xff2a2c32 };
    const juce::Colour accent       { 0xfff0a53e };
    const juce::Colour accentDim    { 0xff8a5e24 };
    const juce::Colour text         { 0xffd2cdc3 };
    const juce::Colour textDim      { 0xff8a857c };
    const juce::Colour knobTop      { 0xff3a3d44 };
    const juce::Colour knobBottom   { 0xff16171a };
    const juce::Colour knobEdge     { 0xff4a4e56 };
    const juce::Colour track        { 0xff33363c };
    const juce::Colour lcdBack      { 0xff110c05 };
    const juce::Colour lcdText      { 0xffffb84d };
    const juce::Colour lcdDim       { 0xff6b4a1c };
    const juce::Colour ledOn        { 0xffff6a2a };
    const juce::Colour ledOff       { 0xff3b2620 };
    const juce::Colour green        { 0xff7fd36b };
    const juce::Colour red          { 0xffff4a3a };
    const juce::Colour white        { 0xffe8e4dc };
}

inline juce::Font labelFont(float size = 10.5f)
{
    return juce::Font(juce::FontOptions(size, juce::Font::bold)).withExtraKerningFactor(0.06f);
}

inline juce::Font titleFont(float size = 12.5f)
{
    return juce::Font(juce::FontOptions(size, juce::Font::bold)).withExtraKerningFactor(0.18f);
}

inline juce::Font lcdFont(float size = 13.0f)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
}

/** LookAndFeel for the stock JUCE pieces (menus, buttons, tooltips, dialogs). */
class ZLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZLookAndFeel();

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};

/** Draws a recessed LCD window. */
void drawLcd(juce::Graphics& g, juce::Rectangle<float> r);

} // namespace zsynth::ui
