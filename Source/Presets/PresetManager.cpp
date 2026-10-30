#include "PresetManager.h"

namespace zsynth
{

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s, const std::vector<Bank>& factoryBanks, juce::File userDirectory)
    : state(s), userDir(userDirectory == juce::File() ? defaultUserDirectory() : userDirectory)
{
    for (const auto& bank : factoryBanks)
    {
        bankNames.add(bank.name);
        for (auto& p : parsePresetBank(bank.text, factoryErrors))
        {
            factory.push_back(std::move(p));
            factoryBank.push_back(bank.name);
        }
    }
    bankNames.add(kUserBankName);
    for (const auto& e : factoryErrors)
    {
        juce::ignoreUnused(e);
        DBG("Factory bank: " << e);
    }
    loadFavourites();
    refresh();
}

juce::File PresetManager::appDataFolder(const juce::String& name)
{
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support").getChildFile(name);
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(name);
#endif
}

void PresetManager::migrateLegacyAppFolder()
{
    // The instrument was called "Z.SYNTH" before it became ZYNTH: carry user
    // presets, favourites and standalone settings over on first launch.
    const auto current = appDataFolder("ZYNTH");
    const auto legacy = appDataFolder("Z.SYNTH");
    if (! current.exists() && legacy.isDirectory())
        legacy.copyDirectoryTo(current);
}

juce::File PresetManager::defaultUserDirectory()
{
    migrateLegacyAppFolder();
    return appDataFolder("ZYNTH").getChildFile("Presets");
}

juce::String PresetManager::sanitiseName(const juce::String& name)
{
    auto n = name.trim().removeCharacters("\\/:*?\"<>|").substring(0, 48).trim();
    return n.isEmpty() ? juce::String("Untitled") : n;
}

juce::String PresetManager::favouriteKey(const Entry& e) const
{
    return (e.factory ? "F:" : "U:") + e.name;
}

void PresetManager::setCurrent(const juce::String& name, const juce::String& category)
{
    const juce::ScopedLock sl(nameLock);
    currentName = name;
    currentCategory = category;
}

void PresetManager::refresh()
{
    const auto previousName = getCurrentName();
    const bool previousFactory = currentIndex >= 0 && currentIndex < (int) entries.size() && entries[(size_t) currentIndex].factory;

    entries.clear();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        Entry e;
        e.name = factory[(size_t) i].name;
        e.category = factory[(size_t) i].category;
        e.author = factory[(size_t) i].author;
        e.bank = factoryBank[(size_t) i];
        e.factory = true;
        e.factoryIndex = i;
        entries.push_back(e);
    }

    if (userDir.isDirectory())
    {
        auto files = userDir.findChildFiles(juce::File::findFiles, false, juce::String("*") + kPresetFileExtension);
        files.sort();
        for (const auto& f : files)
        {
            // Only the header is needed for the list; parse lazily but cheaply.
            if (auto xml = juce::XmlDocument::parse(f))
            {
                if (! xml->hasTagName("ZSynthPreset"))
                    continue;
                Entry e;
                e.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
                e.category = xml->getStringAttribute("category", "User");
                e.author = xml->getStringAttribute("author");
                e.bank = kUserBankName;
                e.factory = false;
                e.file = f;
                entries.push_back(e);
            }
        }
    }

    for (auto& e : entries)
        e.favourite = favourites.contains(favouriteKey(e));

    currentIndex = findIndexByName(previousName, previousFactory);
    sendChangeMessage();
}

int PresetManager::findIndexByName(const juce::String& name, bool isFactory) const
{
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == name && entries[(size_t) i].factory == isFactory)
            return i;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == name)
            return i;
    return -1;
}

juce::StringArray PresetManager::getBankNames() const
{
    return bankNames;
}

juce::StringArray PresetManager::getCategories(const juce::String& bank) const
{
    juce::StringArray cats;
    for (const auto& e : entries)
        if (e.bank == bank)
            cats.addIfNotAlreadyThere(e.category);
    return cats;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (const auto& e : entries)
        cats.addIfNotAlreadyThere(e.category);
    return cats;
}

