#include "Model.h"
#include <atomic>

namespace vima
{
// ---------------------------------------------------------------------------------------
namespace userSettings
{
namespace
{
    std::atomic<int> settingsRevision { 0 };
    std::atomic<bool> lightOn { false };
    std::atomic<int> middleCValue { 3 };
    std::atomic<bool> loaded { false };
    juce::CriticalSection fileLock;

    juce::File& settingsFile()
    {
        // The app settings folder (no permission prompts or blocks, unlike Documents):
        // Windows %APPDATA%\VIMA, macOS ~/Library/Application Support/VIMA, Linux ~/.config/VIMA
        static juce::File f = []
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
           #if JUCE_MAC
            dir = dir.getChildFile ("Application Support");
           #endif
            return dir.getChildFile ("VIMA").getChildFile ("Settings.xml");
        }();
        return f;
    }

    void ensureLoaded()
    {
        if (loaded.load()) return;
        const juce::ScopedLock sl (fileLock);
        if (loaded.load()) return;
        bool light = false;
        int mc = 3;
        if (auto xml = juce::XmlDocument::parse (settingsFile()))
        {
            light = xml->getBoolAttribute ("lightTheme", false);
            mc = juce::jlimit (3, 5, xml->getIntAttribute ("middleC", 3));
        }
        lightOn = light;
        middleCValue = mc;
        loaded = true;
    }

    void save()
    {
        const juce::ScopedLock sl (fileLock);
        juce::XmlElement xml ("VIMASETTINGS");
        xml.setAttribute ("lightTheme", lightOn.load());
        xml.setAttribute ("middleC", middleCValue.load());
        settingsFile().getParentDirectory().createDirectory();
        xml.writeTo (settingsFile());   // if it cannot be written, the change still holds for the session
    }
}

bool lightTheme() { ensureLoaded(); return lightOn.load(); }
int middleC() { ensureLoaded(); return middleCValue.load(); }
int revision() { return settingsRevision.load(); }

void setLightTheme (bool l)
{
    ensureLoaded();
    if (l == lightOn.load()) return;
    lightOn = l;
    ++settingsRevision;
    save();
}

void setMiddleC (int m)
{
    ensureLoaded();
    m = juce::jlimit (3, 5, m);
    if (m == middleCValue.load()) return;
    middleCValue = m;
    ++settingsRevision;
    save();
}

void setFile (const juce::File& f)
{
    const juce::ScopedLock sl (fileLock);
    settingsFile() = f;
    loaded = false;
    ++settingsRevision;
}
} // namespace userSettings

// ---------------------------------------------------------------------------------------
static const DivisionInfo divisions[numDivisions] = {
    { "1/1", 4.0 },  { "1/2", 2.0 },        { "1/4", 1.0 },        { "1/8", 0.5 },          { "1/16", 0.25 }, { "1/32", 0.125 },
    { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 }, { "1/32T", 1.0 / 12.0 },
};

const DivisionInfo& divisionInfo (int index) { return divisions[juce::jlimit (0, numDivisions - 1, index)]; }
int defaultDivision() { return 4; }

static const ScaleInfo scales[numScales] = {
    { "MAJ",   "MAJOR",      0xAB5 },
    { "MIN",   "MINOR",      0x5AD },
    { "HMIN",  "HARM MIN",   0x9AD },
    { "MMIN",  "MEL MIN",    0xAAD },
    { "DOR",   "DORIAN",     0x6AD },
    { "PHR",   "PHRYGIAN",   0x5AB },
    { "LYD",   "LYDIAN",     0xAD5 },
    { "MIX",   "MIXOLYD",    0x6B5 },
    { "LOC",   "LOCRIAN",    0x56B },
    { "PMAJ",  "PENTA MAJ",  0x295 },
    { "PMIN",  "PENTA MIN",  0x4A9 },
    { "BLUES", "BLUES",      0x4E9 },
    { "WHOLE", "WHOLE TONE", 0x555 },
    { "USER",  "USER",       0x000 },   // the mask comes from Globals::userScale
};

