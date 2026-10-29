#include "Operations.h"

namespace vima
{
static int mod12 (int v) { return ((v % 12) + 12) % 12; }
static int floorDiv12 (int v) { return v >= 0 ? v / 12 : -((-v + 11) / 12); }

int Scope::count() const
{
    int n = 0;
    for (int i = 0; i < maxSteps; ++i) n += contains (i) ? 1 : 0;
    return n;
}

int Scope::first() const
{
    for (int i = 0; i < maxSteps; ++i)
        if (contains (i)) return i;
    return -1;
}

std::uint32_t rangeMask (int a, int b)
{
    if (a > b) std::swap (a, b);
    a = juce::jlimit (0, maxSteps - 1, a);
    b = juce::jlimit (0, maxSteps - 1, b);
    std::uint32_t m = 0;
    for (int i = a; i <= b; ++i) m |= (1u << i);
    return m;
}

Scope makeScope (const Project& p, int pattern, int track, std::uint32_t selection)
{
    Scope s;
    s.pattern = juce::jlimit (0, numPatterns - 1, pattern);
    s.track = juce::jlimit (0, numTracks - 1, track);
    const int len = p.patterns[(size_t) s.pattern].tracks[(size_t) s.track].length;
    const auto lenMask = rangeMask (0, len - 1);
    s.mask = selection & lenMask;
    if (s.mask == 0)
        s.mask = lenMask;   // no selection = all steps
    return s;
}

Scope makeScope (const Project& p, int pattern, int track, int selFirst, int selLast)
{
    if (selFirst < 0)
        return makeScope (p, pattern, track, (std::uint32_t) 0);
    return makeScope (p, pattern, track, rangeMask (selFirst, selLast < 0 ? selFirst : selLast));
}

std::vector<Param> armedParams (const Globals& g, bool ccMode)
{
    std::vector<Param> params;
    if (! ccMode && g.armAllNote) return { Param::on, Param::note, Param::octave, Param::velocity, Param::gate, Param::probability };
    if (ccMode)
    {
        if (g.armCcOn) params.push_back (Param::ccOn);
        if (g.armCC) params.push_back (Param::cc);
        return params;
    }
    if (g.armOn) params.push_back (Param::on);
    if (g.armNote) params.push_back (Param::note);
    if (g.armOct) params.push_back (Param::octave);
    if (g.armVel) params.push_back (Param::velocity);
    if (g.armGate) params.push_back (Param::gate);
    if (g.armProb) params.push_back (Param::probability);
    return params;
}

static TrackPattern& trackOf (Project& p, int pattern, int track)
{
    return p.patterns[(size_t) juce::jlimit (0, numPatterns - 1, pattern)].tracks[(size_t) juce::jlimit (0, numTracks - 1, track)];
}

static const TrackPattern& trackOf (const Project& p, int pattern, int track)
{
    return p.patterns[(size_t) juce::jlimit (0, numPatterns - 1, pattern)].tracks[(size_t) juce::jlimit (0, numTracks - 1, track)];
}

int soundingNote (const Project& p, int pattern, int track, int step)
{
    const auto& tp = trackOf (p, pattern, track);
    const auto& s = tp.steps[(size_t) juce::jlimit (0, maxSteps - 1, step)];
    int pitch = midiNote (s.octave, s.pitchClass);
    if (tp.scaleOn)
        pitch = quantizeToScale (p.globals.scaleMask(), p.globals.root, pitch);
    return pitch;
}

static void setPitch (Step& s, int pitch)
{
    const int lo = lowestPitch, hi = highestPitch;
    pitch = juce::jlimit (lo, hi, pitch);
    s.pitchClass = (std::int8_t) mod12 (pitch);
    s.octave = (std::int8_t) (floorDiv12 (pitch) - 2);
}

void nudgePitchClass (Project& p, int pattern, int track, int step, int delta)
{
    auto& tp = trackOf (p, pattern, track);
    auto& s = tp.steps[(size_t) juce::jlimit (0, maxSteps - 1, step)];
    const auto& g = p.globals;
    if (! tp.scaleOn)
    {
        s.pitchClass = (std::int8_t) juce::jlimit (0, 11, s.pitchClass + delta);
        return;
    }
    const auto mask = g.scaleMask();
    std::vector<int> pcs;
    for (int pc = 0; pc < 12; ++pc)
        if (inScale (mask, g.root, pc)) pcs.push_back (pc);
    const int current = mod12 (quantizeToScale (mask, g.root, midiNote (s.octave, s.pitchClass)));
    int idx = 0;
    for (int i = 0; i < (int) pcs.size(); ++i)
        if (pcs[(size_t) i] == current) idx = i;
    idx = juce::jlimit (0, (int) pcs.size() - 1, idx + delta);
    s.pitchClass = (std::int8_t) pcs[(size_t) idx];
}

void setPitchClass (Project& p, int pattern, int track, int step, int pitchClass)
{
    auto& s = trackOf (p, pattern, track).steps[(size_t) juce::jlimit (0, maxSteps - 1, step)];
    s.pitchClass = (std::int8_t) mod12 (pitchClass);
}

void nudgeOctave (Project& p, int pattern, int track, int step, int delta)
{
    auto& s = trackOf (p, pattern, track).steps[(size_t) juce::jlimit (0, maxSteps - 1, step)];
    s.octave = (std::int8_t) juce::jlimit (minOctave, maxOctave, s.octave + delta);
}

// ---------------------------------------------------------------------------------------
static int randomIn (juce::Random& r, int lo, int hi)
{
    if (hi < lo) std::swap (lo, hi);
    return lo + r.nextInt (hi - lo + 1);
}

void randomize (Project& p, const Scope& sc, bool ccMode, juce::Random& r)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    const auto& g = p.globals;
    const auto params = armedParams (g, ccMode);
    const auto mask = g.scaleMask();

