#include "SequencerBar.h"

namespace zsynth::ui
{

namespace
{
    constexpr int kLabelW = 110;
    const juce::Colour kLedRed { 0xffff3b2a };
}

void drawSequencerLights(juce::Graphics& g, juce::Rectangle<float> area, ZSynthProcessor& p, int row, bool numbers)
{
    const int base = row * kPseqRowStride;
    const int length = juce::roundToInt(p.getValue(P::PseqALen + base));
    const bool on = p.getValue(P::PseqAOn + base) >= 0.5f;
    const int current = p.getSeqStep(row);
    const float cell = area.getWidth() / (float) kPseqSteps;
    for (int st = 0; st < kPseqSteps; ++st)
    {
        const auto c = juce::Rectangle<float>(area.getX() + cell * (float) st, area.getY(), cell, area.getHeight());
        const bool inLength = st < length;
        const bool gate = p.getValue(P::PseqAGate1 + base + st * kPseqStepStride) >= 0.5f;
        const float d = juce::jmin(c.getHeight() - (numbers ? 12.0f : 2.0f), 9.0f);
        const auto led = juce::Rectangle<float>(d, d).withCentre({ c.getCentreX(), c.getY() + d * 0.5f + 1.0f });
        const bool lit = st == current && on;
        if (lit)
        {
            g.setColour(kLedRed.withAlpha(0.35f));
            g.fillEllipse(led.expanded(3.0f));
        }
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillEllipse(led.expanded(1.0f));
        const auto lens = lit ? kLedRed.brighter(0.4f) : (inLength ? kLedRed.withMultipliedBrightness(gate ? 0.38f : 0.18f) : juce::Colour(0xff1a1414));
        g.setColour(lens);
        g.fillEllipse(led);
        if (inLength && ! gate && ! lit)
        {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillEllipse(led.reduced(d * 0.3f));
        }
        g.setColour(juce::Colours::white.withAlpha(lit ? 0.6f : 0.15f));
        g.fillEllipse(led.getX() + d * 0.22f, led.getY() + d * 0.15f, d * 0.3f, d * 0.24f);
        if (numbers)
        {
            g.setColour((inLength ? juce::Colour(0xffe8e4dc) : juce::Colour(0xff6a6660)).withAlpha(0.9f));
            g.setFont(labelFont(9.0f));
            g.drawText(juce::String(st + 1), c.withTop(led.getBottom() + 1.0f).toNearestInt(), juce::Justification::centredTop, false);
        }
    }
}

void SongLights::paint(juce::Graphics& g)
{
    const float cell = (float) getWidth() / 16.0f;
    const int slot = shown >= 0 ? shown % 100 : -1;
    for (int i = 0; i < 16; ++i)
    {
        const bool lit = i == slot;
        const auto led = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ cell * ((float) i + 0.5f) - 1.5f, (float) getHeight() * 0.5f });
        if (lit)
        {
            g.setColour(kLedRed.withAlpha(0.35f));
            g.fillEllipse(led.expanded(3.0f));
        }
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillEllipse(led.expanded(1.0f));
        g.setColour(lit ? kLedRed.brighter(0.4f) : kLedRed.withMultipliedBrightness(0.22f));
        g.fillEllipse(led);
        const int repeats = shown / 10000, pass = (shown / 100) % 100;
        if (lit && repeats > 1) // which time through the slot's repeats
        {
            g.setColour(kLedRed.brighter(0.3f));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(juce::String(pass + 1) + "/" + juce::String(repeats),
                       juce::Rectangle<float>(led.getRight() + 4.0f, 0.0f, cell * 0.5f, (float) getHeight()), juce::Justification::centredLeft, false);
        }
    }
}

void SequencerLights::paint(juce::Graphics& g)
{
    drawSequencerLights(g, getLocalBounds().toFloat(), processor, row, true);
}