const ScaleInfo& scaleInfo (int index) { return scales[juce::jlimit (0, numScales - 1, index)]; }

std::uint16_t Globals::scaleMask() const
{
    auto m = scale == userScaleIndex ? userScale : scaleInfo (scale).mask;
    return (std::uint16_t) ((m | 1) & 0xFFF);   // the root is always in the scale
}

// ---------------------------------------------------------------------------------------
static int mod12 (int v) { return ((v % 12) + 12) % 12; }
static int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
static int popcount12 (std::uint16_t m)
{
    int n = 0;
    for (int i = 0; i < 12; ++i) n += (m >> i) & 1;
    return n;
}

bool inScale (std::uint16_t mask, int root, int pitchClass)
{
    return ((mask >> mod12 (pitchClass - root)) & 1) != 0;
}

int quantizeToScale (std::uint16_t mask, int root, int m)
{
    if (inScale (mask, root, m)) return m;
    for (int d = 1; d < 12; ++d)
    {
        if (inScale (mask, root, m - d)) return m - d;
        if (inScale (mask, root, m + d)) return m + d;
    }
    return m;
}

int scaleSize (std::uint16_t mask) { return juce::jmax (1, popcount12 ((std::uint16_t) (mask | 1))); }

int pitchToDegree (std::uint16_t mask, int root, int m)
{
    mask = (std::uint16_t) (mask | 1);
    const int rel = m - root;
    const int oct = floorDiv (rel, 12);
    const int pcRel = rel - oct * 12;
    int idx = popcount12 ((std::uint16_t) (mask & ((1u << pcRel) - 1u)));
    if (((mask >> pcRel) & 1) == 0) idx -= 1;   // not in scale: floor to the degree below
    return oct * scaleSize (mask) + idx;
}

int degreeToPitch (std::uint16_t mask, int root, int degree)
{
    mask = (std::uint16_t) (mask | 1);
    const int size = scaleSize (mask);
    const int oct = floorDiv (degree, size);
    int idx = degree - oct * size;
    int semis = 0;
    for (int s = 0; s < 12; ++s)
        if ((mask >> s) & 1)
        {
            if (idx == 0) { semis = s; break; }
            --idx;
        }
    return root + oct * 12 + semis;
}

const char* pitchClassName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[mod12 (pc)];
}

juce::String patternLabel (int patternIndex)
{
    return juce::String (patternIndex + 1).paddedLeft ('0', 2);   // "05", "21"
}

juce::String pageRangeText (int page)
{
    return juce::String (page * patternsPerPage + 1) + " - " + juce::String (page * patternsPerPage + patternsPerPage);
}

// ---------------------------------------------------------------------------------------
bool TrackPattern::hasOnSteps() const
{
    for (int i = 0; i < length; ++i)
        if (steps[(size_t) i].on || steps[(size_t) i].ccOn) return true;
    return false;
}

int TrackPattern::onStepCount() const
{
    int n = 0;
    for (int i = 0; i < length; ++i)
        if (steps[(size_t) i].on) ++n;
    return n;
}

int TrackPattern::ccOnStepCount() const
{
    int n = 0;
    for (int i = 0; i < length; ++i)
        if (steps[(size_t) i].ccOn) ++n;
    return n;
}

bool Pattern::isEmpty() const
{
    for (const auto& t : tracks)
        if (t.hasOnSteps()) return false;
    return true;
}

double Pattern::lengthPpq() const
{
    double longest = 0.0;
    for (const auto& t : tracks)
        longest = juce::jmax (longest, t.length * divisionInfo (t.division).stepPpq);
    return longest;
}

// ---------------------------------------------------------------------------------------
Project::Project() { resetToDefaults(); }

void Project::resetToDefaults()
{
    for (int i = 0; i < numPatterns; ++i) patterns[(size_t) i] = defaultPattern (i);
    for (int t = 0; t < numTracks; ++t)
    {
        routing[(size_t) t] = TrackRouting();
        routing[(size_t) t].midiChannel = (std::uint8_t) (t + 1);
        routing[(size_t) t].ccNumber = (std::uint8_t) (102 + t);
        routing[(size_t) t].noteOut = t == 0;
    }
    globals = Globals();
    currentPattern = 0;
    for (auto& m : macros) m = MacroMap();
}