    std::vector<int> pcs;
    for (int pc = juce::jmin (g.rangeNote.min, g.rangeNote.max); pc <= juce::jmax (g.rangeNote.min, g.rangeNote.max); ++pc)
        if (! tp.scaleOn || inScale (mask, g.root, pc)) pcs.push_back (pc);
    if (pcs.empty())
        for (int pc = 0; pc < 12; ++pc) pcs.push_back (pc);

    const int octLo = juce::jmax (minOctave, juce::jmin (g.rangeOct.min, g.rangeOct.max));
    const int octHi = juce::jmin (maxOctave, juce::jmax (g.rangeOct.min, g.rangeOct.max));

    for (int i = 0; i < maxSteps; ++i)
    {
        if (! sc.contains (i)) continue;
        auto& s = tp.steps[(size_t) i];
        for (auto param : params)
        {
            switch (param)
            {
                case Param::on:          s.on = r.nextInt (100) < g.onChance; break;       // the ON range bar
                case Param::ccOn:        s.ccOn = r.nextInt (100) < g.ccOnChance; break;
                case Param::note:        s.pitchClass = (std::int8_t) pcs[(size_t) r.nextInt ((int) pcs.size())]; break;
                case Param::octave:      s.octave = (std::int8_t) (octLo <= octHi ? randomIn (r, octLo, octHi) : minOctave); break;
                case Param::velocity:    s.velocity = (std::uint8_t) juce::jlimit (1, 127, randomIn (r, g.rangeVel.min, g.rangeVel.max)); break;
                case Param::gate:        s.gate = (std::uint16_t) juce::jlimit (1, maxGate, randomIn (r, g.rangeGate.min, g.rangeGate.max)); break;
                case Param::probability: s.probability = (std::uint8_t) juce::jlimit (1, 100, randomIn (r, g.rangeProb.min, g.rangeProb.max)); break;
                case Param::cc:          s.cc = (std::uint8_t) juce::jlimit (0, 127, randomIn (r, g.rangeCC.min, g.rangeCC.max)); break;
            }
        }
    }
}

