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
    // The LOWER part's sound on a new instance.
    constexpr const char* kDefaultLowerPreset = "Classic Saw Bass";

    /** Parameters both parts share (setup, tuning, split, sequencer): always the panel's
        values. MASTER is not one of them: each part has its own output level (and,
        like the setup, a preset load leaves it alone). */
    const std::array<bool, kNumParams>& sharedParams()
    {
        static const auto flags = [] {
            std::array<bool, kNumParams> f {};
            for (int i = 0; i < kNumParams; ++i)
                f[(size_t) i] = isGlobalSetupParam(i) && i != P::MasterVolume;
            return f;
        }();
        return flags;
    }

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
    fromTracker.reserve(kMaxEventsPerBlock);
    presets = std::make_unique<PresetManager>(apvts, factoryBanks(), userPresetDir);
    loadKeepPolicies();
    presets->onLoaded = [this](const PresetData& p) { applyPresetSetup(p); };
    presets->onCapture = [this](PresetData& p) {
        captureSplitPart(p);
        if (includeSongWithPreset)
            p.song = std::make_shared<SongData>(captureSong());
        presetHasSong = includeSongWithPreset;
    };

    presets->seedDefaultFavourites({ kStartupPreset, "AXEL F", "JP-8000 - Resonant Sweep", "Synthex - Galactic Lead" });

    // Start on the chosen factory sound (falling back to the first preset).
    const auto& entries = presets->getEntries();
    int startup = entries.empty() ? -1 : 0;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].factory && entries[(size_t) i].name == kStartupPreset)
            startup = i;
    if (startup >= 0)
        presets->load(startup);

    // The LOWER part starts as a bass (it only sounds once SPLIT or LAYER is chosen).
    {
        PresetData lower;
        if (! presets->getPresetData(presets->findFactory(kDefaultLowerPreset), lower))
            lower = capturePreset(apvts);
        storeSound(lower);
    }
    // The eight pattern memories start empty (every step a rest), unless the
    // startup preset brought a song of its own.
    if (! presetHasSong)
        for (int pi = 0; pi < kPatterns; ++pi)
            capturePattern(pi);
    for (auto& part : noteMap)
        for (auto& ch : part)
            std::fill(std::begin(ch), std::end(ch), (int8_t) -1);
    partEvents[0].reserve(kMaxEventsPerBlock);
    partEvents[1].reserve(kMaxEventsPerBlock);
    harmScratch.resize(kMaxEventsPerBlock);

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

juce::StringArray ZSynthProcessor::getPhraseNames()
{
    juce::StringArray names;
    for (const auto& c : builtInClips())
        names.add(c.name);
    return names;
}

void ZSynthProcessor::setParamValue(int index, float value)
{
    if (auto* prm = apvts.getParameter(paramDef(index).id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(value));
        prm->endChangeGesture();
    }
}

void ZSynthProcessor::capturePattern(int index)
{
    auto& p = patterns[(size_t) juce::jlimit(0, kPatterns - 1, index)];
    for (int k = 0; k < 2 * kPseqRowParams; ++k)
        p[(size_t) k].store(getValue(P::PseqAOn + k), std::memory_order_relaxed);
}

void ZSynthProcessor::applyPattern(int index)
{
    const auto& p = patterns[(size_t) juce::jlimit(0, kPatterns - 1, index)];
    for (int k = 0; k < 2 * kPseqRowParams; ++k)
        setParamValue(P::PseqAOn + k, p[(size_t) k].load(std::memory_order_relaxed));
}

void ZSynthProcessor::fillRows(const float* v, int edit, bool single, dsp::PseqRow rows[2]) const
{
    // v: a pattern's values, starting at PseqAOn (the panel's or a stored pattern).
    for (int r = 0; r < 2; ++r)
    {
        auto at = [&](int field) { return v[r * kPseqRowStride + field - P::PseqAOn]; };
        auto& row = rows[r];
        row.on = at(P::PseqAOn) >= 0.5f;
        const int target = juce::roundToInt(at(P::PseqATarget));
        juce::ignoreUnused(single, edit);
        row.partMask = target == 0 ? 1 : (target == 1 ? 2 : 3); // PLAYS: in every mode
        row.length = juce::roundToInt(at(P::PseqALen));
        row.direction = juce::roundToInt(at(P::PseqADir));
        row.stepBeats = pseqStepBeats(juce::roundToInt(at(P::PseqADiv)));
        row.gate = at(P::PseqAGateLen) * 0.01f;
        row.root = 0; // steps hold absolute notes
        row.velocity = juce::roundToInt(at(P::PseqAVel));
        for (int st = 0; st < kPseqSteps; ++st)
        {
            const int f = P::PseqAPitch1 + st * kPseqStepStride;
            row.steps[st].pitch = juce::roundToInt(at(f));
            row.steps[st].gate = at(f + 1) >= 0.5f;
            row.steps[st].accent = at(f + 2) >= 0.5f;
            row.steps[st].slide = at(f + 3) >= 0.5f;
        }
    }
}

void ZSynthProcessor::selectPattern(int index)
{
    index = juce::jlimit(0, kPatterns - 1, index);
    if (index == currentPattern)
        return;
    capturePattern(currentPattern);
    applyingPattern.store(true);
    currentPattern = index;
    applyPattern(index);
    applyingPattern.store(false);
}