// Steps are stored as 9 bytes each (gate in two bytes, little endian).
static juce::String encodeSteps (const std::array<Step, maxSteps>& steps)
{
    juce::MemoryBlock mb;
    for (const auto& s : steps)
    {
        const std::uint8_t bytes[9] = { (std::uint8_t) (s.on ? 1 : 0), (std::uint8_t) s.pitchClass,
                                        (std::uint8_t) (s.octave - minOctave), s.velocity,
                                        (std::uint8_t) (s.gate & 0xFF), (std::uint8_t) (s.gate >> 8), s.probability, s.cc,
                                        (std::uint8_t) (s.ccOn ? 1 : 0) };
        mb.append (bytes, sizeof (bytes));
    }
    return mb.toBase64Encoding();
}

static void decodeSteps (const juce::String& text, std::array<Step, maxSteps>& steps)
{
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (text))
        return;
    constexpr size_t stride = 9;
    if (mb.getSize() < (size_t) maxSteps * stride)
        return;
    const auto* d = static_cast<const std::uint8_t*> (mb.getData());
    for (int i = 0; i < maxSteps; ++i)
    {
        const auto* b = d + (size_t) i * stride;
        Step s;
        s.on = b[0] != 0;
        s.pitchClass = (std::int8_t) juce::jlimit (0, 11, (int) b[1]);
        s.octave = (std::int8_t) juce::jlimit (minOctave, maxOctave, (int) b[2] + minOctave);
        s.velocity = (std::uint8_t) juce::jlimit (1, 127, (int) b[3]);
        const int gate = (int) b[4] | ((int) b[5] << 8);
        s.gate = (std::uint16_t) juce::jlimit (1, maxGate, gate);
        s.probability = (std::uint8_t) juce::jlimit (1, 100, (int) b[6]);   // never 0
        s.cc = (std::uint8_t) juce::jlimit (0, 127, (int) b[7]);
        s.ccOn = b[8] != 0;
        steps[(size_t) i] = s;
    }
}

Pattern blankPattern()
{
    Pattern p;
    for (auto& t : p.tracks)
        for (auto& s : t.steps) { s.on = false; s.ccOn = false; }
    return p;
}

const Pattern& defaultPattern (int index)
{
    static const Pattern init;
    static const Pattern blank = blankPattern();
    return index == 0 ? init : blank;
}

static bool patternEquals (const Pattern& a, const Pattern& b)
{
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& x = a.tracks[(size_t) t];
        const auto& y = b.tracks[(size_t) t];
        if (x.length != y.length || x.division != y.division || x.direction != y.direction || x.swing != y.swing
            || x.scaleOn != y.scaleOn || x.steps != y.steps)
            return false;
    }
    return true;
}

static void appendMacros (juce::ValueTree& root, const std::array<MacroMap, numMacros>& macros)
{
    for (int i = 0; i < numMacros; ++i)
    {
        const auto& m = macros[(size_t) i];
        if (! m.mapped()) continue;
        juce::ValueTree mt ("Macro");
        mt.setProperty ("index", i, nullptr);
        mt.setProperty ("target", (int) m.target, nullptr);
        mt.setProperty ("track", (int) m.track, nullptr);
        mt.setProperty ("pattern", (int) m.pattern, nullptr);
        mt.setProperty ("step", (int) m.step, nullptr);
        root.appendChild (mt, nullptr);
    }
}

static void readMacro (const juce::ValueTree& mt, std::array<MacroMap, numMacros>& macros)
{
    const int i = (int) mt.getProperty ("index", -1);
    const int target = (int) mt.getProperty ("target", 0);
    if (i < 0 || i >= numMacros || target <= 0 || target >= numMacroTargets) return;
    auto& m = macros[(size_t) i];
    m.target = (MacroTarget) target;
    m.track = (std::int8_t) juce::jlimit (0, numTracks - 1, (int) mt.getProperty ("track", 0));
    m.pattern = (std::int8_t) juce::jlimit (0, numPatterns - 1, (int) mt.getProperty ("pattern", 0));
    m.step = (std::uint8_t) juce::jlimit (0, maxSteps - 1, (int) mt.getProperty ("step", 0));
}

