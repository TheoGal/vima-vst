#pragma once

// Step operations (message thread): step edits, randomize, smooth, shift, copy/paste, undo.
// Every operation acts on a Scope: the selected steps, or all steps of the track (SPEC.md 8, 9).

#include "Model.h"
#include <deque>
#include <vector>

namespace vima
{
enum class Lane { on, note, octave, velocity, gate, probability, cc };

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

// Lanes enabled in the LANES chips for a mode (note mode or CC mode)
std::vector<Lane> activeLanes (const Globals&, bool ccMode);

// ---- single step edits --------------------------------------------------------------
// Pitch knob: move the step's pitch class by `delta` positions (only in-scale notes when the track's scale is on)
void nudgePitchClass (Project&, int pattern, int track, int step, int delta);
void setPitchClass (Project&, int pattern, int track, int step, int pitchClass);
void nudgeOctave (Project&, int pattern, int track, int step, int delta);

// The note the step actually plays (scale applied when the track's scale is on)
int soundingNote (const Project&, int pattern, int track, int step);

// ---- operations on a scope ----------------------------------------------------------
void randomize (Project&, const Scope&, bool ccMode, juce::Random&);
void smooth (Project&, const Scope&, bool ccMode);

struct ShiftGesture
{
    Scope scope;
    Lane lane = Lane::note;
    std::array<Step, maxSteps> snapshot {};   // the track's steps when the knob was grabbed
};
ShiftGesture beginShift (const Project&, const Scope&, Lane);
void applyShift (Project&, const ShiftGesture&, int amount);   // computed from the snapshot, values capped

// Copied steps keep their spacing: offset = distance from the first copied step
struct ClipStep
{
    int offset = 0;
    Step step;
};
std::vector<ClipStep> copySteps (const Project&, const Scope&);
// Paste with the first copied step at `start`, keeping the spacing. Steps past the track length
// are cut off. Returns the set of steps written.
std::uint32_t pasteSteps (Project&, int pattern, int track, int start, const std::vector<ClipStep>&);

// ---- undo ---------------------------------------------------------------------------
class UndoHistory
{
public:
    void push (const Project&, int pattern);   // remember a pattern before changing it
    bool undo (Project&, int& restoredPattern);
    bool canUndo() const { return ! entries.empty(); }
    void clear() { entries.clear(); }

private:
    struct Entry { int pattern; Pattern data; };
    std::deque<Entry> entries;
    static constexpr size_t maxEntries = 64;
};
} // namespace vima