// Value of a param as one number (NOTE: combined pitch, in scale degrees when the scale is on)
static int paramValue (const Project& p, const TrackPattern& tp, const Step& s, Param param)
{
    const auto& g = p.globals;
    switch (param)
    {
        case Param::note:
        {
            int pitch = midiNote (s.octave, s.pitchClass);
            if (tp.scaleOn)
            {
                pitch = quantizeToScale (g.scaleMask(), g.root, pitch);
                return pitchToDegree (g.scaleMask(), g.root, pitch);
            }
            return pitch;
        }
        case Param::octave:      return s.octave;
        case Param::velocity:    return s.velocity;
        case Param::gate:        return s.gate;
        case Param::probability: return s.probability;
        case Param::cc:          return s.cc;
        case Param::on:          return s.on ? 1 : 0;
        case Param::ccOn:        return s.ccOn ? 1 : 0;
    }
    return 0;
}

// Limits of a param value (NOTE limits follow the octave range setting)
static void paramLimits (const Project& p, const TrackPattern& tp, Param param, int& lo, int& hi)
{
    const auto& g = p.globals;
    switch (param)
    {
        case Param::note:
        {
            const int loPitch = lowestPitch, hiPitch = highestPitch;
            if (tp.scaleOn)
            {
                const auto mask = g.scaleMask();
                lo = pitchToDegree (mask, g.root, loPitch);
                if (degreeToPitch (mask, g.root, lo) < loPitch) ++lo;
                hi = pitchToDegree (mask, g.root, hiPitch);
            }
            else
            {
                lo = loPitch;
                hi = hiPitch;
            }
            return;
        }
        case Param::octave:      lo = minOctave; hi = maxOctave; return;
        case Param::velocity:    lo = 1; hi = 127; return;
        case Param::gate:        lo = 1; hi = maxGate; return;
        case Param::probability: lo = 1; hi = 100; return;   // never 0 (like velocity)
        case Param::cc:          lo = 0; hi = 127; return;
        case Param::on:
        case Param::ccOn:        lo = 0; hi = 1; return;
    }
}

static void setParamValue (const Project& p, const TrackPattern& tp, Step& s, Param param, int v)
{
    const auto& g = p.globals;
    int lo = 0, hi = 0;
    paramLimits (p, tp, param, lo, hi);
    v = juce::jlimit (lo, hi, v);
    switch (param)
    {
        case Param::note:
            setPitch (s, tp.scaleOn ? degreeToPitch (g.scaleMask(), g.root, v) : v);
            break;
        case Param::octave:      s.octave = (std::int8_t) v; break;
        case Param::velocity:    s.velocity = (std::uint8_t) v; break;
        case Param::gate:        s.gate = (std::uint16_t) v; break;
        case Param::probability: s.probability = (std::uint8_t) v; break;
        case Param::cc:          s.cc = (std::uint8_t) v; break;
        case Param::on:          s.on = v != 0; break;
        case Param::ccOn:        s.ccOn = v != 0; break;
    }
}

