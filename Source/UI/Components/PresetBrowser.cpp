#include "PresetBrowser.h"

namespace zsynth::ui
{

PresetBrowser::PresetBrowser(ZSynthProcessor& p) : processor(p)
{
    addAndMakeVisible(search);
    search.setTextToShowWhenEmpty("Search presets...", Colours::lcdDim.brighter(0.5f));
    search.setFont(juce::Font(juce::FontOptions(15.0f)));
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { loadRow(list.getSelectedRow()); };

    addAndMakeVisible(category);
    category.onChange = [this] { rebuild(); };

    addAndMakeVisible(favouritesOnly);
    favouritesOnly.onClick = [this] { rebuild(); };

    addAndMakeVisible(list);
    list.setRowHeight(26);
    list.setColour(juce::ListBox::backgroundColourId, Colours::lcdBack);
    list.setColour(juce::ListBox::outlineColourId, Colours::sectionEdge);
    list.setOutlineThickness(1);

    for (auto* b : { &loadButton, &favButton, &renameButton, &deleteButton, &initButton, &closeButton })
        addAndMakeVisible(b);
    loadButton.onClick = [this] { loadRow(list.getSelectedRow()); };
    favButton.onClick = [this] {
        const int e = selectedEntry();
        if (e >= 0)
            processor.getPresets().setFavourite(e, ! processor.getPresets().getEntries()[(size_t) e].favourite);
    };
    renameButton.onClick = [this] { renameSelected(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    initButton.onClick = [this] { processor.getPresets().loadInit(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    addAndMakeVisible(status);
    status.setColour(juce::Label::textColourId, Colours::textDim);
    status.setFont(juce::Font(juce::FontOptions(13.0f)));

    processor.getPresets().addChangeListener(this);
    setWantsKeyboardFocus(true);
    rebuild();
}

PresetBrowser::~PresetBrowser()
{
    processor.getPresets().removeChangeListener(this);
}

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        processor.getPresets().refresh();
        search.grabKeyboardFocus();
    }
}

bool PresetBrowser::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

void PresetBrowser::changeListenerCallback(juce::ChangeBroadcaster*)
{
    rebuild();
}

void PresetBrowser::rebuild()
{
    auto& pm = processor.getPresets();

    // Folder filter: "All", then each bank's folders under a bank heading.
    const int previousId = category.getSelectedId();
    const auto previousFolder = previousId > 1 && previousId - 2 < (int) folders.size() ? folders[(size_t) (previousId - 2)]
                                                                                        : std::pair<juce::String, juce::String> {};
    folders.clear();
    category.clear(juce::dontSendNotification);
    category.addItem("All presets", 1);
    int selectId = 1;
    for (const auto& bank : pm.getBankNames())
    {
        const auto cats = pm.getCategories(bank);
        if (cats.isEmpty())
            continue;
        category.addSectionHeading(bank);
        for (const auto& c : cats)
        {
            folders.push_back({ bank, c });
            const int id = (int) folders.size() + 1;
            category.addItem(c, id);
            if (folders.back() == previousFolder)
                selectId = id;
        }
    }
    category.setSelectedId(selectId, juce::dontSendNotification);
    const bool filterFolder = selectId > 1;
    const auto folder = filterFolder ? folders[(size_t) (selectId - 2)] : std::pair<juce::String, juce::String> {};

    const auto query = search.getText().trim().toLowerCase();
    const auto& entries = pm.getEntries();
    filtered.clear();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        if (filterFolder && (e.bank != folder.first || e.category != folder.second))
            continue;
        if (favouritesOnly.getToggleState() && ! e.favourite)
            continue;
        if (query.isNotEmpty() && ! e.name.toLowerCase().contains(query) && ! e.category.toLowerCase().contains(query)
            && ! e.bank.toLowerCase().contains(query))
            continue;
        filtered.push_back(i);
    }
    list.updateContent();
    for (int r = 0; r < (int) filtered.size(); ++r)
        if (filtered[(size_t) r] == pm.getCurrentIndex())
            list.selectRow(r, false, true);

    int user = 0;
    for (const auto& e : entries)
        user += e.factory ? 0 : 1;
    status.setText(juce::String((int) filtered.size()) + " shown  |  " + juce::String((int) entries.size() - user) + " factory, "
                       + juce::String(user) + " user  |  " + pm.getUserDirectory().getFullPathName(),
                   juce::dontSendNotification);
    repaint();
}

int PresetBrowser::selectedEntry() const
{
    const int row = list.getSelectedRow();
    return row >= 0 && row < (int) filtered.size() ? filtered[(size_t) row] : -1;
}

void PresetBrowser::loadRow(int row)
{
    if (row < 0 || row >= (int) filtered.size())
        return;
    auto& pm = processor.getPresets();
    if (! pm.load(filtered[(size_t) row]))
        showError("Could not load the preset:\n" + pm.getLastError());
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    loadRow(row); // single click auditions the sound immediately
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    loadRow(row);
    if (onClose)
        onClose();
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) filtered.size())
        return;
    const auto& e = processor.getPresets().getEntries()[(size_t) filtered[(size_t) row]];
    if (selected)
        g.fillAll(Colours::accentDim.withAlpha(0.6f));
    else if (row % 2 == 1)
        g.fillAll(juce::Colours::white.withAlpha(0.02f));

