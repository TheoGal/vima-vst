#pragma once

// Step operations (message thread): step edits, randomize, smooth, shift, copy/paste, undo.
// Every operation acts on a Scope: the selected steps, or all steps of the track (SPEC.md 8, 9).

#include "Model.h"
#include <deque>
#include <vector>

namespace vima
{
enum class Lane { on, note, octave, velocity, gate, probability, cc };

struct Scope
{
    int pattern = 0;
    int track = 0;
    int first = 0;   // inclusive
    int last = 15;   // inclusive
};

// Scope for a selection (first/last < 0 means "no selection": all steps within the track length)
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
    std::vector<Step> snapshot;   // steps first..last when the knob was grabbed
};
ShiftGesture beginShift (const Project&, const Scope&, Lane);
void applyShift (Project&, const ShiftGesture&, int amount);   // computed from the snapshot, values capped

std::vector<Step> copySteps (const Project&, const Scope&);
// Paste starting at `start`; anything past the track length is cut off. Returns the last step written (or -1).
int pasteSteps (Project&, int pattern, int track, int start, const std::vector<Step>&);

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