void ZSynthProcessor::copyRow(int row)
{
    const int base = P::PseqAOn + juce::jlimit(0, 1, row) * kPseqRowStride;
    for (int k = 0; k < kPseqRowParams; ++k)
        rowClipboard[(size_t) k] = getValue(base + k);
    rowClipboardValid = true;
}

void ZSynthProcessor::pasteRow(int row)
{
    if (! rowClipboardValid)
        return;
    const int base = P::PseqAOn + juce::jlimit(0, 1, row) * kPseqRowStride;
    for (int k = 0; k < kPseqRowParams; ++k)
        if (k != P::PseqATarget - P::PseqAOn) // the row keeps its own routing
            setParamValue(base + k, rowClipboard[(size_t) k]);
}

void ZSynthProcessor::clearRow(int row)
{
    // Every step back to a plain gate on the row's home note (the row's setup stays).
    const int base = P::PseqAOn + juce::jlimit(0, 1, row) * kPseqRowStride;
    for (int st = 0; st < kPseqSteps; ++st)
    {
        const int s0 = base + (P::PseqAPitch1 - P::PseqAOn) + st * kPseqStepStride;
        setParamValue(s0, paramDef(s0).def);
        setParamValue(s0 + 1, 1.0f);
        setParamValue(s0 + 2, 0.0f);
        setParamValue(s0 + 3, 0.0f);
    }
}

void ZSynthProcessor::importIntoRow(const AuditionPlayer::Clip& clip, int row)
{
    // Quantise the clip's opening bars to the row's steps: row A takes the top
    // line (highest note starting in each step), row B the bass line (lowest).
    row = juce::jlimit(0, 1, row);
    const int base = P::PseqAOn + row * kPseqRowStride;
    const int off = base - P::PseqAOn;
    const double stepBeats = pseqStepBeats(juce::roundToInt(getValue(P::PseqADiv + off)));
    const double beatsPerSecond = (clip.bpm > 0.0 ? clip.bpm : 120.0) / 60.0;
    struct Note { double start, end; int note, vel; };
    std::vector<Note> notes;
    for (size_t i = 0; i < clip.events.size(); ++i)
    {
        const auto& e = clip.events[i];
        if ((e.status & 0xF0) != 0x90 || e.data2 == 0)
            continue;
        double end = clip.lengthSeconds;
        for (size_t j = i + 1; j < clip.events.size(); ++j)
        {
            const auto& f = clip.events[j];
            if (f.data1 == e.data1 && ((f.status & 0xF0) == 0x80 || ((f.status & 0xF0) == 0x90 && f.data2 == 0)))
            {
                end = f.seconds;
                break;
            }
        }
        notes.push_back({ e.seconds * beatsPerSecond, end * beatsPerSecond, e.data1, e.data2 });
    }
    if (notes.empty())
        return;
    const int length = juce::jlimit(1, kPseqSteps, (int) std::ceil(clip.lengthSeconds * beatsPerSecond / stepBeats - 1.0e-6));
    int chosen[kPseqSteps];
    int velocity[kPseqSteps] {};
    bool tie[kPseqSteps] {};
    int lowest = 127;
    for (int st = 0; st < kPseqSteps; ++st)
    {
        chosen[st] = -1;
        const double a = st * stepBeats, b = a + stepBeats;
        for (const auto& nt : notes)
            if (nt.start >= a - 1.0e-6 && nt.start < b - 1.0e-6)
                if (chosen[st] < 0 || (row == 0 ? nt.note > chosen[st] : nt.note < chosen[st]))
                {
                    chosen[st] = nt.note;
                    velocity[st] = nt.vel;
                }
        if (chosen[st] < 0 && st > 0 && chosen[st - 1] >= 0)
        {
            // Nothing starts here, but the previous note is still sounding: hold it.
            for (const auto& nt : notes)
                if (nt.note == chosen[st - 1] && nt.start < a && nt.end > a + stepBeats * 0.5)
                {
                    chosen[st] = chosen[st - 1];
                    velocity[st] = velocity[st - 1];
                    tie[st] = true;
                    break;
                }
        }
        if (chosen[st] >= 0 && st < length)
            lowest = std::min(lowest, chosen[st]);
    }
    if (lowest == 127)
        return;
    setParamValue(base + (P::PseqALen - P::PseqAOn), (float) length);
    for (int st = 0; st < kPseqSteps; ++st)
    {
        const int s0 = base + (P::PseqAPitch1 - P::PseqAOn) + st * kPseqStepStride;
        // Rests keep the lowest note, so the keyboard opens near the phrase.
        int pitch = chosen[st] >= 0 ? chosen[st] : lowest;
        while (pitch > 108)
            pitch -= 12;
        while (pitch < 21)
            pitch += 12;
        const bool held = st + 1 < kPseqSteps && tie[st + 1];
        setParamValue(s0, (float) pitch);
        setParamValue(s0 + 1, chosen[st] >= 0 ? 1.0f : 0.0f);
        setParamValue(s0 + 2, chosen[st] >= 0 && velocity[st] >= 110 ? 1.0f : 0.0f);
        setParamValue(s0 + 3, held ? 1.0f : 0.0f);
    }
}

