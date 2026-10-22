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

std::vector<Lane> activeLanes (const Globals& g, bool ccMode)
{
    std::vector<Lane> lanes;
    if (g.laneOn) lanes.push_back (Lane::on);
    if (ccMode)
    {
        if (g.laneCC) lanes.push_back (Lane::cc);
        return lanes;
    }
    if (g.laneNote) lanes.push_back (Lane::note);
    if (g.laneOct) lanes.push_back (Lane::octave);
    if (g.laneVel) lanes.push_back (Lane::velocity);
    if (g.laneGate) lanes.push_back (Lane::gate);
    if (g.laneProb) lanes.push_back (Lane::probability);
    return lanes;
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

static void setPitch (Step& s, int pitch, const Globals& g)
{
    const int lo = midiNote (g.octaveLow, 0), hi = midiNote (g.octaveHigh, 11);
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
    s.octave = (std::int8_t) juce::jlimit (p.globals.octaveLow, p.globals.octaveHigh, s.octave + delta);
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
    const auto lanes = activeLanes (g, ccMode);
    const auto mask = g.scaleMask();

    std::vector<int> pcs;
    for (int pc = juce::jmin (g.rangeNote.min, g.rangeNote.max); pc <= juce::jmax (g.rangeNote.min, g.rangeNote.max); ++pc)
        if (! tp.scaleOn || inScale (mask, g.root, pc)) pcs.push_back (pc);
    if (pcs.empty())
        for (int pc = 0; pc < 12; ++pc) pcs.push_back (pc);

    const int octLo = juce::jmax (g.octaveLow, juce::jmin (g.rangeOct.min, g.rangeOct.max));
    const int octHi = juce::jmin (g.octaveHigh, juce::jmax (g.rangeOct.min, g.rangeOct.max));

    for (int i = 0; i < maxSteps; ++i)
    {
        if (! sc.contains (i)) continue;
        auto& s = tp.steps[(size_t) i];
        for (auto lane : lanes)
        {
            switch (lane)
            {
                case Lane::on:          s.on = r.nextFloat() < 0.7f; break;
                case Lane::note:        s.pitchClass = (std::int8_t) pcs[(size_t) r.nextInt ((int) pcs.size())]; break;
                case Lane::octave:      s.octave = (std::int8_t) (octLo <= octHi ? randomIn (r, octLo, octHi) : g.octaveLow); break;
                case Lane::velocity:    s.velocity = (std::uint8_t) juce::jlimit (1, 127, randomIn (r, g.rangeVel.min, g.rangeVel.max)); break;
                case Lane::gate:        s.gate = (std::uint16_t) juce::jlimit (1, maxGate, randomIn (r, g.rangeGate.min, g.rangeGate.max)); break;
                case Lane::probability: s.probability = (std::uint8_t) juce::jlimit (0, 100, randomIn (r, g.rangeProb.min, g.rangeProb.max)); break;
                case Lane::cc:          s.cc = (std::uint8_t) juce::jlimit (0, 127, randomIn (r, g.rangeCC.min, g.rangeCC.max)); break;
            }
        }
    }
}

// Value of a lane as one number (NOTE: combined pitch, in scale degrees when the scale is on)
static int laneValue (const Project& p, const TrackPattern& tp, const Step& s, Lane lane)
{
    const auto& g = p.globals;
    switch (lane)
    {
        case Lane::note:
        {
            int pitch = midiNote (s.octave, s.pitchClass);
            if (tp.scaleOn)
            {
                pitch = quantizeToScale (g.scaleMask(), g.root, pitch);
                return pitchToDegree (g.scaleMask(), g.root, pitch);
            }
            return pitch;
        }
        case Lane::octave:      return s.octave;
        case Lane::velocity:    return s.velocity;
        case Lane::gate:        return s.gate;
        case Lane::probability: return s.probability;
        case Lane::cc:          return s.cc;
        case Lane::on:          return s.on ? 1 : 0;
    }
    return 0;
}

// Limits of a lane value (NOTE limits follow the octave range setting)
static void laneLimits (const Project& p, const TrackPattern& tp, Lane lane, int& lo, int& hi)
{
    const auto& g = p.globals;
    switch (lane)
    {
        case Lane::note:
        {
            const int loPitch = midiNote (g.octaveLow, 0), hiPitch = midiNote (g.octaveHigh, 11);
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
        case Lane::octave:      lo = g.octaveLow; hi = g.octaveHigh; return;
        case Lane::velocity:    lo = 1; hi = 127; return;
        case Lane::gate:        lo = 1; hi = maxGate; return;
        case Lane::probability: lo = 0; hi = 100; return;
        case Lane::cc:          lo = 0; hi = 127; return;
        case Lane::on:          lo = 0; hi = 1; return;
    }
}

static void setLaneValue (const Project& p, const TrackPattern& tp, Step& s, Lane lane, int v)
{
    const auto& g = p.globals;
    int lo = 0, hi = 0;
    laneLimits (p, tp, lane, lo, hi);
    v = juce::jlimit (lo, hi, v);
    switch (lane)
    {
        case Lane::note:
            setPitch (s, tp.scaleOn ? degreeToPitch (g.scaleMask(), g.root, v) : v, g);
            break;
        case Lane::octave:      s.octave = (std::int8_t) v; break;
        case Lane::velocity:    s.velocity = (std::uint8_t) v; break;
        case Lane::gate:        s.gate = (std::uint16_t) v; break;
        case Lane::probability: s.probability = (std::uint8_t) v; break;
        case Lane::cc:          s.cc = (std::uint8_t) v; break;
        case Lane::on:          s.on = v != 0; break;
    }
}

void smooth (Project& p, const Scope& sc, bool ccMode)
{
    auto& tp = trackOf (p, sc.pattern, sc.track);
    for (auto lane : activeLanes (p.globals, ccMode))
    {
        if (lane == Lane::on)
            continue;   // on/off has no average
        long long sum = 0;
        int n = 0;
        for (int i = 0; i < maxSteps; ++i)
            if (sc.contains (i) && tp.steps[(size_t) i].on) { sum += laneValue (p, tp, tp.steps[(size_t) i], lane); ++n; }
        if (n == 0)
            continue;
        const int avg = (int) std::llround ((double) sum / n);
        for (int i = 0; i < maxSteps; ++i)
        {
            auto& s = tp.steps[(size_t) i];
            if (! sc.contains (i) || ! s.on) continue;   // off steps are not smoothed
            const int v = laneValue (p, tp, s, lane);
            if (v != avg)
                setLaneValue (p, tp, s, lane, v + (v < avg ? 1 : -1));
        }
    }
}

ShiftGesture beginShift (const Project& p, const Scope& sc, Lane lane)
{
    ShiftGesture gst;
    gst.scope = sc;
    gst.lane = lane;
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
        if (gst.lane != Lane::on)
            setLaneValue (p, tp, s, gst.lane, laneValue (p, tp, original, gst.lane) + amount);
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
                          const std::vector<Lane>& lanes)
{
    auto& tp = trackOf (p, pattern, track);
    std::uint32_t written = 0;
    for (const auto& c : clip)
    {
        const int i = start + c.offset;
        if (i < 0 || i >= tp.length) continue;   // cut off at the track length, never wrap
        auto& s = tp.steps[(size_t) i];
        if (lanes.empty())
            s = c.step;
        for (auto l : lanes)
        {
            switch (l)
            {
                case Lane::on:          s.on = c.step.on; break;
                case Lane::note:        s.pitchClass = c.step.pitchClass; break;
                case Lane::octave:      s.octave = c.step.octave; break;
                case Lane::velocity:    s.velocity = c.step.velocity; break;
                case Lane::gate:        s.gate = c.step.gate; break;
                case Lane::probability: s.probability = c.step.probability; break;
                case Lane::cc:          s.cc = c.step.cc; break;
            }
        }
        written |= (1u << i);
    }
    return written;
}

// ---------------------------------------------------------------------------------------
void UndoHistory::push (const Project& p, int pattern)
{
    pattern = juce::jlimit (0, numPatterns - 1, pattern);
    entries.push_back ({ pattern, p.patterns[(size_t) pattern] });
    while (entries.size() > maxEntries)
        entries.pop_front();
}

bool UndoHistory::undo (Project& p, int& restoredPattern)
{
    if (entries.empty())
        return false;
    auto e = entries.back();
    entries.pop_back();
    p.patterns[(size_t) e.pattern] = e.data;
    restoredPattern = e.pattern;
    return true;
}
} // namespace vima
