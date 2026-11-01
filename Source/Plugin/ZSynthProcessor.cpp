#include "ZSynthProcessor.h"

#include "../UI/ZSynthEditor.h"

#include <BinaryData.h>

namespace zsynth
{

namespace
{
    constexpr int kStateVersion = 1;
    // The sound a new instance (and a first launch of the app) starts with.
    constexpr const char* kStartupPreset = "OB-Xa - Jump";

    std::vector<PresetManager::Bank> factoryBanks()
    {
        return { { "ZYNTH Originals", juce::String::fromUTF8(BinaryData::FactoryBank_zsbank, BinaryData::FactoryBank_zsbankSize) },
                 { "Classic Machines", juce::String::fromUTF8(BinaryData::ClassicsBank_zsbank, BinaryData::ClassicsBank_zsbankSize) } };
    }
}

juce::AudioProcessor::BusesProperties ZSynthProcessor::busesFor(AudioInput input)
{
    auto buses = BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    const bool standalone = juce::PluginHostType::getPluginLoadedAs() == juce::AudioProcessor::wrapperType_Standalone;
    if (input == AudioInput::Enabled || (input == AudioInput::Auto && standalone))
        buses = buses.withInput("Input", juce::AudioChannelSet::stereo(), true);
    return buses;
}

ZSynthProcessor::ZSynthProcessor(juce::File userPresetDir, AudioInput input)
    : AudioProcessor(busesFor(input)),
      apvts(*this, nullptr, "ZSynthParams", createParameterLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        rawValues[(size_t) i] = apvts.getRawParameterValue(paramDef(i).id);
        jassert(rawValues[(size_t) i] != nullptr);
    }

    events.reserve(kMaxEventsPerBlock);
    presets = std::make_unique<PresetManager>(apvts, factoryBanks(), userPresetDir);

    presets->seedDefaultFavourites({ kStartupPreset, "AXEL F", "JP-8000 - Resonant Sweep", "Synthex - Galactic Lead" });

    // Start on the chosen factory sound (falling back to the first preset).
    const auto& entries = presets->getEntries();
    int startup = entries.empty() ? -1 : 0;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].factory && entries[(size_t) i].name == kStartupPreset)
            startup = i;
    if (startup >= 0)
        presets->load(startup);
    loadBuiltInClip(0);

    startTimerHz(30);
}

namespace
{
    struct ClipResource
    {
        juce::String name;
        const char* data;
        int size;
    };

    std::vector<ClipResource> builtInClips()
    {
        std::vector<ClipResource> clips;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String file(BinaryData::originalFilenames[i]);
            if (! file.endsWithIgnoreCase(".mid"))
                continue;
            int size = 0;
            const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
            // "03 Mono Lead.mid" -> "Mono Lead" (the number only orders the menu).
            const auto name = file.upToLastOccurrenceOf(".", false, false).fromFirstOccurrenceOf(" ", false, false);
            clips.push_back({ file, data, size });
            clips.back().name = name.isNotEmpty() ? name : file;
            clips.back().name = file + "|" + clips.back().name;
        }
        std::sort(clips.begin(), clips.end(), [](const ClipResource& a, const ClipResource& b) { return a.name < b.name; });
        for (auto& c : clips)
            c.name = c.name.fromFirstOccurrenceOf("|", false, false);
        return clips;
    }
}

juce::StringArray ZSynthProcessor::getBuiltInClipNames()
{
    juce::StringArray names;
    for (const auto& c : builtInClips())
        names.add(c.name);
    return names;
}

bool ZSynthProcessor::loadBuiltInClip(int index)
{
    const auto clips = builtInClips();
    if (index < 0 || index >= (int) clips.size())
        return false;
    juce::String error;
    auto clip = AuditionPlayer::parse(clips[(size_t) index].data, (size_t) clips[(size_t) index].size, clips[(size_t) index].name, error);
    if (clip == nullptr)
        return false;
    audition.collectGarbage();
    audition.setClip(std::move(clip));
    return true;
}

juce::Result ZSynthProcessor::loadAuditionFile(const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData(data))
        return juce::Result::fail("Could not read " + file.getFullPathName());
    juce::String error;
    auto clip = AuditionPlayer::parse(data.getData(), data.getSize(), file.getFileNameWithoutExtension(), error);
    if (clip == nullptr)
        return juce::Result::fail(file.getFileName() + ": " + error);
    audition.collectGarbage();
    audition.setClip(std::move(clip));
    return juce::Result::ok();
}

ZSynthProcessor::~ZSynthProcessor()
{
    stopTimer();
}