bool ZSynthProcessor::loadPhraseIntoRow(int phrase, int row)
{
    const auto clips = builtInClips();
    if (phrase < 0 || phrase >= (int) clips.size())
        return false;
    juce::String error;
    auto clip = AuditionPlayer::parse(clips[(size_t) phrase].data, (size_t) clips[(size_t) phrase].size, clips[(size_t) phrase].name, error);
    if (clip == nullptr)
        return false;
    importIntoRow(*clip, row);
    return true;
}

juce::StringArray ZSynthProcessor::getFactorySongNames()
{
    juce::StringArray names;
    for (const auto& song : factorySongs())
        names.add(song.name);
    return names;
}

bool ZSynthProcessor::loadFactorySong(int index)
{
    const auto& songs = factorySongs();
    if (index < 0 || index >= (int) songs.size())
        return false;
    applySong(songs[(size_t) index]);
    return true;
}

juce::File ZSynthProcessor::getSongDirectory() const
{
    return presets->getUserDirectory().getParentDirectory().getChildFile("Songs");
}

juce::Result ZSynthProcessor::loadSongFile(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    SongData song;
    if (xml == nullptr || ! songFromXml(*xml, song))
        return juce::Result::fail("Not a ZYNTH song: " + file.getFileName());
    if (song.name.isEmpty())
        song.name = file.getFileNameWithoutExtension();
    applySong(song);
    return juce::Result::ok();
}

juce::Result ZSynthProcessor::saveSongFile(const juce::File& file)
{
    auto song = captureSong();
    song.name = file.getFileNameWithoutExtension();
    file.getParentDirectory().createDirectory();
    if (! songToXml(song)->writeTo(file))
        return juce::Result::fail("Could not write " + file.getFullPathName());
    songName = song.name;
    return juce::Result::ok();
}

SongData ZSynthProcessor::captureSong() const
{
    SongData song;
    song.name = songName;
    song.songMode = getValue(P::PseqSongOn) >= 0.5f;
    song.tempo = getValue(P::Tempo);
    song.swing = getValue(P::PseqSwing);
    for (int pi = 0; pi < kPatterns; ++pi)
        for (int k = 0; k < 2 * kPseqRowParams; ++k)
            song.patterns[(size_t) pi][(size_t) k] = pi == currentPattern.load() ? getValue(P::PseqAOn + k)
                                                                                 : patterns[(size_t) pi][(size_t) k].load();
    for (int i = 0; i < SongData::kSlots; ++i)
    {
        song.slots[(size_t) i] = juce::roundToInt(getValue(P::PseqSlot1 + i));
        song.repeats[(size_t) i] = juce::roundToInt(getValue(P::PseqRep1 + i));
    }
    return song;
}

void ZSynthProcessor::clearSong()
{
    setParamValue(P::PseqRun, 0.0f);
    applySong(blankSong(), false);
}

void ZSynthProcessor::applySong(const SongData& song, bool withTempo)
{
    for (int pi = 0; pi < kPatterns; ++pi)
        for (int k = 0; k < 2 * kPseqRowParams; ++k)
            patterns[(size_t) pi][(size_t) k].store(song.patterns[(size_t) pi][(size_t) k]);
    applyingPattern.store(true);
    currentPattern = 0;
    applyPattern(0);
    applyingPattern.store(false);
    for (int i = 0; i < SongData::kSlots; ++i)
    {
        setParamValue(P::PseqSlot1 + i, (float) song.slots[(size_t) i]);
        setParamValue(P::PseqRep1 + i, (float) song.repeats[(size_t) i]);
    }
    if (withTempo)
    {
        setParamValue(P::Tempo, song.tempo);
        setParamValue(P::PseqSwing, song.swing);
    }
    setParamValue(P::PseqSongOn, song.songMode ? 1.0f : 0.0f);
    songName = song.name;
    restartSequencer();
}

