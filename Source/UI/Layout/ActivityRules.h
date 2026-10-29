#pragma once

// Decides which controls are currently irrelevant. Inactive controls never
// move or disappear: the panel dims them and the tooltip explains why.

#include "../../Parameters/ParamTable.h"

#include <juce_core/juce_core.h>

namespace zsynth::ui
{

struct Activity
{
    bool active = true;
    juce::String reason;
};

/** values: a snapshot of every parameter in real units (indexed by P::). */
Activity activityFor(int paramIndex, const float* values, bool hostTempo);

} // namespace zsynth::ui