bool ZSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool inputOk = in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    return inputOk && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void ZSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare(sampleRate, std::max(32, samplesPerBlock));
    monoScratch.assign((size_t) std::max(32, samplesPerBlock) * 2, 0.0f);
    trackInput.assign((size_t) std::max(32, samplesPerBlock) * 2, 0.0f);
    tracker.prepare(sampleRate);
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

    // PITCH TRACK: take the instrument from the input channels before the
    // engine renders over them.
    const int inputs = getTotalNumInputChannels();
    const bool tracking = rawValues[(size_t) P::TrkOn]->load() >= 0.5f && inputs > 0 && (int) trackInput.size() >= n;
    if (tracking)
    {
        const int sel = juce::roundToInt(rawValues[(size_t) P::TrkInput]->load());
        const float* a = buffer.getReadPointer(0);
        const float* b = inputs > 1 ? buffer.getReadPointer(1) : a;
        for (int i = 0; i < n; ++i)
            trackInput[(size_t) i] = sel == 0 ? a[i] : (sel == 1 ? b[i] : 0.5f * (a[i] + b[i]));
    }
    for (int ch = 0; ch < inputs && ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, n);

    // The audition clip plays on the receive channel, before the on-screen
    // keyboard merge so its notes light the keys too.
    const int receive = juce::roundToInt(rawValues[(size_t) P::MidiChannel]->load());
    if (! tracking)
        audition.render(midi, n, currentSampleRate, receive <= 0 ? 1 : receive);

    // On-screen keyboard notes join the incoming MIDI (and external notes light the keys).
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    const int pw = pendingPitchWheel.exchange(-1);
    if (pw >= 0)
        midi.addEvent(juce::MidiMessage::pitchWheel(1, pw), 0);
    const int sus = pendingSustain.exchange(-1);
    if (sus >= 0)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, sus), 0);
    const int mw = pendingModWheel.exchange(-1);
    if (mw >= 0)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, mw), 0);

    midiLearn.process(midi);

    events.clear();
    for (const auto metadata : midi)
    {
        if (events.size() >= (size_t) kMaxEventsPerBlock - 32)
            break;
        const auto* raw = metadata.data;
        if (metadata.numBytes < 1 || metadata.numBytes > 3)
            continue;
        // While tracking, the instrument plays the notes: keyboard, QWERTY and
        // MIDI notes (and pitch bend) are ignored; controllers still work.
        const int kind = raw[0] & 0xF0;
        if (tracking && (kind == 0x80 || kind == 0x90 || kind == 0xA0 || kind == 0xE0))
            continue;
        dsp::MidiEvent e;
        e.offset = juce::jlimit(0, n - 1, metadata.samplePosition);
        e.status = raw[0];
        e.data1 = metadata.numBytes > 1 ? raw[1] : 0;
        e.data2 = metadata.numBytes > 2 ? raw[2] : 0;
        events.push_back(e);
    }

    // The tracker's notes join the events, in time order.
    {
        dsp::MidiEvent tracked[32];
        int count = 0;
        const int channel = receive <= 0 ? 1 : receive;
        if (tracking)
        {
            dsp::TrackerSettings s;
            s.inputGainDb = rawValues[(size_t) P::TrkGain]->load();
            s.gateDb = rawValues[(size_t) P::TrkGate]->load();
            s.threshold = rawValues[(size_t) P::TrkThresh]->load();
            s.velocity = rawValues[(size_t) P::TrkVel]->load();
            s.bass = juce::roundToInt(rawValues[(size_t) P::TrkRange]->load()) == 1;
            s.bendFollow = rawValues[(size_t) P::TrkBend]->load() >= 0.5f;
            s.bendRange = rawValues[(size_t) P::BendRange]->load();
            s.channel = channel;
            count = tracker.process(trackInput.data(), n, s, tracked, 32);
        }
        else if (wasTracking)
        {
            count = tracker.release(tracked, 32, channel); // switched off: end the held note
            tracker.reset();
        }
        wasTracking = tracking;
        for (int k = 0; k < count; ++k)
        {
            // Insert in time order (after events at the same offset); no allocation.
            events.push_back(tracked[k]);
            for (size_t j = events.size() - 1; j > 0 && events[j - 1].offset > events[j].offset; --j)
                std::swap(events[j - 1], events[j]);
        }
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

    // DRY: the instrument itself, mixed with the synth.
    if (tracking)
    {
        const float dry = rawValues[(size_t) P::TrkDry]->load() * juce::Decibels::decibelsToGain(rawValues[(size_t) P::TrkGain]->load());
        if (dry > 0.0f)
            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
                buffer.addFrom(ch, 0, trackInput.data(), n, dry);
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
    audition.stop();
    engine.requestPanic();
    keyboardState.allNotesOff(0);
}

void ZSynthProcessor::setPitchWheel(float bipolar)
{
    pendingPitchWheel.store(juce::jlimit(0, 16383, (int) std::lround((bipolar + 1.0f) * 8191.5f)));
}

void ZSynthProcessor::setSustainPedal(bool down)
{
    pendingSustain.store(down ? 127 : 0);
}

void ZSynthProcessor::setModWheel(float unipolar)
{
    pendingModWheel.store(juce::jlimit(0, 127, (int) std::lround(unipolar * 127.0f)));
}

void ZSynthProcessor::timerCallback()
{
    midiLearn.dispatchPending(apvts);
    audition.collectGarbage();
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