juce::ValueTree Project::musicToValueTree() const
{
    auto root = toValueTree();
    root.setProperty ("content", "preset", nullptr);
    for (int i = root.getNumChildren(); --i >= 0;)
        if (root.getChild (i).hasType ("Macro"))
            root.removeChild (i, nullptr);
    // Settings that live only in the Settings dialog are not part of a preset (SPEC.md 13.3);
    // the user scale and the root stay (a preset can select USER)
    auto g = root.getChildWithName ("Globals");
    for (auto* id : { "ccSmoothing", "patternCC", "patternCCChannel", "patternCCNumber", "quantize" })
        g.removeProperty (id, nullptr);
    return root;
}

// Keep the Settings-dialog-only settings across a preset load or preset NEW
static void keepDialogSettings (Globals& g, const Globals& old)
{
    g.ccSmoothing = old.ccSmoothing;
    g.patternCC = old.patternCC;
    g.patternCCChannel = old.patternCCChannel;
    g.patternCCNumber = old.patternCCNumber;
    g.quantize = old.quantize;
}

void Project::loadMusic (const juce::ValueTree& tree)
{
    const auto keepMacros = macros;
    const auto keepGlobals = globals;
    fromValueTree (tree);
    macros = keepMacros;
    keepDialogSettings (globals, keepGlobals);
}

void Project::resetMusic()
{
    const auto keepMacros = macros;
    const auto keepGlobals = globals;
    resetToDefaults();
    macros = keepMacros;
    keepDialogSettings (globals, keepGlobals);
}

juce::ValueTree Project::macrosToValueTree() const
{
    juce::ValueTree root ("VIMAMACROS");
    root.setProperty ("version", 1, nullptr);
    appendMacros (root, macros);
    return root;
}

bool Project::loadMacros (const juce::ValueTree& root)
{
    if (! root.hasType ("VIMAMACROS"))
        return false;
    for (auto& m : macros) m = MacroMap();
    for (auto mt : root)
        if (mt.hasType ("Macro"))
            readMacro (mt, macros);
    return true;
}

// A pattern's 8 tracks as `Track` children (project state, presets and pattern files)
static void appendTracks (juce::ValueTree& parent, const Pattern& pat)
{
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& tr = pat.tracks[(size_t) t];
        juce::ValueTree tt ("Track");
        tt.setProperty ("index", t, nullptr);
        tt.setProperty ("length", (int) tr.length, nullptr);
        tt.setProperty ("division", (int) tr.division, nullptr);
        tt.setProperty ("direction", (int) tr.direction, nullptr);
        tt.setProperty ("swing", (int) tr.swing, nullptr);
        tt.setProperty ("scaleOn", tr.scaleOn, nullptr);
        tt.setProperty ("steps", encodeSteps (tr.steps), nullptr);
        parent.appendChild (tt, nullptr);
    }
}

static void readTracks (const juce::ValueTree& parent, Pattern& pat)
{
    for (auto tt : parent)
    {
        if (! tt.hasType ("Track")) continue;
        const int t = (int) tt.getProperty ("index", -1);
        if (t < 0 || t >= numTracks) continue;
        auto& tr = pat.tracks[(size_t) t];
        tr.length = (std::uint8_t) juce::jlimit (minLength, maxSteps, (int) tt.getProperty ("length", 16));
        tr.division = (std::uint8_t) juce::jlimit (0, numDivisions - 1, (int) tt.getProperty ("division", defaultDivision()));
        tr.direction = (Direction) juce::jlimit (0, 2, (int) tt.getProperty ("direction", 0));
        tr.swing = (std::uint8_t) juce::jlimit (0, 100, (int) tt.getProperty ("swing", 0));
        tr.scaleOn = (bool) tt.getProperty ("scaleOn", true);
        decodeSteps (tt.getProperty ("steps").toString(), tr.steps);
    }
}

