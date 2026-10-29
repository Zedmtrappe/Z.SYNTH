// Loads the built Z.SYNTH.vst3 through JUCE's VST3 *hosting* code (the same
// path a DAW takes) and checks: discovery metadata, parameter count, audio
// rendering from MIDI, state save/restore through the VST3 interface, and that
// the editor can be created. Usage: ZSynthVst3HostTest <path/to/Z.SYNTH.vst3>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
    int failures = 0;
    void check(bool ok, const char* what)
    {
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
        if (! ok)
            ++failures;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: %s <path/to/Z.SYNTH.vst3>\n", argv[0]);
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, juce::String(argv[1]));
    check(found.size() == 1, "bundle contains exactly one plug-in");
    if (found.isEmpty())
        return 1;

    const auto& desc = *found[0];
    std::printf("      name='%s' manufacturer='%s' category='%s' version='%s'\n", desc.name.toRawUTF8(),
                desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8(), desc.version.toRawUTF8());
    check(desc.name == "Z.SYNTH", "plug-in name");
    check(desc.isInstrument, "registered as an instrument");

    juce::String error;
    auto instance = format.createInstanceFromDescription(desc, 48000.0, 512, error);
    check(instance != nullptr, ("instantiation " + error).toRawUTF8());
    if (instance == nullptr)
        return 1;

    const int numParams = instance->getParameters().size();
    std::printf("      parameters exposed to the host: %d\n", numParams);
    check(numParams >= 300, "all parameters exposed for automation");
    check(instance->acceptsMidi(), "accepts MIDI");

    instance->setPlayConfigDetails(0, 2, 48000.0, 512);
    instance->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8) 110), 0);
    double energy = 0.0;
    bool finite = true;
    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();
        instance->processBlock(buffer, midi);
        midi.clear();
        if (block == 40)
            midi.addEvent(juce::MidiMessage::noteOff(1, 57), 0);
        energy += buffer.getRMSLevel(0, 0, 512);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                finite = finite && std::isfinite(buffer.getSample(c, i));
    }
    check(energy > 0.05, "renders audio from MIDI through the VST3 interface");
    check(finite, "output is finite");

    // Automate a parameter through the host interface, then round-trip state.
    auto* cutoff = instance->getParameters()[0];
    for (auto* p : instance->getParameters())
        if (p->getName(64) == "Cutoff")
            cutoff = p;
    cutoff->setValueNotifyingHost(0.123f);
    juce::MemoryBlock state;
    instance->getStateInformation(state);
    check(state.getSize() > 100, "state saved");
    cutoff->setValueNotifyingHost(0.9f);
    instance->setStateInformation(state.getData(), (int) state.getSize());
    check(std::abs(cutoff->getValue() - 0.123f) < 0.002f, "state restored (automated parameter recalled)");

    // Editor creation (requires a display; skipped gracefully without one).
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditorIfNeeded());
        check(editor != nullptr, "editor created");
        if (editor != nullptr)
            std::printf("      editor size %d x %d\n", editor->getWidth(), editor->getHeight());
        editor.reset();
    }
    instance->releaseResources();
    instance.reset();

    std::printf("\n%s (%d failures)\n", failures == 0 ? "VST3 host test PASSED" : "VST3 host test FAILED", failures);
    return failures == 0 ? 0 : 1;
}
