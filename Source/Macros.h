#pragma once

// Macro targets (SPEC.md 13.1): what each target is, how to read and write it, and how to
// show it. Used by the host parameters (MacroParams) and the Macros page.

#include "Model.h"
#include <atomic>
#include <optional>

namespace vima
{
enum class MacroKind { none, global, track, step };

struct MacroTargetInfo
{
    MacroTarget target;
    MacroKind kind;
    const char* name;      // "VELOCITY"
    const char* hostName;  // "Velocity"
    bool perPattern;       // stored in each pattern (track settings and steps)
    int min, max;          // full range (octave: see macroLimits)
};

const MacroTargetInfo& macroTargetInfo (MacroTarget);
MacroKind macroKind (MacroTarget);

// Full value range of a target (octave follows the octave range in Settings)
void macroLimits (MacroTarget, const Globals&, int& lo, int& hi);

// A new mapping covering the full range
MacroMap defaultMacroMap (MacroTarget, int track, int step, const Globals&);

int macroTrack (const MacroMap&, int shownTrack);
int macroPattern (const Project&, const MacroMap&);

// Current value of the target (plain units), nothing for an unmapped macro
std::optional<int> macroTargetValue (const Project&, const MacroMap&, int shownTrack);

// Write a plain value to the target. Pattern targets queue the pattern (quantize applies).
// Returns true if the project changed.
bool applyMacroValue (Project&, std::atomic<int>& queuedPattern, const MacroMap&, int shownTrack, int plain);

// Macro value (0..1) <-> target value, through the mapping's MIN / MAX
int macroPlain (const MacroMap&, float normalized);
float macroNormalized (const MacroMap&, int plain);

juce::String macroValueText (const Project&, const MacroMap&, int shownTrack, int plain);
// Same without the project (no scale applied to notes): for host display
juce::String macroValueText (const Globals&, const MacroMap&, int plain);
juce::String macroTargetName (const MacroMap&, bool compact = false);   // "STEP 05 VELOCITY" ("STEP 05 VEL"), "LENGTH", "-"
juce::String macroWhereText (const MacroMap&, bool compact = false);    // "SHOWN TRK · CURRENT" ("SHOWN · CUR"), "T03 · B01 · 05", "GLOBAL"
juce::String macroId (int index);                  // "M001"
} // namespace vima