SequencerBar::SequencerBar(ZSynthProcessor& p) : processor(p)
{
    playButton.setTooltip("Play / stop the pattern sequencer (both rows). With HOST SYNC on, a host's transport starts and stops it.");
    playButton.onClick = [this] {
        if (auto* prm = processor.getState().getParameter("pseq.run"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->getValue() >= 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
        updateButtons();
    };
    songButton.setTooltip("SONG mode: the sequencer plays its song slots (lit while on; shows the slot playing). Click to switch it on or off.");
    songButton.onClick = [this] {
        if (auto* prm = processor.getState().getParameter("pseq.song"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->getValue() >= 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
        updateButtons();
    };
    addAndMakeVisible(songButton);
    editButton.setButtonText(juce::String::fromUTF8("SEQUENCER \xe2\x96\xb8"));
    editButton.setTooltip("Open the pattern sequencer: steps, rows, patterns");
    editButton.onClick = [this] { if (onEdit) onEdit(); };
    for (int i = 0; i < ZSynthProcessor::kPatterns; ++i)
    {
        auto& b = patternButtons[i];
        b.setButtonText(juce::String::charToString((juce::juce_wchar) ('A' + i)));
        b.setTooltip("Pattern " + b.getButtonText() + ": click to recall it (the one you leave is kept)");
        b.onClick = [this, i] {
            processor.selectPattern(i);
            updateButtons();
        };
        addAndMakeVisible(b);
    }
    addAndMakeVisible(playButton);
    addAndMakeVisible(editButton);
    updateButtons();
    startTimerHz(30);
}

SequencerBar::~SequencerBar()
{
    stopTimer();
}

juce::Rectangle<int> SequencerBar::lightsArea() const
{
    const int x = editButton.getRight() + 12;
    return { x, 3, getWidth() - 4 - x, getHeight() - 6 };
}

void SequencerBar::resized()
{
    int x = kLabelW;
    playButton.setBounds(x, 2, 92, getHeight() - 4);
    x += 100;
    for (auto& b : patternButtons)
    {
        b.setBounds(x, 2, 28, getHeight() - 4);
        x += 30;
    }
    // All the sequencer's keys together on the left, the step lights after them.
    songButton.setBounds(x + 6, 2, 104, getHeight() - 4);
    editButton.setBounds(songButton.getRight() + 6, 2, 124, getHeight() - 4);
}

void SequencerBar::updateButtons()
{
    const bool running = processor.isSeqRunning() || processor.getValue(P::PseqRun) >= 0.5f;
    playButton.setButtonText(juce::String::fromUTF8(running ? "\xe2\x96\xa0  STOP" : "\xe2\x96\xb6  PLAY"));
    playButton.setToggleState(running, juce::dontSendNotification);
    for (int i = 0; i < ZSynthProcessor::kPatterns; ++i)
        patternButtons[i].setToggleState(i == processor.getPattern(), juce::dontSendNotification);
    const bool song = processor.getValue(P::PseqSongOn) >= 0.5f;
    const int slot = song && running ? processor.getSongSlot() : -1;
    juce::String songText("SONG");
    if (slot >= 0)
    {
        songText << "  " << (slot + 1);
        if (processor.getSongRepeats() > 1)
            songText << "  " << (processor.getSongPass() + 1) << "/" << processor.getSongRepeats();
    }
    songButton.setButtonText(songText);
    songButton.setToggleState(song, juce::dontSendNotification);
}

void SequencerBar::timerCallback()
{
    const bool running = processor.isSeqRunning() || processor.getValue(P::PseqRun) >= 0.5f;
    const int songState = processor.getValue(P::PseqSongOn) >= 0.5f
                              ? processor.getSongSlot() + (running ? 100 : 0) + 1000 * processor.getSongPass() : -2;
    bool dirty = running != shownRunning || processor.getPattern() != shownPattern || songState != shownSong;
    shownSong = songState;
    if (dirty)
    {
        shownRunning = running;
        shownPattern = processor.getPattern();
        updateButtons();
    }
    for (int r = 0; r < 2; ++r)
    {
        const int st = processor.getSeqStep(r), len = juce::roundToInt(processor.getValue(P::PseqALen + r * kPseqRowStride));
        if (st != shownStep[r] || len != shownLength[r])
        {
            shownStep[r] = st;
            shownLength[r] = len;
            dirty = true;
        }
    }
    if (dirty)
        repaint(lightsArea());
}

void SequencerBar::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    legend.draw(g, "SEQUENCER", juce::Rectangle<float>(8.0f, 0.0f, (float) kLabelW - 12.0f, r.getHeight()), titleFont(11.5f),
                juce::Justification::centredLeft, Colours::accent.brighter(0.2f), 0.9f);
    // Both rows' lights, A above B, on a dark strip.
    const auto lights = lightsArea().toFloat();
    drawLcd(g, lights, juce::Colour(0xff0c0606));
    auto inner = lights.reduced(8.0f, 3.0f);
    g.setFont(labelFont(8.5f));
    for (int row = 0; row < 2; ++row)
    {
        auto line = inner.removeFromTop(inner.getHeight() / (float) (2 - row));
        g.setColour(juce::Colour(0xffe8e4dc).withAlpha(0.7f));
        g.drawText(row == 0 ? "A" : "B", line.removeFromLeft(14.0f).toNearestInt(), juce::Justification::centredLeft, false);
        drawSequencerLights(g, line, processor, row, false);
    }
}

} // namespace zsynth::ui