juce::Result ZSynthProcessor::loadMidiFileIntoRow(const juce::File& file, int row)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData(data))
        return juce::Result::fail("Could not read " + file.getFullPathName());
    juce::String error;
    auto clip = AuditionPlayer::parse(data.getData(), data.getSize(), file.getFileNameWithoutExtension(), error);
    if (clip == nullptr)
        return juce::Result::fail(file.getFileName() + ": " + error);
    importIntoRow(*clip, row);
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
    mixLimiterGain = 1.0f;
    pseq.prepare(sampleRate);
    mixLimiterRelease = (float) std::exp(-1.0 / (0.08 * sampleRate));
    for (auto& e : engines)
        e.prepare(sampleRate, std::max(32, samplesPerBlock));
    monoScratch.assign((size_t) std::max(32, samplesPerBlock) * 2, 0.0f);
    partL.assign((size_t) std::max(32, samplesPerBlock), 0.0f);
    partR.assign((size_t) std::max(32, samplesPerBlock), 0.0f);
    partTail[0] = partTail[1] = 0;
    for (auto& h : harmonizers)
        h.prepare(sampleRate);
    keyDetector.prepare(sampleRate);
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

    const int receive = juce::roundToInt(rawValues[(size_t) P::MidiChannel]->load());

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
    fromTracker.clear();
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
        fromTracker.push_back(0);
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
            s.cleanUp = rawValues[(size_t) P::TrkClean]->load();
            s.sustain = rawValues[(size_t) P::TrkSustain]->load();
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
            fromTracker.push_back(1);
            for (size_t j = events.size() - 1; j > 0 && events[j - 1].offset > events[j].offset; --j)
            {
                std::swap(events[j - 1], events[j]);
                std::swap(fromTracker[j - 1], fromTracker[j]);
            }
        }
    }

    // Both parts' parameters. The panel holds the edited part; the other comes from
    // `stored`. If setEditPart is swapping them right now, keep last block's values.
    {
        const uint32_t seq = swapSequence.load(std::memory_order_acquire);
        if ((seq & 1u) == 0)
        {
            const int edit = editPart.load(std::memory_order_acquire);
            for (int i = 0; i < kNumParams; ++i)
                snapshot[i] = rawValues[(size_t) i]->load(std::memory_order_relaxed);
            const auto& shared = sharedParams();
            float other[kNumParams];
            for (int i = 0; i < kNumParams; ++i)
                other[i] = shared[(size_t) i] ? snapshot[i] : stored[(size_t) i].load(std::memory_order_relaxed);
            if (swapSequence.load(std::memory_order_acquire) == seq)
            {
                std::memcpy(partParams[edit], snapshot, sizeof(snapshot));
                std::memcpy(partParams[1 - edit], other, sizeof(other));
            }
        }
    }
    dsp::TransportInfo transport;
    readTransport(transport);

    // PATTERN SEQUENCER: is it running? With KEY TRANSPOSE, played keys transpose it
    // (C4 = 0) instead of sounding.
    const auto* gp = partParams[getEditPart()];
    const bool hostDriven = gp[P::PseqSync] >= 0.5f && transport.ppqValid;
    const bool seqOn = hostDriven ? transport.playing : gp[P::PseqRun] >= 0.5f;
    seqRunning.store(seqOn, std::memory_order_relaxed);
    if (seqOn && gp[P::PseqKeyTr] >= 0.5f)
    {
        size_t w = 0;
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& e = events[i];
            const int kind = e.status & 0xF0;
            if (kind == 0x90 && e.data2 > 0)
                pseqTranspose = juce::jlimit(-36, 36, (int) e.data1 - 60);
            if (kind == 0x80 || kind == 0x90)
                continue;
            fromTracker[w] = fromTracker[i];
            events[w++] = e;
        }
        events.resize(w);
        fromTracker.resize(w);
    }

    keyDetector.observe(events.data(), (int) events.size(), n);
    routeEvents(partParams[getEditPart()]);

    {
        // The sequencer's notes join each part's events, in time order: each row
        // plays the part(s) its PLAYS chooses, whatever the keyboard mode.
        struct PartSink : dsp::PatternSequencer::Sink
        {
            std::array<std::vector<dsp::MidiEvent>, 2>& ev;
            explicit PartSink(std::array<std::vector<dsp::MidiEvent>, 2>& e) : ev(e) {}
            void add(int part, const dsp::MidiEvent& e) override
            {
                auto& list = ev[(size_t) part];
                if (list.size() >= list.capacity())
                    return; // never allocate on the audio thread
                list.push_back(e);
                for (size_t j = list.size() - 1; j > 0 && list[j - 1].offset > list[j].offset; --j)
                    std::swap(list[j - 1], list[j]);
            }
        } sink(partEvents);
        dsp::PseqSettings ps;
        ps.run = gp[P::PseqRun] >= 0.5f;
        ps.hostSync = gp[P::PseqSync] >= 0.5f;
        ps.bpm = transport.hostTempo ? transport.bpm : gp[P::Tempo];
        ps.swing = gp[P::PseqSwing] * 0.01f;
        ps.transpose = gp[P::PseqKeyTr] >= 0.5f ? pseqTranspose : 0;
        ps.channel = receive <= 0 ? 0 : receive - 1;
        const bool single = (SplitMode) juce::roundToInt(gp[P::SplitMode]) == SplitMode::Single;
        fillRows(gp + P::PseqAOn, getEditPart(), single, ps.rows);
        // SONG: the slots choose the patterns; the one on the panel plays as edited, the others from memory.
        struct Song : dsp::PseqSong
        {
            const ZSynthProcessor& owner;
            const float* g;
            bool single;
            Song(const ZSynthProcessor& o, const float* globals, bool s) : owner(o), g(globals), single(s) {}
            int slot(int index) const override
            {
                const int v = juce::roundToInt(g[P::PseqSlot1 + index]);
                return v == 0 ? kEmpty : (v <= 8 ? v - 1 : (v == 9 ? kLoop : kEnd));
            }
            int repeats(int index) const override { return juce::roundToInt(g[P::PseqRep1 + index]) + 1; }
            void rowsOf(int pattern, dsp::PseqRow rows[2]) const override
            {
                if (pattern == owner.currentPattern.load() && ! owner.applyingPattern.load())
                {
                    owner.fillRows(g + P::PseqAOn, owner.getEditPart(), single, rows);
                    return;
                }
                float block[2 * kPseqRowParams];
                const auto& stored = owner.patterns[(size_t) juce::jlimit(0, kPatterns - 1, pattern)];
                for (int k = 0; k < 2 * kPseqRowParams; ++k)
                    block[k] = stored[(size_t) k].load(std::memory_order_relaxed);
                owner.fillRows(block, owner.getEditPart(), single, rows);
            }
        } song(*this, gp, single);
        if (gp[P::PseqSongOn] >= 0.5f)
            ps.song = &song;
        pseq.process(n, ps, transport, sink);

        // Note preview: a step's note chosen on the page sounds for 0.3 s on its row's sound.
        const int req = previewRequest.exchange(-1);
        auto send = [&](int mask, int offset, int kind, int note, int vel) {
            dsp::MidiEvent e;
            e.offset = offset;
            e.status = (uint8_t) (kind | ps.channel);
            e.data1 = (uint8_t) note;
            e.data2 = (uint8_t) vel;
            for (int part = 0; part < 2; ++part)
                if ((mask >> part) & 1)
                    sink.add(part, e);
        };
        if (previewNote >= 0 && (req >= 0 || previewLeft <= n))
        {
            send(previewMask, req >= 0 ? 0 : juce::jmax(0, previewLeft - 1), 0x80, previewNote, 0);
            previewNote = -1;
        }
        else if (previewNote >= 0)
        {
            previewLeft -= n;
        }
        if (req >= 0)
        {
            const auto& row = ps.rows[req / 128];
            previewNote = req % 128;
            previewMask = row.partMask;
            previewLeft = (int) (0.3 * currentSampleRate);
            send(previewMask, 0, 0x90, previewNote, row.velocity);
        }
    }
    // HARMONY: on everything each part plays (keys, PITCH TRACK and the sequencer).
    harmonise(n, partParams[getEditPart()]);

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    std::fill(left, left + n, 0.0f);
    if (right != nullptr)
        std::fill(right, right + n, 0.0f);
    if ((int) partL.size() < n)
        return; // host exceeded its announced block size: never allocate here

    const int tail = (int) (currentSampleRate * 15.0); // reverb and delay tails of a part that stopped playing
    for (int part = 0; part < 2; ++part)
    {
        auto& ev = partEvents[(size_t) part];
        auto& eng = engines[(size_t) part];
        if (! ev.empty() || eng.getActiveVoices() > 0)
        {
            if (partTail[part] <= 0)
                eng.snapParameters(); // waking up: settings changed while idle apply at once
            partTail[part] = tail;
        }
        else if (partTail[part] <= 0)
            continue; // silent: skip it entirely
        else
            partTail[part] -= n;

        eng.process(partL.data(), partR.data(), n, partParams[part], ev.data(), (int) ev.size(), transport);

        // (Each part's level is its own MASTER; the split page's part levels are retired.)
        const float target = 1.0f;
        const float g0 = partGain[part], step = (target - g0) / (float) n;
        partGain[part] = target;
        if (right != nullptr)
        {
            for (int i = 0; i < n; ++i)
            {
                const float k = g0 + step * (float) (i + 1);
                left[i] += partL[(size_t) i] * k;
                right[i] += partR[(size_t) i] * k;
            }
        }
        else
        {
            // Mono output bus: fold down.
            for (int i = 0; i < n; ++i)
                left[i] += 0.5f * (partL[(size_t) i] + partR[(size_t) i]) * (g0 + step * (float) (i + 1));
        }
    }

    // SPLIT / LAYER: each part is limited on its own, but two loud parts can still
    // add up past full scale. A last limiter on the mix catches that (ceiling
    // -0.3 dBFS, instant attack, 80 ms release); it does nothing below the ceiling.
    // (In SINGLE the sequencer can play both sounds too, so it always runs.)
    {
        constexpr float ceiling = 0.966f;
        for (int i = 0; i < n; ++i)
        {
            const float p = std::max(std::abs(left[i]), right != nullptr ? std::abs(right[i]) : 0.0f);
            const float needed = p > ceiling ? ceiling / p : 1.0f;
            mixLimiterGain = needed < mixLimiterGain ? needed : needed + (mixLimiterGain - needed) * mixLimiterRelease;
            left[i] *= mixLimiterGain;
            if (right != nullptr)
                right[i] *= mixLimiterGain;
        }
        if (mixLimiterGain > 0.9999f)
            mixLimiterGain = 1.0f;
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
    for (auto& e : engines)
        e.requestPanic();
    clearNoteMaps.store(true);
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

// ---- keyboard split / layer
void ZSynthProcessor::routeEvents(const float* g)
{
    if (clearNoteMaps.exchange(false))
        for (auto& part : noteMap)
            for (auto& ch : part)
                std::fill(std::begin(ch), std::end(ch), (int8_t) -1);

    const auto keyMode = (SplitMode) juce::roundToInt(g[P::SplitMode]);
    const int point = juce::roundToInt(g[P::SplitPoint]);
    const int octave[2] = { juce::roundToInt(g[P::SplitUpOct]), juce::roundToInt(g[P::SplitLowOct]) };
    const int edit = getEditPart();
    partEvents[0].clear();
    partEvents[1].clear();

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& e = events[i];
        // PITCH TRACK always plays the part that is selected, as SINGLE does.
        const bool tracked = i < fromTracker.size() && fromTracker[i] != 0;
        const auto mode = tracked ? SplitMode::Single : keyMode;
        const int kind = e.status & 0xF0, ch = e.status & 0x0F, key = e.data1 & 0x7F;
        const bool on = kind == 0x90 && e.data2 > 0;
        const bool off = kind == 0x80 || (kind == 0x90 && e.data2 == 0);
        if (on && learningSplit.load(std::memory_order_relaxed))
        {
            // LEARN: this key becomes the split point (applied on the message thread); it does not sound.
            learnedNote.store(key);
            learningSplit.store(false);
            continue;
        }
        if (on)
        {
            for (int part = 0; part < 2; ++part)
            {
                const bool plays = mode == SplitMode::Layer || (mode == SplitMode::Single ? part == edit
                                                                                           : (key < point) == (part == kLower));
                if (! plays)
                    continue;
                const int sounding = mode == SplitMode::Single ? key : juce::jlimit(0, 127, key + 12 * octave[part]);
                noteMap[part][ch][key] = (int8_t) sounding;
                auto routed = e;
                routed.data1 = (uint8_t) sounding;
                partEvents[(size_t) part].push_back(routed);
            }
        }
        else if (off || kind == 0xA0)
        {
            // Releases (and poly pressure) follow the note to wherever it went.
            for (int part = 0; part < 2; ++part)
            {
                const int sounding = noteMap[part][ch][key];
                if (sounding < 0)
                    continue;
                auto routed = e;
                routed.data1 = (uint8_t) sounding;
                partEvents[(size_t) part].push_back(routed);
                if (off)
                    noteMap[part][ch][key] = -1;
            }
        }
        else
        {
            // Controllers, pitch bend, channel pressure: in SINGLE (and from PITCH TRACK)
            // the selected part; with SPLIT / LAYER, both. All-notes-off: both, always.
            const bool allOff = kind == 0xB0 && (e.data1 == 120 || e.data1 == 123);
            if (allOff)
                for (auto& part : noteMap)
                    std::fill(std::begin(part[ch]), std::end(part[ch]), (int8_t) -1);
            for (int part = 0; part < 2; ++part)
                if (allOff || mode != SplitMode::Single || part == edit)
                    partEvents[(size_t) part].push_back(e);
        }
    }
}