juce::ValueTree Project::toValueTree() const
{
    juce::ValueTree root ("VIMA");
    root.setProperty ("version", 2, nullptr);
    root.setProperty ("currentPattern", currentPattern, nullptr);

    const auto& g = globals;
    juce::ValueTree gt ("Globals");
    gt.setProperty ("scale", g.scale, nullptr);
    gt.setProperty ("root", g.root, nullptr);
    gt.setProperty ("userScale", (int) g.userScale, nullptr);
    gt.setProperty ("ccSmoothing", g.ccSmoothing, nullptr);
    gt.setProperty ("patternCC", g.patternCC, nullptr);
    gt.setProperty ("patternCCChannel", g.patternCCChannel, nullptr);
    gt.setProperty ("patternCCNumber", g.patternCCNumber, nullptr);
    gt.setProperty ("quantize", (int) g.quantize, nullptr);
    gt.setProperty ("chainOn", g.chainOn, nullptr);
    gt.setProperty ("chainLoop", g.chainLoop, nullptr);
    juce::StringArray chainText;
    for (const auto& e : g.chain) chainText.add (juce::String (e.pattern) + ":" + juce::String (e.repeats));
    gt.setProperty ("chain", chainText.joinIntoString (","), nullptr);
    juce::String params;
    for (bool b : { g.armOn, g.armNote, g.armOct, g.armVel, g.armGate, g.armProb, g.armCC, g.armCcOn, g.armAllNote })
        params << (b ? "1" : "0");
    gt.setProperty ("params", params, nullptr);
    juce::StringArray ranges;
    for (const auto* r : { &g.rangeNote, &g.rangeOct, &g.rangeVel, &g.rangeGate, &g.rangeProb, &g.rangeCC })
        ranges.add (juce::String (r->min) + ":" + juce::String (r->max));
    gt.setProperty ("ranges", ranges.joinIntoString (","), nullptr);
    root.appendChild (gt, nullptr);

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& r = routing[(size_t) t];
        juce::ValueTree rt ("Routing");
        rt.setProperty ("track", t, nullptr);
        rt.setProperty ("channel", (int) r.midiChannel, nullptr);
        rt.setProperty ("noteOut", r.noteOut, nullptr);
        rt.setProperty ("ccOut", r.ccOut, nullptr);
        rt.setProperty ("ccNumber", (int) r.ccNumber, nullptr);
        root.appendChild (rt, nullptr);
    }

    appendMacros (root, macros);

    for (int p = 0; p < numPatterns; ++p)
    {
        const auto& pat = patterns[(size_t) p];
        if (patternEquals (pat, defaultPattern (p)))
            continue;
        juce::ValueTree pt ("Pattern");
        pt.setProperty ("index", p, nullptr);
        appendTracks (pt, pat);
        root.appendChild (pt, nullptr);
    }
    return root;
}

