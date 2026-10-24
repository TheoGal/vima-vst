#include "Macros.h"
#include "Operations.h"

namespace vima
{
static const MacroTargetInfo infos[numMacroTargets] = {
    { MacroTarget::none,        MacroKind::none,   "-",          "-",           false, 0, 1 },
    { MacroTarget::pattern,     MacroKind::global, "PATTERN",    "Pattern",     false, 0, numPatterns - 1 },
    { MacroTarget::chainOn,     MacroKind::global, "CHAIN ON",   "Chain On",    false, 0, 1 },
    { MacroTarget::noteOut,     MacroKind::track,  "NOTES OUT",  "Notes Out",   false, 0, 1 },
    { MacroTarget::ccOut,       MacroKind::track,  "CC OUT",     "CC Out",      false, 0, 1 },
    { MacroTarget::length,      MacroKind::track,  "LENGTH",     "Length",      true,  minLength, maxSteps },
    { MacroTarget::division,    MacroKind::track,  "DIVISION",   "Division",    true,  0, numDivisions - 1 },
    { MacroTarget::direction,   MacroKind::track,  "DIRECTION",  "Direction",   true,  0, 2 },
    { MacroTarget::swing,       MacroKind::track,  "SWING",      "Swing",       true,  0, 100 },
    { MacroTarget::scaleOn,     MacroKind::track,  "SCALE ON",   "Scale On",    true,  0, 1 },
    { MacroTarget::stepOn,      MacroKind::step,   "NOTE STEP",  "Note Step",   true,  0, 1 },
    { MacroTarget::note,        MacroKind::step,   "NOTE",       "Note",        true,  0, 11 },
    { MacroTarget::octave,      MacroKind::step,   "OCTAVE",     "Octave",      true,  minOctave, maxOctave },
    { MacroTarget::velocity,    MacroKind::step,   "VELOCITY",   "Velocity",    true,  1, 127 },
    { MacroTarget::gate,        MacroKind::step,   "GATE",       "Gate",        true,  1, maxGate },
    { MacroTarget::probability, MacroKind::step,   "PROB",       "Probability", true,  1, 100 },
    { MacroTarget::cc,          MacroKind::step,   "CC VALUE",   "CC Value",    true,  0, 127 },
    { MacroTarget::ccStepOn,    MacroKind::step,   "CC STEP",    "CC Step",     true,  0, 1 },
};

const MacroTargetInfo& macroTargetInfo (MacroTarget t)
{
    return infos[juce::jlimit (0, numMacroTargets - 1, (int) t)];
}

MacroKind macroKind (MacroTarget t) { return macroTargetInfo (t).kind; }

void macroLimits (MacroTarget t, int& lo, int& hi)
{
    const auto& i = macroTargetInfo (t);
    lo = i.min;
    hi = i.max;
}

MacroMap defaultMacroMap (MacroTarget t, int track, int pattern, int step, const Globals&)
{
    MacroMap m;
    m.target = t;
    m.track = (std::int8_t) juce::jlimit (0, numTracks - 1, track);
    m.pattern = (std::int8_t) juce::jlimit (0, numPatterns - 1, pattern);
    m.step = (std::uint8_t) juce::jlimit (0, maxSteps - 1, step);
    return m;
}

bool sameControl (const MacroMap& a, const MacroMap& b)
{
    if (! a.mapped() || a.target != b.target)
        return false;
    const auto& i = macroTargetInfo (a.target);
    if (i.kind == MacroKind::global)
        return true;
    if (a.track != b.track)
        return false;
    if (i.perPattern && a.pattern != b.pattern)
        return false;
    return i.kind != MacroKind::step || a.step == b.step;
}

static int macroTrack (const MacroMap& m) { return juce::jlimit (0, numTracks - 1, (int) m.track); }
static int macroPattern (const MacroMap& m) { return juce::jlimit (0, numPatterns - 1, (int) m.pattern); }

std::optional<int> macroTargetValue (const Project& p, const MacroMap& m)
{
    if (! m.mapped())
        return std::nullopt;
    const int t = macroTrack (m);
    const auto& tp = p.patterns[(size_t) macroPattern (m)].tracks[(size_t) t];
    const auto& s = tp.steps[(size_t) juce::jlimit (0, maxSteps - 1, (int) m.step)];
    const auto& r = p.routing[(size_t) t];
    switch (m.target)
    {
        case MacroTarget::none:        break;
        case MacroTarget::pattern:     return p.currentPattern;
        case MacroTarget::chainOn:     return p.globals.chainOn ? 1 : 0;
        case MacroTarget::noteOut:     return r.noteOut ? 1 : 0;
        case MacroTarget::ccOut:       return r.ccOut ? 1 : 0;
        case MacroTarget::length:      return (int) tp.length;
        case MacroTarget::division:    return (int) tp.division;
        case MacroTarget::direction:   return (int) tp.direction;
        case MacroTarget::swing:       return (int) tp.swing;
        case MacroTarget::scaleOn:     return tp.scaleOn ? 1 : 0;
        case MacroTarget::stepOn:      return s.on ? 1 : 0;
        case MacroTarget::ccStepOn:    return s.ccOn ? 1 : 0;
        case MacroTarget::note:        // the note that sounds, as the step's note knob shows it
            return tp.scaleOn ? (((quantizeToScale (p.globals.scaleMask(), p.globals.root, 60 + s.pitchClass) % 12) + 12) % 12)
                              : (int) s.pitchClass;
        case MacroTarget::octave:      return (int) s.octave;
        case MacroTarget::velocity:    return (int) s.velocity;
        case MacroTarget::gate:        return (int) s.gate;
        case MacroTarget::probability: return (int) s.probability;
        case MacroTarget::cc:          return (int) s.cc;
    }
    return std::nullopt;
}

bool applyMacroValue (Project& p, std::atomic<int>& queuedPattern, const MacroMap& m, int v)
{
    const auto current = macroTargetValue (p, m);
    if (! current.has_value())
        return false;
    int lo = 0, hi = 1;
    macroLimits (m.target, lo, hi);
    v = juce::jlimit (lo, hi, v);
    if (v == *current)
        return false;

    const int t = macroTrack (m);
    auto& tp = p.patterns[(size_t) macroPattern (m)].tracks[(size_t) t];
    auto& s = tp.steps[(size_t) juce::jlimit (0, maxSteps - 1, (int) m.step)];
    auto& r = p.routing[(size_t) t];
    switch (m.target)
    {
        case MacroTarget::none:        return false;
        case MacroTarget::pattern:     queuedPattern = v; return false;   // switches when quantize allows
        case MacroTarget::chainOn:     p.globals.chainOn = v != 0; break;
        case MacroTarget::noteOut:     r.noteOut = v != 0; break;
        case MacroTarget::ccOut:       r.ccOut = v != 0; break;
        case MacroTarget::length:      tp.length = (std::uint8_t) v; break;
        case MacroTarget::division:    tp.division = (std::uint8_t) v; break;
        case MacroTarget::direction:   tp.direction = (Direction) v; break;
        case MacroTarget::swing:       tp.swing = (std::uint8_t) v; break;
        case MacroTarget::scaleOn:     tp.scaleOn = v != 0; break;
        case MacroTarget::stepOn:      s.on = v != 0; break;
        case MacroTarget::ccStepOn:    s.ccOn = v != 0; break;
        case MacroTarget::note:        s.pitchClass = (std::int8_t) v; break;
        case MacroTarget::octave:      s.octave = (std::int8_t) v; break;
        case MacroTarget::velocity:    s.velocity = (std::uint8_t) v; break;
        case MacroTarget::gate:        s.gate = (std::uint16_t) v; break;
        case MacroTarget::probability: s.probability = (std::uint8_t) juce::jlimit (1, 100, v); break;
        case MacroTarget::cc:          s.cc = (std::uint8_t) v; break;
    }
    return true;
}

bool MacroNotes::allows (int pc) const
{
    return ! scaleOn || inScale (mask, root, ((pc % 12) + 12) % 12);
}

MacroNotes macroNotes (const Project& p, const MacroMap& m)
{
    MacroNotes n;
    if (m.target != MacroTarget::note)
        return n;
    n.scaleOn = p.patterns[(size_t) macroPattern (m)].tracks[(size_t) macroTrack (m)].scaleOn;
    n.mask = p.globals.scaleMask();
    n.root = p.globals.root;
    return n;
}

namespace
{
    // The allowed notes from C to B: all 12, or the in-scale notes
    int noteList (const MacroNotes& n, std::array<int, 12>& out)
    {
        int count = 0;
        for (int pc = 0; pc < 12; ++pc)
            if (n.allows (pc)) out[(size_t) count++] = pc;
        if (count == 0)
            out[(size_t) count++] = n.root;
        return count;
    }