    g.setColour(e.favourite ? Colours::accent : Colours::lcdDim);
    g.setFont(juce::Font(juce::FontOptions(15.0f)));
    g.drawText(e.favourite ? juce::String::fromUTF8("\xe2\x98\x85") : juce::String::fromUTF8("\xe2\x98\x86"), 6, 0, 20, height, juce::Justification::centred);
    g.setColour(selected ? juce::Colours::white : Colours::lcdText);
    g.setFont(lcdFont(14.0f));
    g.drawText(e.name, 30, 0, width - 260, height, juce::Justification::centredLeft, true);
    g.setColour(Colours::text.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(e.category, width - 230, 0, 140, height, juce::Justification::centredLeft, true);
    g.setColour(e.factory ? Colours::textDim : Colours::green);
    g.drawText(e.factory ? (e.bank == "Classic Machines" ? "CLASSICS" : "ORIGINALS") : "USER", width - 84, 0, 76, height,
               juce::Justification::centredRight, true);
}

void PresetBrowser::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.translated(0.0f, 6.0f), 10.0f);
    g.setColour(Colours::section);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(Colours::accentDim);
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.5f);
    g.setColour(Colours::accent);
    g.setFont(titleFont(15.0f));
    g.drawText("PRESET BROWSER", getLocalBounds().reduced(20, 12).removeFromTop(24), juce::Justification::centredLeft, false);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced(20, 12);
    r.removeFromTop(30);
    auto top = r.removeFromTop(34);
    search.setBounds(top.removeFromLeft(r.getWidth() / 2));
    top.removeFromLeft(10);
    category.setBounds(top.removeFromLeft(260));
    top.removeFromLeft(10);
    favouritesOnly.setBounds(top);
    r.removeFromTop(10);
    status.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(6);
    auto buttons = r.removeFromBottom(36);
    for (auto* b : { &loadButton, &favButton, &renameButton, &deleteButton, &initButton })
    {
        b->setBounds(buttons.removeFromLeft(118));
        buttons.removeFromLeft(8);
    }
    closeButton.setBounds(buttons.removeFromRight(118));
    r.removeFromBottom(8);
    list.setBounds(r);
}

void PresetBrowser::showError(const juce::String& message)
{
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                                     .withTitle("Z.SYNTH").withMessage(message).withButton("OK").withAssociatedComponent(this),
                                 nullptr);
}

void PresetBrowser::renameSelected()
{
    const int e = selectedEntry();
    auto& pm = processor.getPresets();
    if (e < 0 || pm.getEntries()[(size_t) e].factory)
    {
        showError("Select a user preset to rename. Factory presets are read-only (use Save As to make a copy).");
        return;
    }
    auto* w = new juce::AlertWindow("Rename preset", "New name for '" + pm.getEntries()[(size_t) e].name + "':", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", pm.getEntries()[(size_t) e].name);
    w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBrowser> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, e](int result) {
        if (safe != nullptr && result == 1)
        {
            const auto r = safe->processor.getPresets().renameUser(e, w->getTextEditorContents("name"));
            if (r.failed())
                safe->showError(r.getErrorMessage());
        }
    }), true);
}

void PresetBrowser::deleteSelected()
{
    const int e = selectedEntry();
    auto& pm = processor.getPresets();
    if (e < 0 || pm.getEntries()[(size_t) e].factory)
    {
        showError("Select a user preset to delete. Factory presets cannot be deleted.");
        return;
    }
    const auto name = pm.getEntries()[(size_t) e].name;
    juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle("Delete preset").withMessage("Move the user preset '" + name + "' to the trash?")
                                     .withButton("Delete").withButton("Cancel").withAssociatedComponent(this),
                                 [safe, e](int result) {
                                     if (safe != nullptr && result == 1)
                                     {
                                         const auto r = safe->processor.getPresets().deleteUser(e);
                                         if (r.failed())
                                             safe->showError(r.getErrorMessage());
                                     }
                                 });
}

void showSaveAsDialog(ZSynthProcessor& processor, juce::Component* parent)
{
    auto& pm = processor.getPresets();
    auto* w = new juce::AlertWindow("Save preset as", "Save the current sound as a user preset.", juce::MessageBoxIconType::NoIcon, parent);
    w->addTextEditor("name", pm.getCurrentName() == "Init" ? juce::String("My Sound") : pm.getCurrentName(), "Name:");
    auto cats = pm.getCategories();
    cats.removeString("Init");
    cats.addIfNotAlreadyThere("User");
    w->addComboBox("category", cats, "Category:");
    if (auto* box = w->getComboBoxComponent("category"))
    {
        box->setEditableText(true);
        const int i = cats.indexOf(pm.getCurrentCategory());
        box->setSelectedItemIndex(i >= 0 ? i : cats.indexOf("User"));
    }
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::Component> safeParent(parent);
    w->enterModalState(true, juce::ModalCallbackFunction::create([&processor, w, safeParent](int result) {
        if (result != 1)
            return;
        const auto name = w->getTextEditorContents("name");
        auto* box = w->getComboBoxComponent("category");
        const auto category = box != nullptr ? box->getText() : juce::String("User");
        auto& presets = processor.getPresets();
        auto r = presets.saveUser(name, category, false);
        if (r.failed() && r.getErrorMessage().contains("already exists"))
        {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                                             .withTitle("Overwrite?").withMessage(r.getErrorMessage() + ". Overwrite it?")
                                             .withButton("Overwrite").withButton("Cancel"),
                                         [&processor, name, category](int answer) {
                                             if (answer == 1)
                                             {
                                                 const auto res = processor.getPresets().saveUser(name, category, true);
                                                 if (res.failed())
                                                     juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                                                                                      .withTitle("Save failed").withMessage(res.getErrorMessage()).withButton("OK"), nullptr);
                                             }
                                         });
        }
        else if (r.failed())
        {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                                             .withTitle("Save failed").withMessage(r.getErrorMessage()).withButton("OK"), nullptr);
        }
    }), true);
}

} // namespace zsynth::ui
