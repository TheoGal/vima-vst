#pragma once

// VIMA data model: steps, tracks, patterns, global settings, scales.
// Plain data, no JUCE GUI dependency, so the engine and tests can use it directly.
// See SPEC.md sections 4-7 and 11.

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <cstdint>

namespace vima
{
constexpr int numTracks = 8;
constexpr int maxSteps = 32;
constexpr int minLength = 4;
constexpr int numBanks = 4;
constexpr int patternsPerBank = 16;
constexpr int numPatterns = numBanks * patternsPerBank;   // 64
constexpr int maxChainEntries = 64;

// Internal octave numbering: MIDI note = 12 * (octave + 2) + pitchClass, so C3 = 60
// (the "middle C is C3" convention). The C4 convention only changes the displayed number.
constexpr int minOctave = -2;
constexpr int maxOctave = 8;     // octave 8 reaches G (MIDI 127); higher notes are not played
constexpr int lowestPitch = 0, highestPitch = 127;
constexpr int maxGate = 400;   // gate: 1-400 % of one step

inline int midiNote (int octave, int pitchClass) { return 12 * (octave + 2) + pitchClass; }

// ---------------------------------------------------------------------------------------
// Defaults = the init values (SPEC.md 5): every step on, F (the middle of the note knob)
// in the octave of middle C, velocity 100, gate 100 %, probability 100 %, CC 64.
struct Step
{
    bool on = true;
    std::int8_t pitchClass = 5;   // 0-11 (C to B)
    std::int8_t octave = 3;       // minOctave..maxOctave; 3 = the octave starting at middle C (MIDI 60)
    std::uint8_t velocity = 100;  // 1-127 (never 0: velocity 0 is a note-off in MIDI)
    std::uint16_t gate = 100;     // 1-400 % of one step
    std::uint8_t probability = 100; // 0-100 %
    std::uint8_t cc = 64;         // 0-127

    bool operator== (const Step& o) const
    {
        return on == o.on && pitchClass == o.pitchClass && octave == o.octave && velocity == o.velocity
            && gate == o.gate && probability == o.probability && cc == o.cc;
    }
    bool operator!= (const Step& o) const { return ! (*this == o); }
};

// ---------------------------------------------------------------------------------------
enum class Direction : std::uint8_t { forward, backward, pingPong };

// Divisions: straight 1/1 .. 1/32, then triplets 1/2T .. 1/32T
struct DivisionInfo
{
    const char* name;
    double stepPpq;   // length of one step in quarter notes
};

constexpr int numDivisions = 11;
const DivisionInfo& divisionInfo (int index);
int defaultDivision();   // 1/16

// Per track, per pattern: musical settings (stored in each pattern)
struct TrackPattern
{
    std::uint8_t length = 16;          // 4-32
    std::uint8_t division = 4;         // index into divisions (4 = 1/16)
    Direction direction = Direction::forward;
    std::uint8_t swing = 0;            // 0-100 %
    bool scaleOn = true;
    std::array<Step, maxSteps> steps {};

    bool hasOnSteps() const;
    int onStepCount() const;
};

struct Pattern
{
    std::array<TrackPattern, numTracks> tracks {};
    bool isEmpty() const;
    double lengthPpq() const;   // one play of the pattern = its longest track (length x division)
};

// Per track, global (not per pattern): routing settings
struct TrackRouting
{
    std::uint8_t midiChannel = 1;   // 1-16 (init: the track number)
    bool noteOut = false;           // init: on for track 1 only
    bool ccOut = false;
    std::uint8_t ccNumber = 102;    // 0-127 (init: 102 + track index, undefined CCs 102-109)
};

// ---------------------------------------------------------------------------------------
// Scales: 14 built-in + USER. Each is a 12-bit mask of pitch classes relative to the root.
constexpr int numScales = 15;
constexpr int userScaleIndex = 14;
struct ScaleInfo
{
    const char* shortName;   // shown in the SCALE tile, e.g. "MIN"
    const char* longName;    // shown in the popover, e.g. "MINOR"
    std::uint16_t mask;      // bit n = semitone n above root is in the scale
};
const ScaleInfo& scaleInfo (int index);

enum class Quantize : std::uint8_t { off, beat, bar, patternEnd };

struct ChainEntry
{
    std::uint8_t pattern = 0;   // 0..191
    std::uint8_t repeats = 1;   // 1..99
};

// Fixed-size chain list: copying it never allocates (it is read while the audio lock is held)
class Chain
{
public:
    int size() const { return count; }
    bool isEmpty() const { return count == 0; }
    void clear() { count = 0; }
    void add (ChainEntry e) { if (count < maxChainEntries) entries[(size_t) count++] = e; }
    void insert (int i, ChainEntry e)
    {
        if (count >= maxChainEntries) return;
        i = juce::jlimit (0, count, i);
        for (int k = count; k > i; --k) entries[(size_t) k] = entries[(size_t) k - 1];
        entries[(size_t) i] = e;
        ++count;
    }
    // Move entry `from` so that it ends up at index `to` (both in the current list)
    void move (int from, int to)
    {
        if (from < 0 || from >= count || to < 0 || to >= count || from == to) return;
        const auto e = entries[(size_t) from];
        remove (from);
        insert (to, e);
    }
    void remove (int i)
    {
        if (i < 0 || i >= count) return;
        for (int k = i; k < count - 1; ++k) entries[(size_t) k] = entries[(size_t) k + 1];
        --count;
    }
    ChainEntry operator[] (int i) const { return entries[(size_t) juce::jlimit (0, maxChainEntries - 1, i)]; }
    ChainEntry& getReference (int i) { return entries[(size_t) juce::jlimit (0, maxChainEntries - 1, i)]; }
    const ChainEntry& getReference (int i) const { return entries[(size_t) juce::jlimit (0, maxChainEntries - 1, i)]; }
    const ChainEntry* begin() const { return entries.data(); }
    const ChainEntry* end() const { return entries.data() + count; }

private:
    std::array<ChainEntry, maxChainEntries> entries {};
    int count = 0;
};

// Randomize lane ranges (global, shared by all tracks)
struct LaneRange { int min, max; };

struct Globals
{
    int scale = 1;                 // MINOR
    int root = 0;                  // C
    std::uint16_t userScale = 0b010110101101;   // natural minor shape by default, root always on
    int middleC = 3;               // octave number shown for middle C: 3, 4 or 5 (display only)
    int octaveOffset() const { return middleC - 3; }
    bool lightTheme = false;