void ZSynthProcessor::harmonise(int n, const float* g)
{
    dsp::HarmonySettings hs;
    hs.key = juce::roundToInt(g[P::HarmKey]);
    hs.scale = (HarmScale) juce::roundToInt(g[P::HarmScale]);
    hs.voices = juce::roundToInt(g[P::HarmVoices]);
    const int ints[3] = { P::HarmV1Int, P::HarmV2Int, P::HarmV3Int }, dirs[3] = { P::HarmV1Dir, P::HarmV2Dir, P::HarmV3Dir };
    for (int v = 0; v < 3; ++v)
    {
        hs.interval[v] = (HarmInterval) juce::roundToInt(g[ints[v]]);
        hs.down[v] = juce::roundToInt(g[dirs[v]]) == 1;
    }
    hs.outOfKey = (HarmOutOfKey) juce::roundToInt(g[P::HarmOutKey]);
    hs.topNoteOnly = juce::roundToInt(g[P::HarmMode]) == 1;
    hs.level = g[P::HarmLevel];
    hs.strumMs = g[P::HarmStrum];
    hs.lowest = juce::roundToInt(g[P::HarmLow]);
    hs.highest = juce::roundToInt(g[P::HarmHigh]);

    const bool on = g[P::HarmOn] >= 0.5f;
    const int applies = juce::roundToInt(g[P::HarmPart]); // 0 both, 1 upper, 2 lower
    for (int part = 0; part < 2; ++part)
    {
        const bool wanted = on && (applies == 0 || (applies == 1) == (part == kUpper));
        auto& ev = partEvents[(size_t) part];
        auto& h = harmonizers[(size_t) part];
        const int cap = (int) std::min(harmScratch.size(), ev.capacity());
        int c = 0;
        if (wanted)
        {
            c = h.process(ev.data(), (int) ev.size(), n, hs, harmScratch.data(), cap);
        }
        else if (h.isActive())
        {
            // Switched off (or no longer this part): release what it played, then pass the block on.
            c = h.flush(harmScratch.data(), cap);
            for (const auto& e : ev)
                if (c < cap)
                    harmScratch[(size_t) c++] = e;
        }
        else
        {
            continue;
        }
        ev.assign(harmScratch.begin(), harmScratch.begin() + c);
    }
}

