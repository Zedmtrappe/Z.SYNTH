#include "MidiLearn.h"

namespace zsynth
{

namespace
{
    // Controllers with fixed performance meanings are never learnable.
    bool isReservedController(int cc)
    {
        return cc == 1 || cc == 11 || cc == 64 || cc >= 120;
    }
}

MidiLearn::MidiLearn()
{
    for (auto& c : ccToParam)
        c.store(-1);
}

void MidiLearn::process(const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (! m.isController())
            continue;
        const int cc = m.getControllerNumber();
        if (isReservedController(cc))
            continue;

        const int target = learnTarget.load(std::memory_order_relaxed);
        if (target >= 0)
        {
            // Assign: a controller drives one parameter and a parameter has one controller.
            for (auto& c : ccToParam)
                if (c.load(std::memory_order_relaxed) == target)
                    c.store(-1, std::memory_order_relaxed);
            ccToParam[cc].store(target, std::memory_order_relaxed);
            learnTarget.store(-1, std::memory_order_relaxed);
            mappingChanged.store(true, std::memory_order_relaxed);
        }

        const int param = ccToParam[cc].load(std::memory_order_relaxed);
        if (param >= 0)
        {
            const auto scope = fifo.write(1);
            if (scope.blockSize1 > 0)
                pending[scope.startIndex1] = { param, (float) m.getControllerValue() / 127.0f };
        }
    }
}

bool MidiLearn::dispatchPending(juce::AudioProcessorValueTreeState& state)
{
    // Coalesce: only the latest value per parameter matters.
    float latest[kNumParams];
    bool touched[kNumParams] {};
    const auto scope = fifo.read(fifo.getNumReady());
    auto take = [&](int start, int size) {
        for (int i = 0; i < size; ++i)
        {
            const auto& p = pending[start + i];
            latest[p.param] = p.normalised;
            touched[p.param] = true;
        }
    };
    take(scope.startIndex1, scope.blockSize1);
    take(scope.startIndex2, scope.blockSize2);

    for (int i = 0; i < kNumParams; ++i)
        if (touched[i])
            if (auto* param = state.getParameter(paramDef(i).id))
                param->setValueNotifyingHost(latest[i]);

    return mappingChanged.exchange(false);
}

void MidiLearn::clearMapping(int paramIndex)
{
    for (auto& c : ccToParam)
        if (c.load() == paramIndex)
            c.store(-1);
}

void MidiLearn::clearAll()
{
    for (auto& c : ccToParam)
        c.store(-1);
}

int MidiLearn::getControllerFor(int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[cc].load() == paramIndex)
            return cc;
    return -1;
}

juce::ValueTree MidiLearn::toValueTree() const
{
    juce::ValueTree tree("MidiLearn");
    for (int cc = 0; cc < 128; ++cc)
    {
        const int p = ccToParam[cc].load();
        if (p >= 0)
        {
            juce::ValueTree m("Map");
            m.setProperty("cc", cc, nullptr);
            m.setProperty("param", paramDef(p).id, nullptr);
            tree.appendChild(m, nullptr);
        }
    }
    return tree;
}

void MidiLearn::fromValueTree(const juce::ValueTree& tree)
{
    if (! tree.hasType("MidiLearn"))
        return;
    clearAll();
    for (const auto& m : tree)
    {
        const int cc = m.getProperty("cc", -1);
        const int p = findParamIndex(m.getProperty("param").toString().toRawUTF8());
        if (cc >= 0 && cc < 128 && p >= 0 && ! isReservedController(cc))
            ccToParam[cc].store(p);
    }
}

} // namespace zsynth
