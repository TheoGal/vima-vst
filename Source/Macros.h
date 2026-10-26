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
    int min, max;          // full range of the target
};

const MacroTargetInfo& macroTargetInfo (MacroTarget);
MacroKind macroKind (MacroTarget);

// Full value range of a target
void macroLimits (MacroTarget, int& lo, int& hi);

// A new mapping covering the full range. Every mapping is fixed (SPEC.md 13.1): one track,
// one pattern (for settings stored per pattern) and one step.
MacroMap defaultMacroMap (MacroTarget, int track, int pattern, int step, const Globals&);

// True when two mappings point at the same control (no two macros may do that)
bool sameControl (const MacroMap&, const MacroMap&);

// Current value of the target (plain units), nothing for an unmapped macro
// For the Pattern target, the value is the queued pattern while a switch is waiting for its
// quantize point (`queuedPattern` >= 0), so a macro shows the pattern picked at once.
std::optional<int> macroTargetValue (const Project&, const MacroMap&, int queuedPattern = -1);

// Write a plain value to the target. Pattern targets queue the pattern (quantize applies).
// Returns true if the project changed.
bool applyMacroValue (Project&, std::atomic<int>& queuedPattern, const MacroMap&, int plain);

// The notes a Note macro may land on: all 12, or only in-scale notes when the target track's
// scale is on (like the step's note knob)
struct MacroNotes
{
    bool scaleOn = false;
    std::uint16_t mask = 0xFFF;   // scale mask relative to root
    int root = 0;
    bool allows (int pitchClass) const;
};
MacroNotes macroNotes (const Project&, const MacroMap&);

// Macro value (0..1) <-> target value, over the target's full range. A Note macro moves
// through the allowed notes (all 12, or the in-scale ones), evenly spread over its travel.
int macroPlain (const MacroMap&, float normalized, const MacroNotes& = {});
float macroNormalized (const MacroMap&, int plain, const MacroNotes& = {});
int macroPositions (const MacroMap&, const MacroNotes& = {});   // distinct values over the travel

juce::String macroValueText (const Project&, const MacroMap&, int plain);
// Same without the project (no scale applied to notes): for host display
juce::String macroValueText (const Globals&, const MacroMap&, int plain);
juce::String macroTargetName (const MacroMap&, bool compact = false);   // "STEP 05 VELOCITY" ("STEP 05 VEL"), "LENGTH", "-"
juce::String macroWhereText (const MacroMap&, bool compact = false);    // "T03 · 05", "T03", "GLOBAL"
juce::String macroId (int index);                  // "M001"
// Name the host shows for a macro: "Vel St05 Tr03 Pat23", "Swing Tr03 Pat23", "NotesOut Tr03",
// "Pattern"; "Macro 1" when unmapped (SPEC.md 13.1)
juce::String macroHostName (const MacroMap&, int index);
} // namespace vima
