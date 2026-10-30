#include "AuditionBar.h"

namespace zsynth::ui
{

namespace
{
    constexpr int kLabelW = 96;
    constexpr int kClipW = 300;
}

AuditionBar::AuditionBar(ZSynthProcessor& p) : processor(p)
{
    playButton.setTooltip("Play / stop the audition clip. It plays into the current sound, so you can tweak the panel while it runs.");
    playButton.onClick = [this] {
        auto& a = processor.getAudition();
        if (a.isPlaying())
            a.stop();
        else
            a.play();
        updateButtons();
    };
    loopButton.setButtonText("LOOP");
    loopButton.setClickingTogglesState(true);
    loopButton.setToggleState(processor.getAudition().isLooping(), juce::dontSendNotification);
    loopButton.setTooltip("Repeat the clip until stopped");
    loopButton.onClick = [this] { processor.getAudition().setLooping(loopButton.getToggleState()); };
    loadButton.setButtonText(juce::String::fromUTF8("LOAD MIDI\xe2\x80\xa6"));
    loadButton.setTooltip("Audition with your own standard MIDI file (.mid). All its channels play as one part.");
    loadButton.onClick = [this] { chooseFile(); };
    for (auto* b : { &playButton, &loopButton, &loadButton })
        addAndMakeVisible(*b);
    lastFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    updateButtons();
    startTimerHz(30);
}

AuditionBar::~AuditionBar()
{
    stopTimer();
}

juce::Rectangle<int> AuditionBar::clipArea() const
{
    return { kLabelW, 3, kClipW, getHeight() - 6 };
}

juce::Rectangle<int> AuditionBar::progressArea() const
{
    const int x = loadButton.getRight() + 10;
    return { x, 5, getWidth() - x - 260, getHeight() - 10 };
}

void AuditionBar::resized()
{
    int x = clipArea().getRight() + 8;
    playButton.setBounds(x, 2, 84, getHeight() - 4);
    x += 90;
    loopButton.setBounds(x, 2, 64, getHeight() - 4);
    x += 70;
    loadButton.setBounds(x, 2, 110, getHeight() - 4);
}

void AuditionBar::updateButtons()
{
    const bool playing = processor.getAudition().isPlaying();
    playButton.setButtonText(juce::String::fromUTF8(playing ? "\xe2\x96\xa0  STOP" : "\xe2\x96\xb6  PLAY"));
    playButton.setToggleState(playing, juce::dontSendNotification);
    loopButton.setToggleState(processor.getAudition().isLooping(), juce::dontSendNotification);
}

void AuditionBar::timerCallback()
{
    auto& a = processor.getAudition();
    const auto name = a.getClipName();
    const float pos = a.isPlaying() ? a.getPosition() : 0.0f;
    if (a.isPlaying() != shownPlaying)
    {
        shownPlaying = a.isPlaying();
        updateButtons();
        repaint();
    }
    if (name != shownName)
    {
        shownName = name;
        repaint();
    }
    if (std::abs(pos - shownPosition) > 0.002f)
    {
        shownPosition = pos;
        repaint(progressArea());
    }
}

void AuditionBar::mouseDown(const juce::MouseEvent& e)
{
    if (clipArea().contains(e.getPosition()))
        showClipMenu();
}

void AuditionBar::showClipMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("AUDITION CLIPS");
    const auto names = ZSynthProcessor::getBuiltInClipNames();
    const auto current = processor.getAudition().getClipName();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem(i + 1, names[i], true, names[i] == current);
    menu.addSeparator();
    menu.addItem(1000, juce::String::fromUTF8("Load MIDI file\xe2\x80\xa6"));

    juce::Component::SafePointer<AuditionBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(clipArea()))
                           .withMinimumWidth(clipArea().getWidth()),
                       [safe](int result) {
                           if (safe == nullptr || result <= 0)
                               return;
                           if (result == 1000)
                               safe->chooseFile();
                           else
                               safe->processor.loadBuiltInClip(result - 1);
                       });
}