    bool ccSmoothing = false;
    int ccSmoothingTime = 100;     // 1-100 % of the time between two active steps

    bool patternCC = false;
    int patternCCChannel = 16;
    int patternCCNumber = 20;
    Quantize quantize = Quantize::bar;

    bool chainOn = false;
    bool chainLoop = true;
    Chain chain;

    // Randomize / smooth lane selection and ranges
    bool laneOn = false, laneNote = true, laneOct = false, laneVel = false, laneGate = false, laneProb = false, laneCC = false;
    LaneRange rangeNote { 0, 11 }, rangeOct { minOctave, maxOctave }, rangeVel { 1, 127 }, rangeGate { 1, maxGate },
              rangeProb { 0, 100 }, rangeCC { 0, 127 };

    std::uint16_t scaleMask() const;   // current scale mask (relative to root)
};

// ---------------------------------------------------------------------------------------
// Scale helpers (all pitch classes absolute, 0 = C)
bool inScale (std::uint16_t mask, int root, int pitchClass);
int quantizeToScale (std::uint16_t mask, int root, int midiNoteNumber);   // nearest in-scale note, ties go down
int scaleSize (std::uint16_t mask);
// Degree index (0-based, counted from the root) of an absolute pitch: floor-based, continuous across octaves
int pitchToDegree (std::uint16_t mask, int root, int midiNoteNumber);
int degreeToPitch (std::uint16_t mask, int root, int degree);

const char* pitchClassName (int pc);
juce::String noteDisplayName (int pitchClass, int octave, int octaveOffset);

// ---------------------------------------------------------------------------------------
// Macros (SPEC.md 13.1): 128 host-automatable knobs, each mapped to one target
constexpr int numMacros = 128;
constexpr int macrosPerBank = 32;

enum class MacroTarget : std::uint8_t
{
    none,
    pattern, chainOn,                                                      // global
    noteOut, ccOut, length, division, direction, swing, scaleOn,           // track
    stepOn, note, octave, velocity, gate, probability, cc                  // step
};
constexpr int numMacroTargets = 17;

struct MacroMap
{
    MacroTarget target = MacroTarget::none;
    std::int8_t track = 0;       // 0..7
    std::int8_t pattern = 0;     // 0..63 (targets stored per pattern)
    std::uint8_t step = 0;       // 0..31 (step targets)
    std::int16_t min = 0, max = 1;

    bool mapped() const { return target != MacroTarget::none; }
    bool operator== (const MacroMap& o) const
    {
        return target == o.target && track == o.track && pattern == o.pattern && step == o.step && min == o.min && max == o.max;
    }
    bool operator!= (const MacroMap& o) const { return ! (*this == o); }
};

// ---------------------------------------------------------------------------------------
// The whole persistent state
struct Project
{
    std::array<MacroMap, numMacros> macros {};
    std::array<Pattern, numPatterns> patterns {};
    std::array<TrackRouting, numTracks> routing {};
    Globals globals;
    int currentPattern = 0;   // pattern shown and played (0..63)

    Project();
    void resetToDefaults();

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    // Presets (SPEC.md 13.3): the music only. Macro mappings and the theme are not part of a
    // preset and are kept when one is loaded.
    juce::ValueTree musicToValueTree() const;
    void loadMusic (const juce::ValueTree&);
    void resetMusic();

    // Macro mapping sets (SPEC.md 13.3): all 128 mappings
    juce::ValueTree macrosToValueTree() const;
    bool loadMacros (const juce::ValueTree&);   // false if the tree is not a mapping set
};

juce::String patternLabel (int patternIndex);   // "B01 · 01"
} // namespace vima
