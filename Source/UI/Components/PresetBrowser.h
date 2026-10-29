#pragma once

// Floating preset browser. It overlays the panel (the panel layout is never
// replaced or rearranged) and offers search, category filter, favourites,
// load, save-as, rename and delete-with-confirmation.

#include "../../Plugin/ZSynthProcessor.h"
#include "Theme.h"

namespace zsynth::ui
{

class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser(ZSynthProcessor& p);
    ~PresetBrowser() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onClose;

private:
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override { loadRow(row); }
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    void rebuild();
    void loadRow(int row);
    int selectedEntry() const;
    void renameSelected();
    void deleteSelected();
    void showError(const juce::String& message);

    ZSynthProcessor& processor;
    juce::TextEditor search;
    juce::ComboBox category;
    juce::ToggleButton favouritesOnly { "Favourites only" };
    juce::ListBox list { "Presets", this };
    juce::TextButton loadButton { "Load" }, favButton { "Favourite" }, renameButton { "Rename" },
                     deleteButton { "Delete" }, initButton { "Init Patch" }, closeButton { "Close" };
    juce::Label status;
    std::vector<int> filtered;
    std::vector<std::pair<juce::String, juce::String>> folders; // (bank, category) per combo item
};

/** Modal "Save As" dialog (name + category). */
void showSaveAsDialog(ZSynthProcessor& processor, juce::Component* parent);

} // namespace zsynth::ui