void smooth (Project& p, const Scope& sc, bool ccMode, juce::Random* rnd)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    auto& r = rnd != nullptr ? *rnd : juce::Random::getSystemRandom();
    for (auto param : armedParams (p.globals, ccMode))
    {
        if (param == Param::on || param == Param::ccOn)
        {
            // ON (SPEC.md 9.3): the number of on steps moves one step toward the on chance
            // (chance x steps in scope), switching a random step, only when that gets strictly closer
            const bool cc = param == Param::ccOn;
            const double target = (cc ? p.globals.ccOnChance : p.globals.onChance) / 100.0 * sc.count();
            std::vector<int> onSteps, offSteps;
            for (int i = 0; i < maxSteps; ++i)
                if (sc.contains (i))
                    ((cc ? tp.steps[(size_t) i].ccOn : tp.steps[(size_t) i].on) ? onSteps : offSteps).push_back (i);
            const double now = (double) onSteps.size();
            auto flip = [&] (std::vector<int>& from, bool value)
            {
                if (from.empty()) return;
                auto& s = tp.steps[(size_t) from[(size_t) r.nextInt ((int) from.size())]];
                (cc ? s.ccOn : s.on) = value;
            };
            if (std::abs (now + 1.0 - target) < std::abs (now - target) - 1.0e-9) flip (offSteps, true);
            else if (std::abs (now - 1.0 - target) < std::abs (now - target) - 1.0e-9) flip (onSteps, false);
            continue;
        }
        // the CC param follows the CC on/off, the note params the note on/off
        auto active = [param] (const Step& st) { return param == Param::cc ? st.ccOn : st.on; };
        long long sum = 0;
        int n = 0;
        for (int i = 0; i < maxSteps; ++i)
            if (sc.contains (i) && active (tp.steps[(size_t) i])) { sum += paramValue (p, tp, tp.steps[(size_t) i], param); ++n; }
        if (n == 0)
            continue;
        const int avg = (int) std::llround ((double) sum / n);
        for (int i = 0; i < maxSteps; ++i)
        {
            auto& s = tp.steps[(size_t) i];
            if (! sc.contains (i) || ! active (s)) continue;   // off steps are not smoothed
            const int v = paramValue (p, tp, s, param);
            if (v != avg)
                setParamValue (p, tp, s, param, v + (v < avg ? 1 : -1));
        }
    }
}

// ---- curves, rotate, invert (SPEC.md 9.5) ----------------------------------------
namespace
{
    ParamRange rangeOf (const Globals& g, Param l)
    {
        switch (l)
        {
            case Param::note:        return g.rangeNote;
            case Param::octave:      return g.rangeOct;
            case Param::velocity:    return g.rangeVel;
            case Param::gate:        return g.rangeGate;
            case Param::probability: return g.rangeProb;
            case Param::cc:          return g.rangeCC;
            case Param::on: case Param::ccOn: break;
        }
        return { 0, 1 };
    }

    // NOTE positions: the pitch classes in the NOTE range, in-scale only when the track's scale is on
    std::vector<int> notePositions (const Globals& g, const TrackPattern& tp)
    {
        std::vector<int> pcs;
        const auto mask = g.scaleMask();
        const int lo = juce::jmin (g.rangeNote.min, g.rangeNote.max), hi = juce::jmax (g.rangeNote.min, g.rangeNote.max);
        for (int pc = lo; pc <= hi; ++pc)
            if (! tp.scaleOn || inScale (mask, g.root, pc)) pcs.push_back (pc);
        if (pcs.empty())
            for (int pc = 0; pc < 12; ++pc)
                if (! tp.scaleOn || inScale (mask, g.root, pc)) pcs.push_back (pc);
        return pcs;
    }

    // Raw stored value of a param (NOTE = pitch class only), for rotate
    int rawValue (const Step& s, Param l)
    {
        switch (l)
        {
            case Param::on:          return s.on ? 1 : 0;
            case Param::ccOn:        return s.ccOn ? 1 : 0;
            case Param::note:        return s.pitchClass;
            case Param::octave:      return s.octave;
            case Param::velocity:    return s.velocity;
            case Param::gate:        return s.gate;
            case Param::probability: return s.probability;
            case Param::cc:          return s.cc;
        }
        return 0;
    }

    void setRaw (Step& s, Param l, int v)
    {
        switch (l)
        {
            case Param::on:          s.on = v != 0; break;
            case Param::ccOn:        s.ccOn = v != 0; break;
            case Param::note:        s.pitchClass = (std::int8_t) juce::jlimit (0, 11, v); break;
            case Param::octave:      s.octave = (std::int8_t) juce::jlimit (minOctave, maxOctave, v); break;
            case Param::velocity:    s.velocity = (std::uint8_t) juce::jlimit (1, 127, v); break;
            case Param::gate:        s.gate = (std::uint16_t) juce::jlimit (1, maxGate, v); break;
            case Param::probability: s.probability = (std::uint8_t) juce::jlimit (1, 100, v); break;
            case Param::cc:          s.cc = (std::uint8_t) juce::jlimit (0, 127, v); break;
        }
    }

