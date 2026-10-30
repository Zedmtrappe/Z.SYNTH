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
    // A moulded rubber/plastic key standing on the panel; it sinks when pressed.
    auto r = b.getLocalBounds().toFloat().reduced(1.5f, 1.5f).withTrimmedBottom(1.5f);
    const bool on = b.getToggleState();
    const float sink = down ? 1.2f : 0.0f;
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(r.expanded(1.0f).translated(0.8f, 2.8f - sink), 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.translated(0.4f, 1.5f - sink * 0.5f), 4.0f);
    r = r.translated(0.0f, sink);
    const auto top = on ? Colours::accentDim.brighter(0.35f) : juce::Colour(0xff4a4d55).brighter(highlighted ? 0.12f : 0.0f);
    const auto bottom = on ? Colours::accentDim.darker(0.4f) : juce::Colour(0xff17181b);
    juce::ColourGradient grad(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    grad.addColour(0.5, top.interpolatedWith(bottom, 0.6f));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 4.0f);
    juce::ColourGradient edge(juce::Colours::white.withAlpha(0.28f), 0.0f, r.getY(), juce::Colours::black.withAlpha(0.6f), 0.0f, r.getBottom(), false);
    edge.addColour(0.2, juce::Colours::white.withAlpha(0.05f));
    edge.addColour(0.8, juce::Colours::transparentBlack);
    g.setGradientFill(edge);
    g.drawRoundedRectangle(r.reduced(0.6f), 3.5f, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r.expanded(0.5f), 4.5f, 1.0f);
}

void ZLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    // Back-lit legend on the key cap.
    const auto font = getTextButtonFont(b, b.getHeight());
    auto r = b.getLocalBounds().reduced(4, 2).translated(0, down ? 1 : 0).toFloat().withTrimmedBottom(1.5f);
    const auto colour = b.findColour(b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                            .withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.5f);
    g.setFont(font);
    g.setColour(colour.withAlpha(0.18f));
    for (auto d : { juce::Point<float>(-0.8f, 0.0f), juce::Point<float>(0.8f, 0.0f), juce::Point<float>(0.0f, 0.8f) })
        g.drawFittedText(b.getButtonText(), (r + d).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    g.setColour(colour);
    g.drawFittedText(b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
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
    // A display window set into the panel: dark smoked glass with depth.
    // Gradients only (no blurs): displays repaint at the panel frame rate.
    constexpr float radius = 3.0f;
    juce::ColourGradient grad(Colours::lcdBack.brighter(0.1f), r.getX(), r.getY(), Colours::lcdBack.darker(0.4f), r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, radius);

    g.saveState();
    juce::Path p;
    p.addRoundedRectangle(r, radius);
    g.reduceClipRegion(p);
    // Inner shadow from the upper and left walls of the cut-out.
    const float d = juce::jmin(7.0f, r.getHeight() * 0.35f);
    juce::ColourGradient topWall(juce::Colours::black.withAlpha(0.75f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + d, false);
    g.setGradientFill(topWall);
    g.fillRect(r.withHeight(d));
    juce::ColourGradient leftWall(juce::Colours::black.withAlpha(0.45f), r.getX(), 0.0f, juce::Colours::transparentBlack, r.getX() + d * 0.7f, 0.0f, false);
    g.setGradientFill(leftWall);
    g.fillRect(r.withWidth(d * 0.7f));
    g.restoreState();

    // Bezel: dark upper wall, lit lower lip.
    juce::ColourGradient lip(juce::Colours::black, 0.0f, r.getY(), juce::Colours::white.withAlpha(0.16f), 0.0f, r.getBottom(), false);
    lip.addColour(0.6, juce::Colours::black.withAlpha(0.6f));
    g.setGradientFill(lip);
    g.drawRoundedRectangle(r.expanded(0.5f), radius + 0.5f, 1.2f);
}

} // namespace zsynth::ui
