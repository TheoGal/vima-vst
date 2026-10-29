#pragma once

// Step operations (message thread): step edits, randomize, smooth, shift, copy/paste, undo.
// Every operation acts on a Scope: the selected steps, or all steps of the track (SPEC.md 8, 9).

#include "Model.h"
#include <deque>
#include <vector>

namespace vima
{
enum class Param { on, note, octave, velocity, gate, probability, cc, ccOn };   // on = note on/off, ccOn = CC on/off

// The steps an operation acts on: a set of steps (bit n = step n), always within the track length
struct Scope
{
    int pattern = 0;
    int track = 0;
    std::uint32_t mask = 0;

    bool contains (int i) const { return i >= 0 && i < maxSteps && ((mask >> i) & 1u) != 0; }
    int count() const;
    int first() const;   // lowest step in the scope (-1 if empty)
};

std::uint32_t rangeMask (int first, int last);   // steps first..last (inclusive, any order)

// Scope for a selection set. An empty selection (or one entirely beyond the length) = all steps.
Scope makeScope (const Project&, int pattern, int track, std::uint32_t selection);
// Same, for a contiguous range (selFirst < 0 = no selection)
Scope makeScope (const Project&, int pattern, int track, int selFirst, int selLast);

// Params enabled in the LANES chips for a mode (note mode or CC mode)
std::vector<Param> armedParams (const Globals&, bool ccMode);

// ---- single step edits --------------------------------------------------------------
// Pitch knob: move the step's pitch class by `delta` positions (only in-scale notes when the track's scale is on)
void nudgePitchClass (Project&, int pattern, int track, int step, int delta);
void setPitchClass (Project&, int pattern, int track, int step, int pitchClass);
void nudgeOctave (Project&, int pattern, int track, int step, int delta);

// The note the step actually plays (scale applied when the track's scale is on)
int soundingNote (const Project&, int pattern, int track, int step);

// ---- operations on a scope ----------------------------------------------------------
void randomize (Project&, const Scope&, bool ccMode, juce::Random&);
// Smooth: values one step toward their average; ON: the on count one step toward the on chance
void smooth (Project&, const Scope&, bool ccMode, juce::Random* rnd = nullptr);

// Curves (SPEC.md 9.5): write one cycle of a shape over the scope (first to last step), for every
// armed param, between the param's RANGE min and max
enum class CurveShape { sine, triangle, ramp, square };
void applyCurve (Project&, const Scope&, bool ccMode, CurveShape);
// Rotate the armed params' values by one step among the steps in scope: +1 = the last value goes
// first, -1 = the first value goes last
void rotateSteps (Project&, const Scope&, bool ccMode, int direction);
// Flip the armed params' values around the center of their RANGE (min + max - value)
void invertSteps (Project&, const Scope&, bool ccMode);

struct ShiftGesture
{
    Scope scope;
    Param param = Param::note;
    std::array<Step, maxSteps> snapshot {};   // the track's steps when the knob was grabbed
};
ShiftGesture beginShift (const Project&, const Scope&, Param);
void applyShift (Project&, const ShiftGesture&, int amount);   // computed from the snapshot, values capped

// Copied steps keep their spacing: offset = distance from the first copied step
struct ClipStep
{
    int offset = 0;
    Step step;
};
std::vector<ClipStep> copySteps (const Project&, const Scope&);
// Paste with the first copied step at `start`, keeping the spacing. Only the given params are
// written (empty = whole steps). Steps past the track length are cut off. Returns the steps written.
std::uint32_t pasteSteps (Project&, int pattern, int track, int start, const std::vector<ClipStep>&,
                          const std::vector<Param>& params = {});

// ---- undo ---------------------------------------------------------------------------
class UndoHistory
{
public:
    // remember a pattern before changing it; `label` says what the change is (shown by the
    // UNDO info box, SPEC.md 9), for example "RANDOMIZE · VEL · T03 · P23"
    void push (const Project&, int pattern, const juce::String& label = {});
    bool undo (Project&, int& restoredPattern);
    bool redo (Project&, int& restoredPattern);   // steps undone can be redone until a new edit
    bool canUndo() const { return ! entries.empty(); }
    bool canRedo() const { return ! redoEntries.empty(); }
    void clear() { entries.clear(); redoEntries.clear(); }
    juce::String nextLabel() const { return entries.empty() ? juce::String() : entries.back().label; }
    juce::String nextRedoLabel() const { return redoEntries.empty() ? juce::String() : redoEntries.back().label; }
    void relabelLast (const juce::String& l) { if (! entries.empty()) entries.back().label = l; }

private:
    struct Entry { int pattern; Pattern data; juce::String label; };
    std::deque<Entry> entries, redoEntries;
    static constexpr size_t maxEntries = 64;
};
} // namespace vima
