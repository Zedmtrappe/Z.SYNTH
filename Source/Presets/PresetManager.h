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
        juce::String bank;          // e.g. "ZYNTH Originals", "Classic Machines", "User"
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
        bool favourite = false;
    };

    /** A named factory bank (plain-text .zsbank contents). */
    struct Bank
    {
        juce::String name;
        juce::String text;
    };
    static constexpr const char* kUserBankName = "User";

    /** userDirectory may be empty to use the platform default. */
    PresetManager(juce::AudioProcessorValueTreeState& state, const std::vector<Bank>& factoryBanks,
                  juce::File userDirectory = {});

    /** Bank names in display order (factory banks, then "User"). */
    juce::StringArray getBankNames() const;
    /** Categories (folders) of one bank, in first-appearance order. */
    juce::StringArray getCategories(const juce::String& bank) const;

    /** The per-user ZYNTH folder (settings, favourites, Presets/). */
    static juce::File appDataFolder(const juce::String& name);
    /** Copies the pre-rename "Z.SYNTH" folder to "ZYNTH" if only the old one exists. */
    static void migrateLegacyAppFolder();
    static juce::File defaultUserDirectory();
    juce::File getUserDirectory() const { return userDir; }

    const std::vector<Entry>& getEntries() const { return entries; }
    juce::StringArray getCategories() const;
    const juce::StringArray& getFactoryErrors() const { return factoryErrors; }

    /** Rescans the user folder (call after external changes). */
    void refresh();

    int getCurrentIndex() const { return currentIndex; }
    // Name/category may be read by the host's state calls on any thread.
    juce::String getCurrentName() const { const juce::ScopedLock sl(nameLock); return currentName; }
    juce::String getCurrentCategory() const { const juce::ScopedLock sl(nameLock); return currentCategory; }

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
    void setCurrent(const juce::String& name, const juce::String& category);

    juce::AudioProcessorValueTreeState& state;
    std::vector<PresetData> factory;
    std::vector<juce::String> factoryBank; // bank name per factory preset
    juce::StringArray bankNames;
    juce::StringArray factoryErrors;
    juce::File userDir;
    std::vector<Entry> entries;
    juce::StringArray favourites;
    int currentIndex = -1;
    juce::CriticalSection nameLock;
    juce::String currentName { "Init" };
    juce::String currentCategory { "Init" };
    juce::String lastError;
};

} // namespace zsynth
