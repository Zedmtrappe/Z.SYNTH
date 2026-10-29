#include "Theme.h"

namespace zsynth::ui
{

ZLookAndFeel::ZLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, Colours::panel);
    setColour(juce::TextButton::buttonColourId, Colours::titleBar);
    setColour(juce::TextButton::buttonOnColourId, Colours::accentDim);
    setColour(juce::TextButton::textColourOffId, Colours::text);
    setColour(juce::TextButton::textColourOnId, Colours::lcdText);
    setColour(juce::PopupMenu::backgroundColourId, Colours::section);
    setColour(juce::PopupMenu::textColourId, Colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Colours::accentDim);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::PopupMenu::headerTextColourId, Colours::accent);
    setColour(juce::TooltipWindow::backgroundColourId, Colours::lcdBack);
    setColour(juce::TooltipWindow::textColourId, Colours::lcdText);
    setColour(juce::TooltipWindow::outlineColourId, Colours::accentDim);
    setColour(juce::TextEditor::backgroundColourId, Colours::lcdBack);
    setColour(juce::TextEditor::textColourId, Colours::lcdText);
    setColour(juce::TextEditor::outlineColourId, Colours::sectionEdge);
    setColour(juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour(juce::TextEditor::highlightColourId, Colours::accentDim);
    setColour(juce::CaretComponent::caretColourId, Colours::accent);
    setColour(juce::ComboBox::backgroundColourId, Colours::lcdBack);
    setColour(juce::ComboBox::textColourId, Colours::lcdText);
    setColour(juce::ComboBox::outlineColourId, Colours::sectionEdge);
    setColour(juce::ComboBox::arrowColourId, Colours::accent);
    setColour(juce::ListBox::backgroundColourId, Colours::lcdBack);
    setColour(juce::ScrollBar::thumbColourId, Colours::accentDim);
    setColour(juce::AlertWindow::backgroundColourId, Colours::section);
    setColour(juce::AlertWindow::textColourId, Colours::text);
    setColour(juce::AlertWindow::outlineColourId, Colours::accentDim);
    setColour(juce::Label::textColourId, Colours::text);
    setColour(juce::ToggleButton::textColourId, Colours::text);
    setColour(juce::ToggleButton::tickColourId, Colours::accent);

    auto scheme = getDarkColourScheme();
    scheme.setUIColour(ColourScheme::windowBackground, Colours::section);
    scheme.setUIColour(ColourScheme::widgetBackground, Colours::lcdBack);
    scheme.setUIColour(ColourScheme::menuBackground, Colours::section);
    scheme.setUIColour(ColourScheme::outline, Colours::sectionEdge);
    scheme.setUIColour(ColourScheme::defaultText, Colours::text);
    scheme.setUIColour(ColourScheme::defaultFill, Colours::accentDim);
    scheme.setUIColour(ColourScheme::highlightedText, juce::Colours::white);
    scheme.setUIColour(ColourScheme::highlightedFill, Colours::accentDim);
    scheme.setUIColour(ColourScheme::menuText, Colours::text);
    setColourScheme(scheme);
}

void ZLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    juce::ColourGradient grad(on ? Colours::accentDim.brighter(0.2f) : Colours::knobTop.brighter(highlighted ? 0.15f : 0.0f), r.getX(), r.getY(),
                              on ? Colours::accentDim.darker(0.3f) : Colours::knobBottom.brighter(0.1f), r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(down ? Colours::accent : Colours::knobEdge.withAlpha(0.9f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 3, r.getRight() - 3);
}

juce::Font ZLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return labelFont(juce::jmin(12.0f, (float) buttonHeight * 0.42f));
}

void ZLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(Colours::section);
    g.setColour(Colours::accentDim);
    g.drawRect(0, 0, width, height);
}

juce::Font ZLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(14.0f));
}

void ZLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.fillAll(Colours::lcdBack);
    g.setColour(Colours::accentDim);
    g.drawRect(0, 0, width, height);
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    const auto lines = juce::StringArray::fromLines(text);
    for (int i = 0; i < lines.size(); ++i)
        s.append(lines[i] + (i < lines.size() - 1 ? "\n" : ""),
                 juce::Font(juce::FontOptions(i == 0 ? 14.0f : 13.0f, i == 0 ? juce::Font::bold : juce::Font::plain)),
                 i == 0 ? Colours::lcdText : Colours::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, (float) width - 16.0f);
    tl.draw(g, { 8.0f, 6.0f, (float) width - 16.0f, (float) height - 12.0f });
}

juce::Rectangle<int> ZLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto lines = juce::StringArray::fromLines(tipText);
    juce::AttributedString s;
    for (const auto& l : lines)
        s.append(l + "\n", juce::Font(juce::FontOptions(14.0f)));
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, 340.0f);
    const int w = (int) tl.getWidth() + 24, h = (int) tl.getHeight() + 14;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void drawLcd(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.0f), 3.0f);
    juce::ColourGradient grad(Colours::lcdBack.brighter(0.08f), r.getX(), r.getY(), Colours::lcdBack, r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawHorizontalLine((int) r.getBottom(), r.getX() + 2, r.getRight() - 2);
}

} // namespace zsynth::ui
