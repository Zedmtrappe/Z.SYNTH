#pragma once

// Preset library: factory bank (embedded, read-only) plus user presets stored
// as individual .zspreset files. Message-thread only; loading a preset sets
// parameters through the APVTS, so the audio thread only ever sees atomic
// parameter changes (never file I/O or allocation).

#include "PresetFormat.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace zsynth
{

class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Entry
    {
        juce::String name, category, author;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
        bool favourite = false;
    };

    /** userDirectory may be empty to use the platform default. */
    PresetManager(juce::AudioProcessorValueTreeState& state, const juce::String& factoryBankText,
                  juce::File userDirectory = {});

    static juce::File defaultUserDirectory();
    juce::File getUserDirectory() const { return userDir; }

    const std::vector<Entry>& getEntries() const { return entries; }
    juce::StringArray getCategories() const;
    const juce::StringArray& getFactoryErrors() const { return factoryErrors; }

    /** Rescans the user folder (call after external changes). */
    void refresh();

    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    juce::String getCurrentCategory() const { return currentCategory; }

    bool load(int index);
    void loadInit();
    void next();
    void previous();

    /** Saves the current sound as a user preset; overwrite=false fails if it exists. */
    juce::Result saveUser(const juce::String& name, const juce::String& category, bool overwrite);
    juce::Result renameUser(int index, const juce::String& newName);
    juce::Result deleteUser(int index);
    void setFavourite(int index, bool favourite);

    /** Restores the displayed name after a session/state recall. */
    void setCurrentNameFromState(const juce::String& name, const juce::String& category);

    juce::String getLastError() const { return lastError; }

    static juce::String sanitiseName(const juce::String& name);

private:
    void loadFavourites();
    void saveFavourites();
    juce::String favouriteKey(const Entry& e) const;
    int findIndexByName(const juce::String& name, bool factory) const;

    juce::AudioProcessorValueTreeState& state;
    std::vector<PresetData> factory;
    juce::StringArray factoryErrors;
    juce::File userDir;
    std::vector<Entry> entries;
    juce::StringArray favourites;
    int currentIndex = -1;
    juce::String currentName { "Init" };
    juce::String currentCategory { "Init" };
    juce::String lastError;
};

} // namespace zsynth