    // Shape value 0..1 at position x (0 = first step of the scope, 1 = last)
    double shapeAt (CurveShape shape, int pos, int span)
    {
        const double cyc = span > 0 ? (double) pos / (double) span : 0.0;        // one cycle over the span
        const double lin = span > 1 ? (double) pos / (double) (span - 1) : 0.0;  // 0 at the first, 1 at the last step
        switch (shape)
        {
            case CurveShape::sine:     return 0.5 + 0.5 * std::sin (cyc * juce::MathConstants<double>::twoPi);
            case CurveShape::triangle: return 1.0 - std::abs (2.0 * lin - 1.0);
            case CurveShape::ramp:     return lin;
            case CurveShape::square:   return cyc < 0.5 ? 1.0 : 0.0;
        }
        return 0.0;
    }
}

void applyCurve (Project& p, const Scope& sc, bool ccMode, CurveShape shape)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    const auto& g = p.globals;
    const int first = sc.first();
    if (first < 0) return;
    int last = first;
    for (int i = 0; i < maxSteps; ++i) if (sc.contains (i)) last = i;
    const int span = last - first + 1;   // by step position: gaps in the selection are respected
    const auto pcs = notePositions (g, tp);
    for (auto param : armedParams (g, ccMode))
        for (int i = 0; i < maxSteps; ++i)
        {
            if (! sc.contains (i)) continue;
            const double f = shapeAt (shape, i - first, span);
            auto& s = tp.steps[(size_t) i];
            if (param == Param::on || param == Param::ccOn)
            {
                // on where the curve is above 100 % minus the on chance (chance 50 % = the upper half)
                const int chance = param == Param::ccOn ? g.ccOnChance : g.onChance;
                setRaw (s, param, chance > 0 && f >= 1.0 - chance / 100.0 - 1.0e-9 ? 1 : 0);
            }
            else if (param == Param::note)
                s.pitchClass = (std::int8_t) pcs[(size_t) juce::jlimit (0, (int) pcs.size() - 1, (int) std::lround (f * (double) (pcs.size() - 1)))];
            else
            {
                const auto r = rangeOf (g, param);
                setRaw (s, param, (int) std::lround (r.min + f * (double) (r.max - r.min)));
            }
        }
}

void rotateSteps (Project& p, const Scope& sc, bool ccMode, int direction)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    std::vector<int> idx;
    for (int i = 0; i < maxSteps; ++i) if (sc.contains (i)) idx.push_back (i);
    const int n = (int) idx.size();
    if (n < 2) return;
    for (auto param : armedParams (p.globals, ccMode))
    {
        std::vector<int> v ((size_t) n);
        for (int k = 0; k < n; ++k) v[(size_t) k] = rawValue (tp.steps[(size_t) idx[(size_t) k]], param);
        for (int k = 0; k < n; ++k)
        {
            const int from = direction > 0 ? (k - 1 + n) % n : (k + 1) % n;   // > : the last value goes first
            setRaw (tp.steps[(size_t) idx[(size_t) k]], param, v[(size_t) from]);
        }
    }
}

void invertSteps (Project& p, const Scope& sc, bool ccMode)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    const auto& g = p.globals;
    const auto pcs = notePositions (g, tp);
    const int np = (int) pcs.size();
    for (auto param : armedParams (g, ccMode))
        for (int i = 0; i < maxSteps; ++i)
        {
            if (! sc.contains (i)) continue;
            auto& s = tp.steps[(size_t) i];
            if (param == Param::on || param == Param::ccOn)
                setRaw (s, param, rawValue (s, param) != 0 ? 0 : 1);
            else if (param == Param::note)
            {
                // the nearest NOTE position, mirrored
                int best = 0;
                for (int k = 1; k < np; ++k)
                    if (std::abs (pcs[(size_t) k] - s.pitchClass) < std::abs (pcs[(size_t) best] - s.pitchClass)) best = k;
                s.pitchClass = (std::int8_t) pcs[(size_t) (np - 1 - best)];
            }
            else
            {
                const auto r = rangeOf (g, param);
                setRaw (s, param, r.min + r.max - rawValue (s, param));
            }
        }
}

