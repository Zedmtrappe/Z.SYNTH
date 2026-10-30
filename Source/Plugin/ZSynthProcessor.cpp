#include "ZSynthProcessor.h"

#include "../UI/ZSynthEditor.h"

#include <BinaryData.h>

namespace zsynth
{

namespace
{
    constexpr int kStateVersion = 1;

    std::vector<PresetManager::Bank> factoryBanks()
    {
        return { { "ZYNTH Originals", juce::String::fromUTF8(BinaryData::FactoryBank_zsbank, BinaryData::FactoryBank_zsbankSize) },
                 { "Classic Machines", juce::String::fromUTF8(BinaryData::ClassicsBank_zsbank, BinaryData::ClassicsBank_zsbankSize) } };
    }
}

ZSynthProcessor::ZSynthProcessor(juce::File userPresetDir)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ZSynthParams", createParameterLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        rawValues[(size_t) i] = apvts.getRawParameterValue(paramDef(i).id);
        jassert(rawValues[(size_t) i] != nullptr);
    }

    events.reserve(kMaxEventsPerBlock);
    presets = std::make_unique<PresetManager>(apvts, factoryBanks(), userPresetDir);

    // Start on a playable factory sound rather than a bare init patch.
    if (! presets->getEntries().empty())
        presets->load(0);

    startTimerHz(30);
}

ZSynthProcessor::~ZSynthProcessor()
{
    stopTimer();
}

bool ZSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ZSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare(sampleRate, std::max(32, samplesPerBlock));
    monoScratch.assign((size_t) std::max(32, samplesPerBlock) * 2, 0.0f);
    keyboardState.reset();
}

void ZSynthProcessor::readTransport(dsp::TransportInfo& t)
{
    t = {};
    if (auto* head = getPlayHead())
    {
        if (const auto pos = head->getPosition())
        {
            if (const auto bpm = pos->getBpm())
            {
                t.bpm = *bpm;
                t.hostTempo = *bpm > 0.0;
            }
            t.playing = pos->getIsPlaying();
            if (const auto ppq = pos->getPpqPosition())
            {
                t.ppqPosition = *ppq;
                t.ppqValid = true;
            }
        }
    }
}

void ZSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const auto startTicks = juce::Time::getHighResolutionTicks();
    audioCallbacks.fetch_add(1, std::memory_order_relaxed);

    // On-screen keyboard notes join the incoming MIDI (and external notes light the keys).
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    const int pw = pendingPitchWheel.exchange(-1);
    if (pw >= 0)
        midi.addEvent(juce::MidiMessage::pitchWheel(1, pw), 0);
    const int mw = pendingModWheel.exchange(-1);
    if (mw >= 0)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, mw), 0);

    midiLearn.process(midi);

    events.clear();
    for (const auto metadata : midi)
    {
        if (events.size() >= (size_t) kMaxEventsPerBlock)
            break;
        const auto* raw = metadata.data;
        if (metadata.numBytes < 1 || metadata.numBytes > 3)
            continue;
        dsp::MidiEvent e;
        e.offset = juce::jlimit(0, n - 1, metadata.samplePosition);
        e.status = raw[0];
        e.data1 = metadata.numBytes > 1 ? raw[1] : 0;
        e.data2 = metadata.numBytes > 2 ? raw[2] : 0;
        events.push_back(e);
    }

    for (int i = 0; i < kNumParams; ++i)
        snapshot[i] = rawValues[(size_t) i]->load(std::memory_order_relaxed);

    dsp::TransportInfo transport;
    readTransport(transport);

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    if (right != nullptr)
    {
        engine.process(left, right, n, snapshot, events.data(), (int) events.size(), transport);
    }
    else
    {
        // Mono output bus: render stereo into a scratch channel prepared in
        // prepareToPlay, then fold down.
        float* scratch = monoScratch.data();
        if ((int) monoScratch.size() < n)
            return; // host exceeded its announced block size: never allocate here
        engine.process(left, scratch, n, snapshot, events.data(), (int) events.size(), transport);
        for (int i = 0; i < n; ++i)
            left[i] = 0.5f * (left[i] + scratch[i]);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, n);
    midi.clear();

    const double elapsed = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - startTicks);
    const double budget = (double) n / currentSampleRate;
    const float load = (float) (elapsed / juce::jmax(1.0e-6, budget));
    cpuLoad.store(cpuLoad.load() * 0.9f + load * 0.1f);
}

juce::AudioProcessorEditor* ZSynthProcessor::createEditor()
{
    return new ZSynthEditor(*this);
}

void ZSynthProcessor::panic()
{
    engine.requestPanic();
    keyboardState.allNotesOff(0);
}

void ZSynthProcessor::setPitchWheel(float bipolar)
{
    pendingPitchWheel.store(juce::jlimit(0, 16383, (int) std::lround((bipolar + 1.0f) * 8191.5f)));
}

void ZSynthProcessor::setModWheel(float unipolar)
{
    pendingModWheel.store(juce::jlimit(0, 127, (int) std::lround(unipolar * 127.0f)));
}

void ZSynthProcessor::timerCallback()
{
    midiLearn.dispatchPending(apvts);
}

// ---- programs: the preset library is exposed to hosts as programs
int ZSynthProcessor::getNumPrograms() { return std::max(1, (int) presets->getEntries().size()); }
int ZSynthProcessor::getCurrentProgram() { return std::max(0, presets->getCurrentIndex()); }
void ZSynthProcessor::setCurrentProgram(int index) { presets->load(index); }
const juce::String ZSynthProcessor::getProgramName(int index)
{
    const auto& e = presets->getEntries();
    return index >= 0 && index < (int) e.size() ? e[(size_t) index].name : juce::String("Init");
}

// ---- state
void ZSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root("ZSynthState");
    root.setAttribute("stateVersion", kStateVersion);
    root.setAttribute("pluginVersion", JucePlugin_VersionString);

    PresetData current = capturePreset(apvts);
    current.name = presets->getCurrentName();
    current.category = presets->getCurrentCategory();
    root.addChildElement(presetToXml(current).release());

    if (auto learnXml = midiLearn.toValueTree().createXml())
        root.addChildElement(learnXml.release());

    copyXmlToBinary(root, destData);
}

void ZSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName("ZSynthState"))
        return; // unknown or damaged state: keep the current sound

    if (auto* presetXml = xml->getChildByName("ZSynthPreset"))
    {
        PresetData p;
        const auto result = presetFromXml(*presetXml, p);
        if (result.ok)
        {
            applyPreset(p, apvts, ApplyScope::Everything);
            presets->setCurrentNameFromState(p.name, p.category);
        }
    }
    if (auto* learn = xml->getChildByName("MidiLearn"))
        midiLearn.fromValueTree(juce::ValueTree::fromXml(*learn));
}

} // namespace zsynth

// Factory function used by every JUCE plug-in wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new zsynth::ZSynthProcessor();
}