    void limits (const MacroMap& m, int& lo, int& hi) { macroLimits (m.target, lo, hi); }
}

int macroPositions (const MacroMap& m, const MacroNotes& n)
{
    if (m.target == MacroTarget::note)
    {
        std::array<int, 12> list {};
        return noteList (n, list);
    }
    int lo = 0, hi = 1;
    limits (m, lo, hi);
    return hi - lo + 1;
}

int macroPlain (const MacroMap& m, float normalized, const MacroNotes& n)
{
    const float f = juce::jlimit (0.0f, 1.0f, normalized);
    if (m.target == MacroTarget::note)
    {
        std::array<int, 12> list {};
        const int count = noteList (n, list);
        return list[(size_t) juce::jlimit (0, count - 1, juce::roundToInt (f * (float) (count - 1)))];
    }
    int lo = 0, hi = 1;
    limits (m, lo, hi);
    return juce::roundToInt ((float) lo + (float) (hi - lo) * f);
}

float macroNormalized (const MacroMap& m, int plain, const MacroNotes& n)
{
    if (m.target == MacroTarget::note)
    {
        std::array<int, 12> list {};
        const int count = noteList (n, list);
        if (count < 2)
            return 0.0f;
        int best = 0;
        for (int k = 1; k < count; ++k)   // the position of this note, or the nearest one
            if (std::abs (list[(size_t) k] - plain) < std::abs (list[(size_t) best] - plain)) best = k;
        return (float) best / (float) (count - 1);
    }
    int lo = 0, hi = 1;
    limits (m, lo, hi);
    if (hi == lo)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (float) (plain - lo) / (float) (hi - lo));
}