bool ZSynthProcessor::applyDetectedKey()
{
    const int found = keyDetector.takeResult();
    if (found < 0)
        return false;
    auto set = [this](const char* id, float v) {
        if (auto* prm = apvts.getParameter(id))
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    set("harm.key", (float) (found % 12));
    set("harm.scale", (float) (found >= 12 ? HarmScale::NaturalMinor : HarmScale::Major));
    return true;
}

void ZSynthProcessor::storeSound(const PresetData& p)
{
    for (int i = 0; i < kNumParams; ++i)
        stored[(size_t) i].store(p.values[(size_t) i], std::memory_order_relaxed);
    const juce::ScopedLock sl(storedLock);
    storedName = p.name;
    storedCategory = p.category;
}

void ZSynthProcessor::applyPresetSetup(const PresetData& p)
{
    // A preset saved with its sequencer song brings it; others leave the sequencer alone.
    presetHasSong = p.song != nullptr;
    if (p.song != nullptr)
        applySong(*p.song);
    // Loading a preset switches the keyboard split and the harmoniser off,
    // unless the preset was saved with them on: then their settings come back
    // with it (a split preset also brings the other part's sound).
    auto set = [this](int index, float value) {
        if (auto* prm = apvts.getParameter(paramDef(index).id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(value));
            prm->endChangeGesture();
        }
    };
    // The preset has none of its own: keep, switch off or (for the editor) ask.
    auto noneInPreset = [this, &set](KeepItem item, int param, float offValue, bool engaged) {
        if (! engaged)
            return;
        switch (getKeepPolicy(item))
        {
            case KeepPolicy::Keep:    break;
            case KeepPolicy::TurnOff: set(param, offValue); break;
            case KeepPolicy::Ask:     keepQuestion.fetch_or(item); break; // kept until answered
        }
    };
    if ((SplitMode) juce::roundToInt(p.values[(size_t) P::SplitMode]) != SplitMode::Single)
    {
        for (int i = P::SplitMode; i <= P::SplitUpVol; ++i)
            set(i, p.values[(size_t) i]);
        if (p.splitPart != nullptr)
        {
            // The panel already holds the preset's main sound: that part is the
            // one it was saved from, and the other part gets the stored sound.
            swapSequence.fetch_add(1, std::memory_order_acq_rel);
            storeSound(*p.splitPart);
            editPart.store(juce::jlimit(0, 1, p.editPart), std::memory_order_release);
            swapSequence.fetch_add(1, std::memory_order_acq_rel);
        }
    }
    else
    {
        noneInPreset(kKeepSplit, P::SplitMode, (float) SplitMode::Single,
                     (SplitMode) juce::roundToInt(getValue(P::SplitMode)) != SplitMode::Single);
    }
    if (p.values[(size_t) P::HarmOn] >= 0.5f)
        for (int i = P::HarmOn; i <= P::HarmPart; ++i)
            set(i, p.values[(size_t) i]);
    else
        noneInPreset(kKeepHarmony, P::HarmOn, 0.0f, getValue(P::HarmOn) >= 0.5f);
}

void ZSynthProcessor::turnOff(KeepItem item)
{
    const int index = item == kKeepSplit ? (int) P::SplitMode : (int) P::HarmOn;
    if (auto* prm = apvts.getParameter(paramDef(index).id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(item == kKeepSplit ? (float) SplitMode::Single : 0.0f));
        prm->endChangeGesture();
    }
}

juce::File ZSynthProcessor::keepPolicyFile() const
{
    return presets->getUserDirectory().getParentDirectory().getChildFile("preset-change.txt");
}

void ZSynthProcessor::loadKeepPolicies()
{
    const auto lines = juce::StringArray::fromLines(keepPolicyFile().loadFileAsString());
    auto parse = [&](const char* key) {
        for (const auto& l : lines)
            if (l.upToFirstOccurrenceOf("=", false, false).trim() == key)
            {
                const auto v = l.fromFirstOccurrenceOf("=", false, false).trim();
                return v == "keep" ? 1 : (v == "off" ? 2 : 0);
            }
        return 0;
    };
    splitPolicy = parse("split");
    harmPolicy = parse("harmony");
}

void ZSynthProcessor::setKeepPolicy(KeepItem item, KeepPolicy policy)
{
    (item == kKeepSplit ? splitPolicy : harmPolicy).store((int) policy);
    auto word = [](int v) { return v == 1 ? "keep" : (v == 2 ? "off" : "ask"); };
    const auto f = keepPolicyFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::String("split = ") + word(splitPolicy.load()) + "\nharmony = " + word(harmPolicy.load()) + "\n");
}