void Project::fromValueTree (const juce::ValueTree& root)
{
    if (! root.hasType ("VIMA"))
        return;
    resetToDefaults();
    currentPattern = juce::jlimit (0, numPatterns - 1, (int) root.getProperty ("currentPattern", 0));

    auto gt = root.getChildWithName ("Globals");
    if (gt.isValid())
    {
        auto& g = globals;
        g.scale = juce::jlimit (0, numScales - 1, (int) gt.getProperty ("scale", g.scale));
        g.root = juce::jlimit (0, 11, (int) gt.getProperty ("root", g.root));
        g.userScale = (std::uint16_t) (((int) gt.getProperty ("userScale", (int) g.userScale) | 1) & 0xFFF);
        g.ccSmoothing = (bool) gt.getProperty ("ccSmoothing", g.ccSmoothing);
        g.patternCC = (bool) gt.getProperty ("patternCC", g.patternCC);
        g.patternCCChannel = juce::jlimit (1, 16, (int) gt.getProperty ("patternCCChannel", g.patternCCChannel));
        g.patternCCNumber = juce::jlimit (0, 127, (int) gt.getProperty ("patternCCNumber", g.patternCCNumber));
        g.quantize = (Quantize) juce::jlimit (0, 3, (int) gt.getProperty ("quantize", (int) g.quantize));
        g.chainOn = (bool) gt.getProperty ("chainOn", g.chainOn);
        g.chainLoop = (bool) gt.getProperty ("chainLoop", g.chainLoop);
        g.chain.clear();
        for (const auto& item : juce::StringArray::fromTokens (gt.getProperty ("chain").toString(), ",", ""))
        {
            if (g.chain.size() >= maxChainEntries) break;
            auto parts = juce::StringArray::fromTokens (item, ":", "");
            if (parts.size() == 2)
                g.chain.add ({ (std::uint8_t) juce::jlimit (0, numPatterns - 1, parts[0].getIntValue()),
                               (std::uint8_t) juce::jlimit (1, 99, parts[1].getIntValue()) });
        }
        const auto params = gt.getProperty ("params").toString();
        if (params.length() == 9)
        {
            bool* flags[] = { &g.armOn, &g.armNote, &g.armOct, &g.armVel, &g.armGate, &g.armProb, &g.armCC, &g.armCcOn, &g.armAllNote };
            for (int i = 0; i < 9; ++i) *flags[i] = params[i] == '1';
        }
        // something is always armed
        if (! g.armAllNote && ! (g.armOn || g.armNote || g.armOct || g.armVel || g.armGate || g.armProb)) g.armNote = true;
        if (! (g.armCcOn || g.armCC)) g.armCC = true;
        auto rangeParts = juce::StringArray::fromTokens (gt.getProperty ("ranges").toString(), ",", "");
        ParamRange* rs[] = { &g.rangeNote, &g.rangeOct, &g.rangeVel, &g.rangeGate, &g.rangeProb, &g.rangeCC };
        if (rangeParts.size() == 6)
            for (int i = 0; i < 6; ++i)
            {
                auto mm = juce::StringArray::fromTokens (rangeParts[i], ":", "");
                if (mm.size() == 2) { rs[i]->min = mm[0].getIntValue(); rs[i]->max = mm[1].getIntValue(); }
            }
        g.rangeProb.min = juce::jlimit (1, 100, g.rangeProb.min);
        g.rangeProb.max = juce::jlimit (1, 100, g.rangeProb.max);
    }

    for (auto rt : root)
    {
        if (rt.hasType ("Routing"))
        {
            const int t = (int) rt.getProperty ("track", -1);
            if (t < 0 || t >= numTracks) continue;
            auto& r = routing[(size_t) t];
            r.midiChannel = (std::uint8_t) juce::jlimit (1, 16, (int) rt.getProperty ("channel", t + 1));
            r.noteOut = (bool) rt.getProperty ("noteOut", true);
            r.ccOut = (bool) rt.getProperty ("ccOut", false);
            r.ccNumber = (std::uint8_t) juce::jlimit (0, 127, (int) rt.getProperty ("ccNumber", 74));
        }
        else if (rt.hasType ("Macro"))
        {
            readMacro (rt, macros);
        }
        else if (rt.hasType ("Pattern"))
        {
            const int p = (int) rt.getProperty ("index", -1);
            if (p < 0 || p >= numPatterns) continue;
            readTracks (rt, patterns[(size_t) p]);
        }
    }
}

juce::ValueTree Project::patternToValueTree (int index) const
{
    juce::ValueTree root ("VIMAPATTERN");
    root.setProperty ("version", 1, nullptr);
    appendTracks (root, patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)]);
    return root;
}

bool Project::loadPattern (int index, const juce::ValueTree& root)
{
    if (! root.hasType ("VIMAPATTERN") || index < 0 || index >= numPatterns)
        return false;
    auto pat = blankPattern();
    readTracks (root, pat);
    patterns[(size_t) index] = pat;
    return true;
}
} // namespace vima