ShiftGesture beginShift (const Project& p, const Scope& sc, Param param)
{
    ShiftGesture gst;
    gst.scope = sc;
    gst.param = param;
    gst.snapshot = trackOf (p, sc.pattern, sc.track).steps;
    return gst;
}

void applyShift (Project& p, const ShiftGesture& gst, int amount)
{
    auto& tp = trackOf (p, gst.scope.pattern, gst.scope.track);
    for (int i = 0; i < maxSteps; ++i)
    {
        if (! gst.scope.contains (i)) continue;
        const auto& original = gst.snapshot[(size_t) i];
        auto s = original;
        if (gst.param != Param::on && gst.param != Param::ccOn)
            setParamValue (p, tp, s, gst.param, paramValue (p, tp, original, gst.param) + amount);
        tp.steps[(size_t) i] = s;
    }
}

std::vector<ClipStep> copySteps (const Project& p, const Scope& sc)
{
    const auto& tp = trackOf (p, sc.pattern, sc.track);
    std::vector<ClipStep> clip;
    const int first = sc.first();
    for (int i = 0; i < maxSteps; ++i)
        if (sc.contains (i))
            clip.push_back ({ i - first, tp.steps[(size_t) i] });
    return clip;
}

std::uint32_t pasteSteps (Project& p, int pattern, int track, int start, const std::vector<ClipStep>& clip,
                          const std::vector<Param>& params)
{
    auto& tp = trackOf (p, pattern, track);
    std::uint32_t written = 0;
    for (const auto& c : clip)
    {
        const int i = start + c.offset;
        if (i < 0 || i >= tp.length) continue;   // cut off at the track length, never wrap
        auto& s = tp.steps[(size_t) i];
        if (params.empty())
            s = c.step;
        for (auto l : params)
        {
            switch (l)
            {
                case Param::on:          s.on = c.step.on; break;
                case Param::ccOn:        s.ccOn = c.step.ccOn; break;
                case Param::note:        s.pitchClass = c.step.pitchClass; break;
                case Param::octave:      s.octave = c.step.octave; break;
                case Param::velocity:    s.velocity = c.step.velocity; break;
                case Param::gate:        s.gate = c.step.gate; break;
                case Param::probability: s.probability = c.step.probability; break;
                case Param::cc:          s.cc = c.step.cc; break;
            }
        }
        written |= (1u << i);
    }
    return written;
}

// ---------------------------------------------------------------------------------------
void UndoHistory::push (const Project& p, int pattern, const juce::String& label)
{
    pattern = juce::jlimit (0, numPatterns - 1, pattern);
    entries.push_back ({ pattern, p.patterns[(size_t) pattern], label });
    while (entries.size() > maxEntries)
        entries.pop_front();
    redoEntries.clear();   // a new edit: the undone steps can no longer be redone
}

bool UndoHistory::undo (Project& p, int& restoredPattern)
{
    if (entries.empty())
        return false;
    auto e = entries.back();
    entries.pop_back();
    redoEntries.push_back ({ e.pattern, p.patterns[(size_t) e.pattern], e.label });   // the state undo leaves
    while (redoEntries.size() > maxEntries)
        redoEntries.pop_front();
    p.patterns[(size_t) e.pattern] = e.data;
    restoredPattern = e.pattern;
    return true;
}

bool UndoHistory::redo (Project& p, int& restoredPattern)
{
    if (redoEntries.empty())
        return false;
    auto e = redoEntries.back();
    redoEntries.pop_back();
    entries.push_back ({ e.pattern, p.patterns[(size_t) e.pattern], e.label });   // undoable again
    while (entries.size() > maxEntries)
        entries.pop_front();
    p.patterns[(size_t) e.pattern] = e.data;
    restoredPattern = e.pattern;
    return true;
}
} // namespace vima