static juce::String plainText (const MacroMap& m, int v, int octaveOffset)
{
    switch (m.target)
    {
        case MacroTarget::none:        return {};
        case MacroTarget::pattern:     return patternLabel (juce::jlimit (0, numPatterns - 1, v));
        case MacroTarget::chainOn:
        case MacroTarget::noteOut:
        case MacroTarget::ccOut:
        case MacroTarget::scaleOn:
        case MacroTarget::stepOn:
        case MacroTarget::ccStepOn:    return v != 0 ? "ON" : "OFF";
        case MacroTarget::division:    return divisionInfo (v).name;
        case MacroTarget::direction:   return v == 0 ? "FWD" : v == 1 ? "BWD" : "PING";
        case MacroTarget::swing:
        case MacroTarget::gate:
        case MacroTarget::probability: return juce::String (v) + "%";
        case MacroTarget::octave:      return juce::String (v + octaveOffset);
        case MacroTarget::note:        return pitchClassName (((v % 12) + 12) % 12);
        case MacroTarget::length:
        case MacroTarget::velocity:
        case MacroTarget::cc:          return juce::String (v);
    }
    return juce::String (v);
}

juce::String macroValueText (const Globals& g, const MacroMap& m, int v)
{
    return plainText (m, v, g.octaveOffset());
}