void ZSynthProcessor::captureSplitPart(PresetData& p)
{
    if ((SplitMode) juce::roundToInt(getValue(P::SplitMode)) == SplitMode::Single)
        return;
    auto other = std::make_shared<PresetData>();
    for (int i = 0; i < kNumParams; ++i)
        other->values[(size_t) i] = stored[(size_t) i].load(std::memory_order_relaxed);
    {
        const juce::ScopedLock sl(storedLock);
        other->name = storedName;
        other->category = storedCategory;
    }
    p.splitPart = std::move(other);
    p.editPart = getEditPart();
}

juce::String ZSynthProcessor::getPartName(int part) const
{
    if (part == getEditPart())
        return presets->getCurrentName();
    const juce::ScopedLock sl(storedLock);
    return storedName;
}

void ZSynthProcessor::setEditPart(int part)
{
    part = juce::jlimit(0, 1, part);
    if (part == getEditPart())
        return;
    // The audio thread keeps last block's parameters while the sequence number is odd.
    swapSequence.fetch_add(1, std::memory_order_acq_rel);
    PresetData leaving = capturePreset(apvts);
    leaving.name = presets->getCurrentName();
    leaving.category = presets->getCurrentCategory();
    PresetData arriving;
    {
        const juce::ScopedLock sl(storedLock);
        arriving.name = storedName;
        arriving.category = storedCategory;
    }
    for (int i = 0; i < kNumParams; ++i)
        arriving.values[(size_t) i] = stored[(size_t) i].load(std::memory_order_relaxed);
    storeSound(leaving);
    applyPreset(arriving, apvts, ApplyScope::SoundOnly);
    setParamValue(P::MasterVolume, arriving.values[(size_t) P::MasterVolume]); // each part's own MASTER
    editPart.store(part, std::memory_order_release);
    swapSequence.fetch_add(1, std::memory_order_acq_rel);
    presets->setCurrentNameFromState(arriving.name, arriving.category);
}