bool PresetManager::load(int index)
{
    if (index < 0 || index >= (int) entries.size())
        return false;
    const Entry& e = entries[(size_t) index];

    PresetData data;
    if (e.factory)
    {
        data = factory[(size_t) e.factoryIndex];
    }
    else
    {
        const auto result = loadPresetFile(e.file, data);
        if (! result.ok)
        {
            lastError = result.error;
            return false;
        }
        lastError = result.warnings.joinIntoString("\n");
    }

    applyPreset(data, state);
    currentIndex = index;
    setCurrent(data.name, data.category);
    sendChangeMessage();
    return true;
}

void PresetManager::loadInit()
{
    PresetData init;
    applyPreset(init, state);
    currentIndex = -1;
    setCurrent("Init", "Init");
    sendChangeMessage();
}

void PresetManager::next()
{
    if (! entries.empty())
        load((currentIndex + 1) % (int) entries.size());
}

void PresetManager::previous()
{
    if (! entries.empty())
        load(currentIndex <= 0 ? (int) entries.size() - 1 : currentIndex - 1);
}

juce::Result PresetManager::saveUser(const juce::String& rawName, const juce::String& category, bool overwrite)
{
    const auto name = sanitiseName(rawName);
    const auto file = userDir.getChildFile(name + kPresetFileExtension);
    if (file.existsAsFile() && ! overwrite)
        return juce::Result::fail("A user preset called '" + name + "' already exists");

    PresetData data = capturePreset(state);
    data.name = name;
    data.category = category.trim().isEmpty() ? juce::String("User") : category.trim();
    data.author = juce::SystemStats::getFullUserName();

    const auto result = savePresetFile(file, data);
    if (result.failed())
    {
        lastError = result.getErrorMessage();
        return result;
    }

    setCurrent(name, data.category);
    refresh();
    currentIndex = findIndexByName(name, false);
    sendChangeMessage();
    return result;
}

juce::Result PresetManager::renameUser(int index, const juce::String& rawName)
{
    if (index < 0 || index >= (int) entries.size() || entries[(size_t) index].factory)
        return juce::Result::fail("Factory presets cannot be renamed");

    const auto name = sanitiseName(rawName);
    const Entry e = entries[(size_t) index];
    const auto target = userDir.getChildFile(name + kPresetFileExtension);
    if (target.existsAsFile() && target != e.file)
        return juce::Result::fail("A user preset called '" + name + "' already exists");

    PresetData data;
    const auto loaded = loadPresetFile(e.file, data);
    if (! loaded.ok)
        return juce::Result::fail(loaded.error);
    data.name = name;
    const auto saved = savePresetFile(target, data);
    if (saved.failed())
        return saved;
    if (target != e.file)
        e.file.deleteFile();

    favourites.removeString(favouriteKey(e));
    if (e.favourite)
        favourites.add("U:" + name);
    saveFavourites();

    if (getCurrentName() == e.name)
        setCurrent(name, getCurrentCategory());
    refresh();
    return juce::Result::ok();
}

juce::Result PresetManager::deleteUser(int index)
{
    if (index < 0 || index >= (int) entries.size() || entries[(size_t) index].factory)
        return juce::Result::fail("Factory presets cannot be deleted");
    const Entry e = entries[(size_t) index];
    if (! e.file.moveToTrash() && ! e.file.deleteFile())
        return juce::Result::fail("Could not delete " + e.file.getFullPathName());
    favourites.removeString(favouriteKey(e));
    saveFavourites();
    if (currentIndex == index)
        currentIndex = -1;
    refresh();
    return juce::Result::ok();
}

void PresetManager::setFavourite(int index, bool favourite)
{
    if (index < 0 || index >= (int) entries.size())
        return;
    auto& e = entries[(size_t) index];
    e.favourite = favourite;
    favourites.removeString(favouriteKey(e));
    if (favourite)
        favourites.add(favouriteKey(e));
    saveFavourites();
    sendChangeMessage();
}

void PresetManager::setCurrentNameFromState(const juce::String& name, const juce::String& category)
{
    setCurrent(name.isEmpty() ? juce::String("Init") : name, category);
    currentIndex = findIndexByName(getCurrentName(), true);
    sendChangeMessage();
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    const auto f = userDir.getParentDirectory().getChildFile("favourites.txt");
    if (f.existsAsFile())
        favourites.addLines(f.loadFileAsString());
    favourites.removeEmptyStrings();
}

void PresetManager::saveFavourites()
{
    const auto f = userDir.getParentDirectory().getChildFile("favourites.txt");
    if (f.getParentDirectory().createDirectory())
        f.replaceWithText(favourites.joinIntoString("\n"));
}

} // namespace zsynth
