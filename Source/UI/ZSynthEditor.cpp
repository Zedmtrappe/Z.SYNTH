#include "ZSynthEditor.h"

namespace zsynth
{

ZSynthEditor::ZSynthEditor(ZSynthProcessor& p)
    : AudioProcessorEditor(p), processor(p), panel(p)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(panel);

    using L = ui::PanelLayout;
    const float minW = L::kWidth * L::kMinScale, minH = L::kHeight * L::kMinScale;
    setResizable(true, true);
    setResizeLimits((int) minW, (int) minH, (int) (L::kWidth * L::kMaxScale), (int) (L::kHeight * L::kMaxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double) L::kWidth / (double) L::kHeight);

    // Default size: as large as fits comfortably on the main display, at most 1:1.
    float scale = 0.75f;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        scale = juce::jlimit(L::kMinScale, 1.0f, juce::jmin((float) area.getWidth() * 0.92f / L::kWidth,
                                                            (float) (area.getHeight() - 80) / L::kHeight));
    }
    setSize(juce::roundToInt(L::kWidth * scale), juce::roundToInt(L::kHeight * scale));
}

ZSynthEditor::~ZSynthEditor()
{
    setLookAndFeel(nullptr);
}

void ZSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::Colours::panel);
}

void ZSynthEditor::resized()
{
    // Uniform scaling of the fixed canvas: positions and proportions never change.
    const float scale = juce::jmin((float) getWidth() / ui::PanelLayout::kWidth, (float) getHeight() / ui::PanelLayout::kHeight);
    panel.setTransform(juce::AffineTransform::scale(scale));
    panel.setBounds(0, 0, ui::PanelLayout::kWidth, ui::PanelLayout::kHeight);
}

} // namespace zsynth