void ZSynthProcessor::timerCallback()
{
    applyLearnedSplit();
    applyDetectedKey();
    // SONG playing: the panel shows the pattern being played.
    if (getValue(P::PseqSongOn) >= 0.5f && isSeqRunning())
        if (const int playing = pseq.getSongPattern(); playing >= 0 && playing != currentPattern.load())
            selectPattern(playing);
    // SONG reached its END: release RUN (the sequencer has stopped already).
    if (pseq.isSongFinished() && getValue(P::PseqRun) >= 0.5f)
        setParamValue(P::PseqRun, 0.0f);
    midiLearn.dispatchPending(apvts);
}

bool ZSynthProcessor::applyLearnedSplit()
{
    // LEARN: apply the key the audio thread caught (switching SINGLE to SPLIT).
    const int learned = learnedNote.exchange(-1);
    if (learned >= 0)
    {
        auto set = [this](const char* id, float v) {
            if (auto* prm = apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        set("split.point", (float) juce::jlimit(12, 120, learned));
        if (juce::roundToInt(getValue(P::SplitMode)) == (int) SplitMode::Single)
            set("split.mode", (float) SplitMode::Split);
    }
    return learned >= 0;
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

    // Pattern sequencer: the eight pattern memories (the one on the panel is in the parameters).
    {
        auto* seq = root.createNewChildElement("PatternSeq");
        seq->setAttribute("current", currentPattern.load());
        seq->setAttribute("notes", "absolute"); // step pitches are notes, not offsets from ROOT
        seq->setAttribute("song", songName);
        for (int pi = 0; pi < kPatterns; ++pi)
        {
            if (pi == currentPattern)
                capturePattern(pi);
            juce::String values;
            for (const auto& v : patterns[(size_t) pi])
                values << juce::String(v.load(), 4) << " ";
            seq->createNewChildElement("Pattern")->setAttribute("values", values.trimEnd());
        }
    }

    // Keyboard split / layer: the other part's sound, and which part is on the panel.
    {
        PresetData other;
        for (int i = 0; i < kNumParams; ++i)
            other.values[(size_t) i] = stored[(size_t) i].load();
        {
            const juce::ScopedLock sl(storedLock);
            other.name = storedName;
            other.category = storedCategory;
        }
        auto* split = root.createNewChildElement("Split");
        split->setAttribute("editPart", getEditPart());
        split->addChildElement(presetToXml(other).release());
    }

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
    if (auto* seq = xml->getChildByName("PatternSeq"))
    {
        const bool absoluteNotes = seq->getStringAttribute("notes") == "absolute";
        songName = seq->getStringAttribute("song");
        int pi = 0;
        for (auto* pat : seq->getChildWithTagNameIterator("Pattern"))
        {
            if (pi >= kPatterns)
                break;
            const auto tokens = juce::StringArray::fromTokens(pat->getStringAttribute("values"), " ", "");
            auto& dst = patterns[(size_t) pi++];
            for (int k = 0; k < (int) dst.size() && k < tokens.size(); ++k)
                dst[(size_t) k].store(tokens[k].getFloatValue());
            if (! absoluteNotes)
                for (int r = 0; r < 2; ++r)
                {
                    // Older projects: a step's pitch was semitones from the row's ROOT.
                    const int off = r * kPseqRowStride - P::PseqAOn;
                    const float root = dst[(size_t) (P::PseqARoot + off)].load();
                    for (int st = 0; st < kPseqSteps; ++st)
                    {
                        auto& v = dst[(size_t) (P::PseqAPitch1 + off + st * kPseqStepStride)];
                        float n = root + v.load();
                        while (n > 108.0f)
                            n -= 12.0f;
                        while (n < 21.0f)
                            n += 12.0f;
                        v.store(n);
                    }
                }
        }
        currentPattern = juce::jlimit(0, kPatterns - 1, seq->getIntAttribute("current", 0));
        // The panel shows the current pattern (also brings older projects' notes up to date).
        applyingPattern.store(true);
        applyPattern(currentPattern);
        applyingPattern.store(false);
    }
    if (startupBlanksSequencer.value_or(wrapperType == wrapperType_Standalone))
    {
        // The app starting up: an empty sequencer, unless the preset it opens on
        // was saved with a song (a host's project, by contrast, keeps its song).
        setParamValue(P::PseqRun, 0.0f);
        applySong(blankSong(), false);
        songName.clear();
        PresetData current;
        presetHasSong = false;
        if (presets->getPresetData(presets->getCurrentIndex(), current) && current.song != nullptr)
        {
            applySong(*current.song);
            presetHasSong = true;
        }
    }
    if (auto* split = xml->getChildByName("Split"))
    {
        if (auto* other = split->getChildByName("ZSynthPreset"))
        {
            PresetData p;
            if (presetFromXml(*other, p).ok)
            {
                swapSequence.fetch_add(1, std::memory_order_acq_rel);
                storeSound(p);
                editPart.store(juce::jlimit(0, 1, split->getIntAttribute("editPart", kUpper)), std::memory_order_release);
                swapSequence.fetch_add(1, std::memory_order_acq_rel);
            }
        }
    }
}

} // namespace zsynth

// Factory function used by every JUCE plug-in wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new zsynth::ZSynthProcessor();
}