void AuditionBar::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a MIDI file to audition", lastFolder, "*.mid;*.midi;*.smf");
    juce::Component::SafePointer<AuditionBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc) {
                             if (safe == nullptr || fc.getResult() == juce::File())
                                 return;
                             const auto file = fc.getResult();
                             safe->lastFolder = file.getParentDirectory();
                             const auto result = safe->processor.loadAuditionFile(file);
                             if (result.failed())
                                 juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                                                                  .withTitle("ZYNTH").withMessage(result.getErrorMessage()).withButton("OK")
                                                                  .withAssociatedComponent(safe.getComponent()),
                                                              nullptr);
                         });
}

void AuditionBar::paint(juce::Graphics& g)
{
    namespace m = material;
    const auto r = getLocalBounds().toFloat();
    legend.draw(g, "AUDITION", juce::Rectangle<float>(8.0f, 0.0f, (float) kLabelW - 12.0f, r.getHeight()), titleFont(11.5f),
                juce::Justification::centredLeft, Colours::accent.brighter(0.2f), 0.9f);

    // Clip selector: a small display with a drop-down arrow.
    const auto clip = clipArea().toFloat();
    drawLcd(g, clip);
    auto arrow = clip.withTrimmedLeft(clip.getWidth() - 14.0f).reduced(3.0f, clip.getHeight() * 0.36f);
    juce::Path p;
    p.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(Colours::lcdText.withAlpha(0.6f));
    g.fillPath(p);
    const auto& a = processor.getAudition();
    m::glowText(g, shownName.isNotEmpty() ? shownName : juce::String("(no clip)"), clip.withTrimmedRight(16.0f).reduced(8.0f, 0.0f),
                lcdFont(13.0f), Colours::lcdText, juce::Justification::centredLeft, 1.0f);
    m::glassSheen(g, clip, 3.0f, 0.8f);

    // Position: a lit bar in a display window, with beat ticks.
    const auto bar = progressArea().toFloat();
    drawLcd(g, bar);
    const double bpm = a.getClipBpm(), length = a.getClipLength();
    const auto inner = bar.reduced(4.0f, 4.0f);
    if (bpm > 0.0 && length > 0.0)
    {
        const double beats = length * bpm / 60.0;
        const int beatCount = (int) std::round(beats);
        for (int b = 1; b < beatCount; ++b)
        {
            const float x = inner.getX() + inner.getWidth() * (float) b / (float) beats;
            g.setColour(Colours::lcdDim.withAlpha(b % 4 == 0 ? 0.9f : 0.4f));
            g.drawVerticalLine(juce::roundToInt(x), inner.getY() + (b % 4 == 0 ? 0.0f : 3.0f), inner.getBottom() - (b % 4 == 0 ? 0.0f : 3.0f));
        }
    }
    const float pos = a.isPlaying() ? a.getPosition() : 0.0f;
    if (pos > 0.0f)
    {
        const auto lit = inner.withWidth(inner.getWidth() * pos);
        g.setColour(Colours::accent.withAlpha(0.18f));
        g.fillRect(lit);
        g.setColour(Colours::lcdText);
        g.fillRect(lit.getRight() - 1.5f, inner.getY(), 2.0f, inner.getHeight());
    }

    // Clip info on the right.
    juce::String info = bpm > 0.0 ? juce::String(bpm, 0) + " BPM   " + juce::String(length, 1) + " s" : juce::String();
    info += a.isPlaying() ? "   PLAYING" : "";
    hint.draw(g, info.isNotEmpty() ? info : juce::String("load a clip"),
              juce::Rectangle<float>(bar.getRight() + 10.0f, 0.0f, r.getWidth() - bar.getRight() - 14.0f, r.getHeight()),
              labelFont(10.5f), juce::Justification::centredLeft, m::kLegend, 0.5f);
}

} // namespace zsynth::ui
