// Z.SYNTH standalone application.
//
// Based on JUCE's default standalone wrapper, with three deliberate changes:
//   * settings live in a dedicated folder (Linux: ~/.config/Z.SYNTH/zsynth.settings,
//     macOS: ~/Library/Application Support/Z.SYNTH, Windows: %APPDATA%\Z.SYNTH),
//   * all MIDI inputs are opened automatically, so a connected keyboard plays
//     immediately (they can still be deselected in Options > Audio/MIDI Settings),
//   * the full plug-in state (sound, global settings, MIDI-learn map) is saved
//     on quit and restored on the next launch.
// The audio engine itself knows nothing about this layer.

#include <JuceHeader.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace zsynth
{

class StandaloneApp final : public juce::JUCEApplication
{
public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "zsynth";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config/Z.SYNTH";
       #else
        options.folderName = "Z.SYNTH";
       #endif
        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override { return "Z.SYNTH"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            // Headless: run the engine without a window (still plays MIDI input).
            pluginHolder = createPluginHolder();
            return;
        }

        mainWindow = std::make_unique<juce::StandaloneFilterWindow>(
            getApplicationName(),
            juce::Colour(0xff141518),
            createPluginHolder());
        mainWindow->setVisible(true);
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, [] {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr bool autoOpenMidiDevices = true;
        return std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(), false, juce::String {},
                                                              nullptr, juce::Array<juce::StandalonePluginHolder::PluginInOuts> {},
                                                              autoOpenMidiDevices);
    }

    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace zsynth

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new zsynth::StandaloneApp(); }

#endif
