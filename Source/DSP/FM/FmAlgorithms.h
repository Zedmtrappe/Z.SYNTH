#pragma once

// FM operator routing.
//
// REPRESENTATION. Operators are indexed 0..5 (shown to the user as OP1..OP6).
// For every operator, `modulators[op]` is a bitmask of the operators whose
// output phase-modulates it, and `carriers` is the bitmask of operators summed
// to the audio output. Operator 6 (index 5) always has the self-feedback path.
//
// INVARIANT: a modulator always has a higher index than the operator it
// modulates. Rendering therefore runs from OP6 down to OP1 and every
// modulator's current-sample output is ready when it is needed — no implicit
// one-sample delays except the intentional feedback path. Tests verify this.

#include <cstdint>

namespace zsynth::dsp
{

struct FmAlgorithm
{
    uint8_t modulators[6];
    uint8_t carriers;
};

constexpr uint8_t opBit(int opNumber) { return (uint8_t) (1u << (opNumber - 1)); }

// Indexed by the fm.algo choice (ZS_CH_FM_ALGO).
inline const FmAlgorithm& fmAlgorithm(int index)
{
    static const FmAlgorithm algorithms[] = {
        // 1 Stack: 6>5>4>3>2>1
        { { opBit(2), opBit(3), opBit(4), opBit(5), opBit(6), 0 }, opBit(1) },
        // 2 Twin Stacks: 3>2>1, 6>5>4
        { { opBit(2), opBit(3), 0, opBit(5), opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(4)) },
        // 3 Three Pairs: 2>1, 4>3, 6>5
        { { opBit(2), 0, opBit(4), 0, opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(3) | opBit(5)) },
        // 4 Pair+Stack: 2>1, 6>5>4>3
        { { opBit(2), 0, opBit(4), opBit(5), opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(3)) },
        // 5 Y Branch: (2,3)>1, (5,6)>4
        { { (uint8_t) (opBit(2) | opBit(3)), 0, 0, (uint8_t) (opBit(5) | opBit(6)), 0, 0 }, (uint8_t) (opBit(1) | opBit(4)) },
        // 6 Fan Out: 6>(1,2,3,4,5)
        { { opBit(6), opBit(6), opBit(6), opBit(6), opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(2) | opBit(3) | opBit(4) | opBit(5)) },
        // 7 Triple Mod: (2,3,4)>1, 6>5
        { { (uint8_t) (opBit(2) | opBit(3) | opBit(4)), 0, 0, 0, opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(5)) },
        // 8 Deep+Pair: 3>2>1, 5>4, 6 alone (feedback carrier)
        { { opBit(2), opBit(3), 0, opBit(5), 0, 0 }, (uint8_t) (opBit(1) | opBit(4) | opBit(6)) },
        // 9 Stack+Carriers: 6>5>4>3, 2 and 1 pure carriers
        { { 0, 0, opBit(4), opBit(5), opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(2) | opBit(3)) },
        // 10 Dual Mod: 3>2>1, (5,6)>4
        { { opBit(2), opBit(3), 0, (uint8_t) (opBit(5) | opBit(6)), 0, 0 }, (uint8_t) (opBit(1) | opBit(4)) },
        // 11 Organ: six carriers
        { { 0, 0, 0, 0, 0, 0 }, 0x3F },
        // 12 Cascade Fan: 6>5>4, 4>(1,2,3)
        { { opBit(4), opBit(4), opBit(4), opBit(5), opBit(6), 0 }, (uint8_t) (opBit(1) | opBit(2) | opBit(3)) },
    };
    constexpr int count = (int) (sizeof(algorithms) / sizeof(algorithms[0]));
    return algorithms[index < 0 ? 0 : (index >= count ? count - 1 : index)];
}

constexpr int kFmAlgorithmCount = 12;

} // namespace zsynth::dsp