juce::String macroValueText (const Project& p, const MacroMap& m, int v)
{
    // note values are already the sounding note
    return plainText (m, v, p.globals.octaveOffset());
}

static const char* shortName (MacroTarget t)
{
    switch (t)
    {
        case MacroTarget::stepOn:      return "NOTE STEP";
        case MacroTarget::ccStepOn:    return "CC STEP";
        case MacroTarget::note:        return "NOTE";
        case MacroTarget::octave:      return "OCT";
        case MacroTarget::velocity:    return "VEL";
        case MacroTarget::gate:        return "GATE";
        case MacroTarget::probability: return "PROB";
        case MacroTarget::cc:          return "CC";
        case MacroTarget::none:
        case MacroTarget::pattern:
        case MacroTarget::chainOn:
        case MacroTarget::noteOut:
        case MacroTarget::ccOut:
        case MacroTarget::length:
        case MacroTarget::division:
        case MacroTarget::direction:
        case MacroTarget::swing:
        case MacroTarget::scaleOn:     break;
    }
    return macroTargetInfo (t).name;
}

juce::String macroTargetName (const MacroMap& m, bool compact)
{
    if (! m.mapped())
        return "-";
    const auto& i = macroTargetInfo (m.target);
    if (i.kind == MacroKind::step)
        return "STEP " + juce::String ((int) m.step + 1).paddedLeft ('0', 2) + " " + (compact ? shortName (m.target) : i.name);
    return i.name;
}

juce::String macroWhereText (const MacroMap& m, bool compact)
{
    if (! m.mapped())
        return "NOT MAPPED";
    const auto& i = macroTargetInfo (m.target);
    if (i.kind == MacroKind::global)
        return "GLOBAL";
    const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    const juce::String track = "T" + juce::String ((int) m.track + 1).paddedLeft ('0', 2);
    if (! i.perPattern)
        return track;
    auto pattern = patternLabel (m.pattern);
    if (compact)
        pattern = pattern.removeCharacters (" ");
    return track + dot + pattern;
}

juce::String macroHostName (const MacroMap& m, int index)
{
    if (! m.mapped())
        return "Macro " + juce::String (index + 1);
    auto two = [] (int v) { return juce::String (v).paddedLeft ('0', 2); };
    const auto tr = " Tr" + two (m.track + 1);
    const auto pat = " Pat" + two (m.pattern + 1);
    const auto st = " St" + two (m.step + 1);
    switch (m.target)
    {
        case MacroTarget::none:        break;
        case MacroTarget::pattern:     return "Pattern";
        case MacroTarget::chainOn:     return "Chain On";
        case MacroTarget::noteOut:     return "NotesOut" + tr;
        case MacroTarget::ccOut:       return "CCOut" + tr;
        case MacroTarget::length:      return "Length" + tr + pat;
        case MacroTarget::division:    return "Div" + tr + pat;
        case MacroTarget::direction:   return "Dir" + tr + pat;
        case MacroTarget::swing:       return "Swing" + tr + pat;
        case MacroTarget::scaleOn:     return "Scale" + tr + pat;
        case MacroTarget::stepOn:      return "NoteStep" + st + tr + pat;
        case MacroTarget::note:        return "Note" + st + tr + pat;
        case MacroTarget::octave:      return "Oct" + st + tr + pat;
        case MacroTarget::velocity:    return "Vel" + st + tr + pat;
        case MacroTarget::gate:        return "Gate" + st + tr + pat;
        case MacroTarget::probability: return "Prob" + st + tr + pat;
        case MacroTarget::cc:          return "CCval" + st + tr + pat;
        case MacroTarget::ccStepOn:    return "CCStep" + st + tr + pat;
    }
    return "Macro " + juce::String (index + 1);
}

juce::String macroId (int index)
{
    return "M" + juce::String (index + 1).paddedLeft ('0', 3);
}
} // namespace vima
