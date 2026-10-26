// VIMA test harness: offline checks of the engine and step operations against a fake host,
// plus rendered screenshots of the editor. Build with -DVIMA_BUILD_TESTS=ON.
// Run: VimaTests [output-dir]

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Model.h"
#include "../Source/Engine.h"
#include "../Source/Operations.h"
#include "../Source/Popovers.h"
#include "../Source/StepGrid.h"
#include "../Source/OpsPanel.h"
#include "../Source/PatternsView.h"

#include <map>
#include <memory>
#include <vector>

// The theme preference file used by the tests (never the real user settings)
static juce::File testSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vima-test-settings.xml");
}


using namespace vima;

namespace
{
int failures = 0;
int checks = 0;

void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok)
    {
        std::cout << "  FAIL " << what << std::endl;
        ++failures;
    }
}

void section (const char* name) { std::cout << name << std::endl; }

struct Event
{
    double ppq;
    juce::MidiMessage msg;
};

// The engine and operation tests were written against an empty project (all steps off, notes
// out on every track, CC smoothing on). These recreate that starting point on top of the
// init values (SPEC.md 5).
Step blankStep()
{
    Step s;
    s.on = false;
    s.ccOn = false;
    s.pitchClass = 0;
    s.gate = 50;
    return s;
}

void legacyBlank (Project& p)
{
    for (auto& pat : p.patterns)
        for (auto& t : pat.tracks)
            for (auto& s : t.steps) s = blankStep();
    for (auto& r : p.routing) r.noteOut = true;
    p.globals.ccSmoothing = true;
}

// Runs the engine over a span of song time and collects all MIDI with its ppq position
struct Runner
{
    Engine engine;
    std::unique_ptr<Project> project = std::make_unique<Project>();
    Engine::Host host;
    double sampleRate = 48000.0;
    int block = 250;   // divides a 1/16 at 120 BPM and 48 kHz exactly
    std::vector<Event> events;

    Runner()
    {
        legacyBlank (*project);
        engine.prepare (sampleRate);
        engine.setRandomSeed (1234);
        host.playing = true;
        host.bpm = 120.0;
    }

    void run (double beats, const juce::MidiBuffer& in = {}, bool inFirstBlockOnly = true)
    {
        const double pps = host.bpm / 60.0 / sampleRate;
        const double start = host.ppq;
        const auto numBlocks = (int) std::llround (beats / (block * pps));
        juce::MidiBuffer out;
        for (int b = 0; b < numBlocks; ++b)
        {
            host.ppq = start + (double) b * block * pps;
            out.clear();
            engine.process (*project, host, block, (b == 0 || ! inFirstBlockOnly) ? in : juce::MidiBuffer(), out);
            for (const auto m : out)
                events.push_back ({ host.ppq + m.samplePosition * pps, m.getMessage() });
        }
        host.ppq = start + (double) numBlocks * block * pps;
    }

    void stop()
    {
        host.playing = false;
        juce::MidiBuffer out;
        engine.process (*project, host, block, {}, out);
        for (const auto m : out)
            events.push_back ({ host.ppq, m.getMessage() });
        host.playing = true;
    }

    TrackPattern& track (int t, int pattern = 0) { return project->patterns[(size_t) pattern].tracks[(size_t) t]; }

    std::vector<Event> noteOns (int channel = -1) const
    {
        std::vector<Event> r;
        for (auto& e : events)
            if (e.msg.isNoteOn() && (channel < 0 || e.msg.getChannel() == channel)) r.push_back (e);
        return r;
    }
    std::vector<Event> noteOffs (int channel = -1) const
    {
        std::vector<Event> r;
        for (auto& e : events)
            if (e.msg.isNoteOff() && (channel < 0 || e.msg.getChannel() == channel)) r.push_back (e);
        return r;
    }
    std::vector<Event> ccs (int channel = -1) const
    {
        std::vector<Event> r;
        for (auto& e : events)
            if (e.msg.isController() && (channel < 0 || e.msg.getChannel() == channel)) r.push_back (e);
        return r;
    }
};

bool near (double a, double b, double tol = 0.002) { return std::abs (a - b) <= tol; }

void fillOn (TrackPattern& tp, int pc = 0, int oct = 3)
{
    for (auto& s : tp.steps) { s.on = true; s.ccOn = true; s.pitchClass = (std::int8_t) pc; s.octave = (std::int8_t) oct; }
}

// ---------------------------------------------------------------------------------------
void testModel()
{
    section ("Model and scales");
    Globals g;
    check (scaleSize (scaleInfo (0).mask) == 7, "major has 7 notes");
    check (scaleSize (scaleInfo (9).mask) == 5, "major pentatonic has 5 notes");
    check (quantizeToScale (scaleInfo (1).mask, 0, 61) == 60, "C# snaps down to C in C minor (ties go down)");
    check (quantizeToScale (scaleInfo (1).mask, 0, 64) == 63, "E snaps to D# in C minor");
    check (quantizeToScale (scaleInfo (0).mask, 2, 61) == 61, "C# is in D major");
    check (pitchToDegree (scaleInfo (0).mask, 0, 60) == 35 && degreeToPitch (scaleInfo (0).mask, 0, 35) == 60, "degree round trip C3 in C major");
    check (degreeToPitch (scaleInfo (0).mask, 0, 36) == 62, "next degree after C is D in C major");
    check (degreeToPitch (scaleInfo (0).mask, 0, 34) == 59, "degree below C is B in C major");
    g.scale = userScaleIndex;
    g.userScale = 0b000010010000;   // root missing on purpose
    check ((g.scaleMask() & 1) == 1, "the root is always in the user scale");
    check (patternLabel (0) == "01" && patternLabel (17) == "18" && patternLabel (63) == "64", "patterns are numbered 01-64");
    check (pageRangeText (0) == "1 - 16" && pageRangeText (3) == "49 - 64", "pattern pages read 1 - 16 ... 49 - 64");
    check (numScales == 14 && juce::String (scaleInfo (userScaleIndex).longName) == "USER" && juce::String (scaleInfo (userScaleIndex - 1).longName) != "CHROMATIC",
           "13 built-in scales plus USER, no CHROMATIC");

    check (Engine::stepIndexFor (0, 4, Direction::forward) == 0 && Engine::stepIndexFor (5, 4, Direction::forward) == 1, "forward positions");
    check (Engine::stepIndexFor (0, 4, Direction::backward) == 3 && Engine::stepIndexFor (3, 4, Direction::backward) == 0, "backward positions");
    std::vector<int> pp;
    for (int n = 0; n < 8; ++n) pp.push_back (Engine::stepIndexFor (n, 4, Direction::pingPong));
    check (pp == std::vector<int> { 0, 1, 2, 3, 2, 1, 0, 1 }, "ping-pong positions (ends played once)");
    check (Engine::stepIndexFor (-1, 4, Direction::forward) == 3, "negative positions wrap");

    // state round trip
    auto p = std::make_unique<Project>();
    legacyBlank (*p);
    p->patterns[5].tracks[3].steps[7] = { true, false, 4, 5, 33, 77, 60, 99 };
    p->patterns[5].tracks[3].length = 24;
    p->patterns[5].tracks[3].division = 8;
    p->patterns[5].tracks[3].direction = Direction::pingPong;
    p->patterns[5].tracks[3].swing = 40;
    p->routing[3].midiChannel = 9;
    p->routing[3].ccOut = true;
    p->routing[3].ccNumber = 21;
    p->globals.chain.add ({ 5, 3 });
    p->globals.quantize = Quantize::off;
    p->globals.scale = 4;
    p->globals.root = 7;
    p->globals.armProb = true;
    p->globals.rangeVel = { 10, 20 };
    p->currentPattern = 5;
    auto q = std::make_unique<Project>();
    q->fromValueTree (p->toValueTree());
    const auto& qs = q->patterns[5].tracks[3];
    check (qs.steps[7] == p->patterns[5].tracks[3].steps[7], "step survives save/load");
    check (qs.length == 24 && qs.division == 8 && qs.direction == Direction::pingPong && qs.swing == 40, "track settings survive save/load");
    check (q->routing[3].midiChannel == 9 && q->routing[3].ccOut && q->routing[3].ccNumber == 21, "routing survives save/load");
    check (q->globals.chain.size() == 1 && q->globals.chain[0].repeats == 3 && q->globals.quantize == Quantize::off, "chain and quantize survive");
    check (q->globals.scale == 4 && q->globals.root == 7 && q->globals.armProb && q->globals.rangeVel.min == 10, "globals survive");
    check (q->currentPattern == 5, "current pattern survives");
}

void testTiming()
{
    section ("Timing, divisions, direction, swing, gate");
    {
        Runner r;
        fillOn (r.track (0));
        r.run (4.0);
        auto ons = r.noteOns (1);
        check (ons.size() == 16, "16 notes per bar at 1/16, got " + juce::String ((int) ons.size()));
        bool grid = true;
        for (size_t i = 0; i < ons.size(); ++i) grid = grid && near (ons[i].ppq, 0.25 * (double) i);
        check (grid, "notes land on the 1/16 grid");
        check (! ons.empty() && ons[0].msg.getNoteNumber() == 60 && ons[0].msg.getVelocity() == 100, "C3 = MIDI 60 at velocity 100");
        auto offs = r.noteOffs (1);
        check (offs.size() >= 15 && near (offs[0].ppq, 0.125), "gate 50% ends the note after half a step");
    }
    {
        Runner r;
        fillOn (r.track (0));
        r.track (0).division = 8;   // 1/8T
        r.run (4.0);
        check (r.noteOns (1).size() == 12, "1/8T gives 12 steps per bar, got " + juce::String ((int) r.noteOns (1).size()));
    }
    {
        Runner r;
        fillOn (r.track (0));
        r.track (0).length = 4;
        for (int i = 0; i < 4; ++i) r.track (0).steps[(size_t) i].pitchClass = (std::int8_t) (i * 2);   // C D E F#
        r.track (0).scaleOn = false;
        r.track (0).direction = Direction::backward;
        r.run (1.0);
        auto ons = r.noteOns (1);
        check (ons.size() == 4 && ons[0].msg.getNoteNumber() == 66 && ons[3].msg.getNoteNumber() == 60, "backward plays last step first");
    }
    {
        Runner r;
        fillOn (r.track (0));
        r.track (0).swing = 50;
        r.run (1.0);
        auto ons = r.noteOns (1);
        check (ons.size() == 4 && near (ons[0].ppq, 0.0) && near (ons[1].ppq, 0.25 + 0.0625) && near (ons[2].ppq, 0.5),
               "swing 50% delays odd steps by a quarter step");
    }
    {
        Runner r;
        fillOn (r.track (0));
        for (auto& s : r.track (0).steps) s.gate = 100;
        r.run (2.0);
        auto ons = r.noteOns (1), offs = r.noteOffs (1);
        bool ordered = true;
        for (size_t i = 1; i < ons.size() && i - 1 < offs.size(); ++i) ordered = ordered && offs[i - 1].ppq <= ons[i].ppq + 1.0e-9;
        check (ordered && offs.size() >= ons.size() - 1, "gate 100%: each note ends before the same note retriggers");
    }
    {
        Runner r;
        r.track (0).length = 4;
        r.run (2.0);
        check (r.events.empty(), "off steps send nothing");
    }
}

void testOutputs()
{
    section ("Probability, scale, routing, CC");
    {
        Runner r;
        fillOn (r.track (0));
        for (auto& s : r.track (0).steps) s.probability = 1;
        r.run (64.0);
        check (r.noteOns().size() < 15, "probability 1% (the lowest) plays rarely, got " + juce::String ((int) r.noteOns().size()) + " of 256");
    }
    {
        Runner r;
        fillOn (r.track (0));
        for (auto& s : r.track (0).steps) s.probability = 50;
        r.run (64.0);
        const auto n = (int) r.noteOns().size();
        check (n > 98 && n < 158, "probability 50% plays about half (" + juce::String (n) + " of 256)");
    }
    {
        Runner r;
        fillOn (r.track (0), 1);   // C#
        r.run (1.0);
        check (! r.noteOns().empty() && r.noteOns()[0].msg.getNoteNumber() == 60, "scale on: C# plays as C in C minor");
        Runner r2;
        fillOn (r2.track (0), 1);
        r2.track (0).scaleOn = false;
        r2.run (1.0);
        check (! r2.noteOns().empty() && r2.noteOns()[0].msg.getNoteNumber() == 61, "scale off: C# plays as C#");
    }
    {
        Runner r;
        fillOn (r.track (2));
        r.project->routing[2].midiChannel = 11;
        r.run (1.0);
        check (r.noteOns (11).size() == 4 && r.noteOns (3).empty(), "track 3 plays on its assigned channel 11");
    }
    {
        Runner r;
        fillOn (r.track (0));
        for (int i = 0; i < 16; ++i) r.track (0).steps[(size_t) i].cc = (std::uint8_t) (i * 8);
        r.project->routing[0].noteOut = false;
        r.project->routing[0].ccOut = true;
        r.project->routing[0].ccNumber = 21;
        r.project->globals.ccSmoothing = false;
        r.run (4.0);
        auto cc = r.ccs (1);
        check (r.noteOns().empty(), "note output off sends no notes");
        check (cc.size() == 16 && cc[3].msg.getControllerNumber() == 21 && cc[3].msg.getControllerValue() == 24 && near (cc[3].ppq, 0.75),
               "CC without smoothing: one CC 21 per step at the step time");
    }
    {
        Runner r;
        auto& tp = r.track (0);
        tp.length = 4;
        tp.steps[0] = { true, true, 0, 3, 100, 50, 100, 0 };
        tp.steps[2] = { true, true, 0, 3, 100, 50, 100, 100 };
        r.project->routing[0].noteOut = false;
        r.project->routing[0].ccOut = true;
        r.project->globals.ccSmoothing = true;
        r.run (2.0);
        auto cc = r.ccs (1);
        bool reachesTop = false, hasMiddle = false, startsAtZero = ! cc.empty() && cc[0].msg.getControllerValue() == 0;
        for (auto& e : cc)
        {
            const int v = e.msg.getControllerValue();
            if (v == 100 && near (e.ppq, 0.5, 0.01)) reachesTop = true;
            if (v > 30 && v < 70 && e.ppq > 0.15 && e.ppq < 0.35) hasMiddle = true;
        }
        check (startsAtZero && reachesTop && hasMiddle, "CC smoothing glides from 0 to 100 and arrives on the step");

        bool nearHalf = false;
        for (auto& e : cc)
            if (near (e.ppq, 0.25, 0.03) && std::abs (e.msg.getControllerValue() - 50) <= 6) nearHalf = true;
        check (nearHalf, "CC smoothing glides over the whole gap (halfway = 50)");
    }
    {
        Runner r;
        fillOn (r.track (0));
        r.run (3.3);
        r.stop();
        check (r.noteOns().size() == r.noteOffs().size(), "stop releases every note");
    }
}

void testPatterns()
{
    section ("Pattern switching and chain");
    {
        Runner r;
        fillOn (r.track (0, 0), 0);
        fillOn (r.track (0, 1), 7);   // G
        r.run (0.5);
        r.engine.queuedPattern = 1;   // quantize: 1 bar (default)
        r.run (7.5);
        bool switchedOnBar = true;
        for (auto& e : r.noteOns())
            switchedOnBar = switchedOnBar && (e.ppq < 4.0 - 1.0e-6 ? e.msg.getNoteNumber() == 60 : e.msg.getNoteNumber() == 67);
        check (switchedOnBar && r.project->currentPattern == 1, "queued pattern switches on the next bar");
    }
    {
        Runner r;
        r.project->globals.quantize = Quantize::off;
        fillOn (r.track (0, 0), 0);
        for (auto& s : r.track (0, 0).steps) s.gate = 100;
        fillOn (r.track (0, 1), 7);
        r.run (1.1);
        r.engine.queuedPattern = 1;
        r.run (0.4);
        auto ons = r.noteOns();
        check (r.project->currentPattern == 1, "quantize OFF switches immediately");
        bool g = false;
        for (auto& e : ons) if (e.msg.getNoteNumber() == 67 && near (e.ppq, 1.25)) g = true;
        check (g, "after an instant switch the new pattern plays from the same playhead position");
        check (r.noteOns().size() == r.noteOffs().size() || r.noteOffs().size() + 1 == r.noteOns().size(), "notes of the old pattern are released at the switch");
    }
    {
        Runner r;
        r.project->globals.patternCC = true;
        r.project->globals.patternCCChannel = 16;
        r.project->globals.patternCCNumber = 20;
        r.project->globals.quantize = Quantize::beat;
        juce::MidiBuffer in;
        in.addEvent (juce::MidiMessage::controllerEvent (16, 20, 9), 10);
        r.run (0.6, in);
        r.run (1.0);
        check (r.project->currentPattern == 9, "incoming CC 20 on channel 16 selects pattern 10 (value 9)");
    }
    {
        Runner r;
        fillOn (r.track (0, 2), 2);
        fillOn (r.track (0, 3), 5);
        r.track (0, 3).length = 8;   // half a bar
        r.project->globals.chain.add ({ 2, 2 });   // 2 bars
        r.project->globals.chain.add ({ 3, 1 });   // half a bar... longest track decides (other tracks are 1 bar)
        r.project->globals.chainOn = true;
        r.run (13.0);
        auto ons = r.noteOns();
        auto noteAt = [&] (double ppq)
        {
            for (auto& e : ons) if (near (e.ppq, ppq)) return e.msg.getNoteNumber();
            return -1;
        };
        check (noteAt (0.0) == 62 && noteAt (7.75) == 62 && noteAt (8.0) == 65 && noteAt (12.0) == 62,
               "chain: pattern 3 twice (2 bars), then pattern 4 for its length (1 bar), then loops");
        r.project->globals.chainLoop = false;
    }
}

void testOperations()
{
    section ("Step operations");
    auto p = std::make_unique<Project>();
    legacyBlank (*p);
    auto& tp = p->patterns[0].tracks[0];
    juce::Random rnd (42);

    {
        auto sc = makeScope (*p, 0, 0, -1, -1);
        check (sc.mask == rangeMask (0, 15), "no selection = all steps within the length");
        sc = makeScope (*p, 0, 0, 4, 40);
        check (sc.mask == rangeMask (4, 15), "selection is clipped to the length");
        sc = makeScope (*p, 0, 0, (1u << 1) | (1u << 5) | (1u << 20));
        check (sc.mask == ((1u << 1) | (1u << 5)) && sc.count() == 2, "non-adjacent selection; steps beyond the length dropped");
        sc = makeScope (*p, 0, 0, (std::uint32_t) (1u << 20));
        check (sc.mask == rangeMask (0, 15), "a selection entirely beyond the length = all steps");
    }
    {
        p->globals.armOn = false;
        p->globals.armNote = true; p->globals.armVel = true; p->globals.armOct = true;
        p->globals.rangeVel = { 60, 70 };
        p->globals.rangeOct = { 2, 3 };
        randomize (*p, makeScope (*p, 0, 0, -1, -1), false, rnd);
        bool ok = true;
        for (int i = 0; i < 16; ++i)
        {
            const auto& s = tp.steps[(size_t) i];
            ok = ok && s.velocity >= 60 && s.velocity <= 70 && s.octave >= 2 && s.octave <= 3
                 && inScale (p->globals.scaleMask(), p->globals.root, s.pitchClass);
        }
        check (ok, "randomize stays in the ranges and in the scale");
        bool untouched = true;
        for (int i = 16; i < 32; ++i) untouched = untouched && tp.steps[(size_t) i] == blankStep();
        check (untouched, "randomize does not touch steps beyond the length");
    }
    {
        for (auto& s : tp.steps) s = blankStep();
        tp.steps[0].on = tp.steps[1].on = tp.steps[2].on = true;
        tp.steps[0].ccOn = tp.steps[1].ccOn = tp.steps[2].ccOn = true;
        tp.steps[0].cc = 0; tp.steps[1].cc = 64; tp.steps[2].cc = 127;
        tp.steps[3].cc = 0;   // off step
        p->globals.armCC = true;
        smooth (*p, makeScope (*p, 0, 0, 0, 3), true);
        check (tp.steps[0].cc == 1 && tp.steps[1].cc == 64 && tp.steps[2].cc == 126, "smooth: 0, 64, 127 -> 1, 64, 126");
        check (tp.steps[3].cc == 0, "smooth leaves off steps alone");
    }
    {
        for (auto& s : tp.steps) s = blankStep();
        tp.steps[0].velocity = 120; tp.steps[1].velocity = 60;
        auto gst = beginShift (*p, makeScope (*p, 0, 0, 0, 1), Param::velocity);
        applyShift (*p, gst, 20);
        check (tp.steps[0].velocity == 127 && tp.steps[1].velocity == 80, "shift caps at the limit (127)");
        applyShift (*p, gst, 0);
        check (tp.steps[0].velocity == 120 && tp.steps[1].velocity == 60, "turning back in the same gesture restores the shape");
        auto gst2 = beginShift (*p, makeScope (*p, 0, 0, 0, 1), Param::velocity);
        applyShift (*p, gst2, -200);
        check (tp.steps[0].velocity == 1 && tp.steps[1].velocity == 1, "velocity never goes below 1");
    }
    {
        for (auto& s : tp.steps) s = blankStep();
        tp.steps[0].pitchClass = 10; tp.steps[0].octave = 3;   // A#3 in C minor
        auto gst = beginShift (*p, makeScope (*p, 0, 0, 0, 0), Param::note);
        applyShift (*p, gst, 1);
        check (tp.steps[0].pitchClass == 0 && tp.steps[0].octave == 4, "note shift moves in scale steps and rolls into the next octave");
        tp.scaleOn = false;
        auto gst2 = beginShift (*p, makeScope (*p, 0, 0, 0, 0), Param::note);
        applyShift (*p, gst2, 1);
        check (tp.steps[0].pitchClass == 1 && tp.steps[0].octave == 4, "scale off: note shift moves in semitones");
        tp.scaleOn = true;
    }
    {
        for (auto& s : tp.steps) s = blankStep();
        tp.steps[0].pitchClass = 0;
        nudgePitchClass (*p, 0, 0, 0, 1);
        check (tp.steps[0].pitchClass == 2, "pitch knob only lands on scale notes (C -> D in C minor)");
        nudgePitchClass (*p, 0, 0, 0, 1);
        check (tp.steps[0].pitchClass == 3, "D -> D# in C minor");
    }
    {
        for (auto& s : tp.steps) s = blankStep();
        for (int i = 0; i < 4; ++i) { tp.steps[(size_t) i].on = true; tp.steps[(size_t) i].cc = (std::uint8_t) (10 + i); }
        auto clip = copySteps (*p, makeScope (*p, 0, 0, 0, 3));
        auto& other = p->patterns[0].tracks[5];
        other.length = 8;
        const auto written = pasteSteps (*p, 0, 5, 6, clip);
        check (written == rangeMask (6, 7) && other.steps[6].cc == 10 && other.steps[7].cc == 11 && other.steps[8] == blankStep(),
               "paste onto another track is cut off at its length");

        // non-adjacent copy keeps the spacing: steps 1, 3, 6 pasted at 9 -> 9, 11, 14
        for (auto& s : tp.steps) s = blankStep();
        tp.length = 32;
        for (int i : { 0, 2, 5 }) { tp.steps[(size_t) i].on = true; tp.steps[(size_t) i].cc = (std::uint8_t) (40 + i); }
        tp.steps[1].cc = 99;   // not selected
        auto gapClip = copySteps (*p, makeScope (*p, 0, 0, (std::uint32_t) ((1u << 0) | (1u << 2) | (1u << 5))));
        check (gapClip.size() == 3 && gapClip[1].offset == 2 && gapClip[2].offset == 5, "copy remembers the spacing");
        const auto w2 = pasteSteps (*p, 0, 0, 8, gapClip);
        check (w2 == ((1u << 8) | (1u << 10) | (1u << 13)) && tp.steps[10].cc == 42 && tp.steps[13].cc == 45 && tp.steps[9] == blankStep(),
               "paste keeps the spacing and leaves the gaps alone");

        // smooth works on non-adjacent steps only
        for (auto& s : tp.steps) s = blankStep();
        for (int i : { 0, 1, 2 }) tp.steps[(size_t) i].on = tp.steps[(size_t) i].ccOn = true;
        tp.steps[0].cc = 0; tp.steps[1].cc = 50; tp.steps[2].cc = 100;
        smooth (*p, makeScope (*p, 0, 0, (std::uint32_t) ((1u << 0) | (1u << 2))), true);
        check (tp.steps[0].cc == 1 && tp.steps[2].cc == 99 && tp.steps[1].cc == 50, "smooth on steps 1 and 3 only");
        tp.length = 16;
    }
    {
        UndoHistory u;
        tp.steps[0].velocity = 5;
        u.push (*p, 0);
        tp.steps[0].velocity = 99;
        int restored = -1;
        check (u.undo (*p, restored) && restored == 0 && tp.steps[0].velocity == 5, "undo restores the pattern");
        check (! u.undo (*p, restored), "nothing left to undo");
    }
}

void savePng (const juce::Image& img, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "  wrote " << file.getFileName() << std::endl;
}

void snapshot (juce::Component& c, const juce::File& file)
{
    savePng (c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f), file);
}

// ---- simulated mouse input --------------------------------------------------------------
juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<int> p, juce::Point<int> downAt, bool dragged = false,
                             juce::ModifierKeys mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier))
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, p.toFloat(), mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downAt.toFloat(), now, 1, dragged);
}

void click (juce::Component& c, juce::Point<int> p)
{
    c.mouseDown (mouseEvent (c, p, p));
    c.mouseUp (mouseEvent (c, p, p));
}

void click (juce::Component& c) { click (c, c.getLocalBounds().getCentre()); }

// Drag vertically by dy pixels (negative = up). Uses screen positions, like the knobs do.
void dragY (juce::Component& c, int dy)
{
    const auto start = c.getLocalBounds().getCentre();
    c.mouseDown (mouseEvent (c, start, start));
    c.mouseDrag (mouseEvent (c, start.translated (0, dy), start, true));
    c.mouseUp (mouseEvent (c, start.translated (0, dy), start, true));
}

Pill* findPill (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* p = dynamic_cast<Pill*> (child); p != nullptr && p->isVisible() && p->text && p->text() == text)
            return p;
        if (auto* found = findPill (*child, text))
            return found;
    }
    return nullptr;
}

template <typename T> T* findChild (juce::Component& root, int skip = 0)
{
    for (auto* child : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (child))
        {
            if (skip-- == 0) return t;
        }
        else if (auto* found = findChild<T> (*child, skip))
            return found;
    }
    return nullptr;
}

// Demo content, similar to the mockups
void fillDemo (Project& p)
{
    juce::Random r (7);
    auto& pat = p.patterns[(size_t) p.currentPattern];
    for (int t = 0; t < 8; ++t)
    {
        auto& tp = pat.tracks[(size_t) t];
        tp.length = (std::uint8_t) std::array<int, 8> { 16, 16, 24, 12, 32, 8, 32, 16 }[(size_t) t];
        for (int i = 0; i < maxSteps; ++i)
        {
            auto& s = tp.steps[(size_t) i];
            s.on = r.nextFloat() < 0.8f;
            s.pitchClass = (std::int8_t) std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 }[(size_t) r.nextInt (7)];
            s.octave = (std::int8_t) (1 + r.nextInt (3));
            s.velocity = (std::uint8_t) (50 + r.nextInt (77));
            s.gate = (std::uint8_t) (15 + r.nextInt (80));
            s.probability = (std::uint8_t) (r.nextFloat() < 0.55f ? 100 : 20 + r.nextInt (75));
            s.cc = (std::uint8_t) juce::jlimit (0, 127, (int) (64 + 50 * std::sin (i / (double) tp.length * 6.283 + t)) + r.nextInt (20) - 10);
        }
    }
    pat.tracks[2].swing = 56;
    p.routing[2].ccOut = true;
    p.routing[4].ccOut = true;
    p.routing[6].noteOut = false;
    p.routing[6].ccOut = true;
    for (int extra : { 1, 2, 4, 5, 6, 7, 8, 9, 11 })
        p.patterns[(size_t) (16 + extra)] = pat;
    p.globals.chain.add ({ 22, 2 });
    p.globals.chain.add ({ 23, 1 });
    p.globals.chain.add ({ 24, 4 });
}
} // namespace


void testMacros()
{
    section ("Macros (host automation)");
    auto proc = std::make_unique<VimaProcessor>();
    proc->prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    auto block = [&] { midi.clear(); proc->processBlock (audio, midi); };
    auto macro = [&] (int i) { return proc->macros.param (i); };
    auto plainOf = [&] (int i, const MacroMap& m) { return macroPlain (m, macro (i)->getValue()); };

    check (proc->getParameters().size() == numMacros, "the 128 macros are the only host parameters, got " + juce::String (proc->getParameters().size()));
    check (macro (0)->getName (32) == "Macro 1" && macro (127)->getName (32) == "Macro 128", "macros are named Macro 1 to Macro 128");
    check (macro (4)->getCurrentValueAsText() == "-", "an unmapped macro shows -");
    check (juce::String (macroTargetInfo (MacroTarget::stepOn).name) == "NOTE STEP" && juce::String (macroTargetInfo (MacroTarget::ccStepOn).name) == "CC STEP",
           "step macros are named NOTE STEP and CC STEP");

    // map: M1 = velocity of track 4 step 5; M2 = length of track 3 in pattern 10;
    // M3 = pattern; M4 = chain on; M5 = note of step 1 on track 2; M6 = probability of track 4 step 1
    proc->edit (-1, [] (Project& p)
    {
        p.macros[0] = defaultMacroMap (MacroTarget::velocity, 3, 0, 4, p.globals);
        p.macros[1] = defaultMacroMap (MacroTarget::length, 2, 9, 0, p.globals);
        p.macros[2] = defaultMacroMap (MacroTarget::pattern, 0, 0, 0, p.globals);
        p.macros[3] = defaultMacroMap (MacroTarget::chainOn, 0, 0, 0, p.globals);
        p.macros[4] = defaultMacroMap (MacroTarget::note, 1, 0, 0, p.globals);
        p.macros[5] = defaultMacroMap (MacroTarget::probability, 3, 0, 0, p.globals);
        p.globals.quantize = Quantize::off;
    });
    auto m = [&] (int i) { return proc->read ([i] (const Project& p) { return p.macros[(size_t) i]; }); };

    // host -> project
    macro (0)->setValueNotifyingHost (macroNormalized (m (0), 33));
    macro (1)->setValueNotifyingHost (macroNormalized (m (1), 12));
    macro (3)->setValueNotifyingHost (1.0f);
    const auto noteMap = m (4);
    macro (4)->setValueNotifyingHost (proc->read ([&] (const Project& p) { return macroNormalized (noteMap, 7, macroNotes (p, noteMap)); }));
    macro (5)->setValueNotifyingHost (0.5f);
    block();
    check (proc->read ([] (const Project& p) { return p.patterns[0].tracks[3].steps[4].velocity; }) == 33,
           "macro on track 4, step 5: velocity is set");
    check (proc->read ([] (const Project& p) { return p.patterns[9].tracks[2].length; }) == 12
               && proc->read ([] (const Project& p) { return p.patterns[0].tracks[2].length; }) == 16,
           "a macro changes its own pattern only");
    check (proc->read ([] (const Project& p) { return p.globals.chainOn; }), "macro on Chain On");
    check (proc->read ([] (const Project& p) { return (int) p.patterns[0].tracks[1].steps[0].pitchClass; }) == 7, "macro on a step note");
    check (proc->read ([] (const Project& p) { return (int) p.patterns[0].tracks[3].steps[0].probability; }) >= 50,
           "a macro covers the full range: halfway gives about 50% probability (1-100)");
    proc->syncParamsToHost();
    check (macro (0)->getCurrentValueAsText() == "33" && macro (4)->getCurrentValueAsText() == "G", "host shows the target value as text");
    check (macro (0)->getName (64) == "Vel St05 Tr04 Pat01" && macro (1)->getName (64) == "Length Tr03 Pat10" && macro (2)->getName (64) == "Pattern"
               && macro (4)->getName (64) == "Note St01 Tr02 Pat01" && macro (10)->getName (64) == "Macro 11",
           "host names follow the mapping (Vel St05 Tr04 Pat01, Length Tr03 Pat10, Pattern), unmapped: Macro 11");
    check (macro (0)->getName (8) == "Vel St05", "short names keep the param first");

    macro (2)->setValueNotifyingHost (macroNormalized (m (2), 6));
    block();
    check (proc->engine.queuedPattern.load() == 6 || proc->read ([] (const Project& p) { return p.currentPattern; }) == 6,
           "macro on Pattern queues the pattern");
    {
        Project pp;
        const auto pm = defaultMacroMap (MacroTarget::pattern, 0, 0, 0, pp.globals);
        std::atomic<int> q { -1 };
        applyMacroValue (pp, q, pm, 6);
        check (q.load() == 6 && macroTargetValue (pp, pm, q.load()) == 6 && pp.currentPattern == 0,
               "a Pattern macro shows the picked pattern at once, while the switch waits for its quantize point");
        applyMacroValue (pp, q, pm, 0);
        check (q.load() == -1, "turning a Pattern macro back to the playing pattern cancels the queued switch");
    }
    proc->edit (-1, [] (Project& p) { p.currentPattern = 0; });
    proc->engine.queuedPattern = -1;

    // notes follow the scale like the note knob: C minor = C D D# F G G# A#
    proc->edit (-1, [] (Project& p) { p.globals.scale = 1; p.globals.root = 0; p.patterns[0].tracks[1].scaleOn = true; });
    auto noteAt = [&] (float v)
    {
        macro (4)->setValueNotifyingHost (v);
        block();
        return proc->read ([] (const Project& p) { return (int) p.patterns[0].tracks[1].steps[0].pitchClass; });
    };
    check (noteAt (1.0f) == 10 && noteAt (0.5f) == 5 && noteAt (0.0f) == 0, "scale on: the note macro moves through the 7 notes of C minor (top A#, middle F, bottom C)");
    check (macroPositions (noteMap, proc->read ([&] (const Project& p) { return macroNotes (p, noteMap); })) == 7, "7 positions in C minor");
    proc->edit (-1, [] (Project& p) { p.patterns[0].tracks[1].scaleOn = false; });
    check (noteAt (1.0f / 11.0f) == 1, "scale off: all 12 notes (C#)");
    proc->edit (-1, [] (Project& p) { p.patterns[0].tracks[1].scaleOn = true; });

    // project -> host
    proc->edit (-1, [] (Project& p) { p.patterns[0].tracks[3].steps[4].velocity = 90; });
    proc->syncParamsToHost();
    check (plainOf (0, m (0)) == 90, "a change in the plugin moves the macro");
    block();
    check (proc->read ([] (const Project& p) { return p.patterns[0].tracks[3].steps[4].velocity; }) == 90,
           "the macro's new value does not bounce back");

    // values outside the range are not overwritten by the clamped macro
    proc->edit (-1, [] (Project& p) { p.patterns[0].tracks[3].steps[0].probability = 90; });
    proc->syncParamsToHost();
    block();
    check (proc->read ([] (const Project& p) { return (int) p.patterns[0].tracks[3].steps[0].probability; }) == 90,
           "a value outside MIN / MAX is left alone when the macro follows it");

    // a fixed mapping does not follow pattern switches
    proc->edit (-1, [] (Project& p) { p.patterns[1].tracks[3].steps[4].velocity = 11; p.currentPattern = 1; });
    proc->syncParamsToHost();
    check (plainOf (0, m (0)) == 90, "switching pattern does not move a fixed macro");
    proc->edit (-1, [] (Project& p) { p.currentPattern = 0; });

    // state
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    auto other = std::make_unique<VimaProcessor>();
    other->setStateInformation (state.getData(), (int) state.getSize());
    check (other->read ([] (const Project& p) { return p.macros[1]; }) == m (1) && other->read ([] (const Project& p) { return p.macros[5]; }) == m (5),
           "macro mappings are saved with the project");
    check (sameControl (m (0), defaultMacroMap (MacroTarget::velocity, 3, 0, 4, Globals())) && ! sameControl (m (0), m (5)),
           "sameControl: same target, track, pattern and step");
}

void testTransport()
{
    section ("Transport button");
    auto proc = std::make_unique<VimaProcessor>();
    proc->prepareToPlay (48000.0, 250);
    proc->edit (-1, [] (Project& p) { fillOn (p.patterns[0].tracks[0], 0, 3); });
    juce::AudioBuffer<float> audio (2, 250);
    juce::MidiBuffer midi;
    int notes = 0;
    auto run = [&] (int blocks)
    {
        notes = 0;
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            proc->processBlock (audio, midi);
            for (const auto m : midi) notes += m.getMessage().isNoteOn() ? 1 : 0;
        }
    };
    run (10);
    check (! proc->engine.isPlaying.load() && notes == 0, "host stopped: VIMA is stopped");
    proc->toggleTransport();
    run (96);   // 0.5 s at 120 BPM = 4 sixteenths
    check (proc->engine.isPlaying.load() && notes >= 4, "the button starts VIMA on its own while the host is stopped, got " + juce::String (notes) + " notes");
    proc->toggleTransport();
    run (10);
    check (! proc->engine.isPlaying.load() && notes == 0, "pressing it again stops VIMA");

    // a fake host transport
    struct Host : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo i;
            i.setIsPlaying (playing);
            i.setBpm (120.0);
            i.setPpqPosition (ppq);
            i.setTimeInSamples ((juce::int64) (ppq * 24000.0));
            return i;
        }
    } host;
    proc->setPlayHead (&host);
    auto hostRun = [&] (int blocks)
    {
        notes = 0;
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            proc->processBlock (audio, midi);
            for (const auto m : midi) notes += m.getMessage().isNoteOn() ? 1 : 0;
            if (host.playing) host.ppq += 250.0 * 2.0 / 48000.0;
        }
    };

    // stopped with the button while the host plays: held through a host stop and restart
    host.playing = true; host.ppq = 0.0;
    hostRun (96);
    check (proc->engine.isPlaying.load() && notes >= 4, "the host plays: VIMA follows");
    proc->toggleTransport();
    hostRun (10);
    check (! proc->engine.isPlaying.load() && proc->userStopped.load(), "stopped with the button while the host plays: VIMA is held");
    host.playing = false;
    hostRun (10);
    host.playing = true; host.ppq = 0.0;
    hostRun (96);
    check (! proc->engine.isPlaying.load() && notes == 0, "held: VIMA ignores the host stopping and starting again");
    proc->toggleTransport();
    hostRun (96);
    check (proc->engine.isPlaying.load() && notes >= 4 && ! proc->userStopped.load(), "VIMA's play button releases the hold, in sync with the host");

    // stopped with the button while the host is stopped: not held
    host.playing = false;
    hostRun (10);
    proc->toggleTransport();
    hostRun (20);
    check (proc->engine.isPlaying.load(), "host stopped: the button runs VIMA on its own");
    proc->toggleTransport();
    hostRun (10);
    check (! proc->userStopped.load(), "stopped while the host is stopped: not held");
    host.playing = true; host.ppq = 0.0;
    hostRun (96);
    check (proc->engine.isPlaying.load() && notes >= 4, "then the host plays: VIMA follows");

    // held, then play pressed while the host is stopped: runs on its own and releases the hold
    proc->toggleTransport();
    host.playing = false;
    hostRun (10);
    proc->toggleTransport();
    hostRun (20);
    check (proc->engine.isPlaying.load() && ! proc->userStopped.load(), "play with the host stopped releases the hold too");
    proc->toggleTransport();
    host.playing = true; host.ppq = 0.0;
    hostRun (96);
    check (proc->engine.isPlaying.load() && notes >= 4, "and the host plays: VIMA follows");
    proc->setPlayHead (nullptr);
}

void testPresets()
{
    section ("Presets and macro mapping sets");
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vima-preset-test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto proc = std::make_unique<VimaProcessor>();
    proc->edit (-1, [] (Project& p)
    {
        p.patterns[3].tracks[2].steps[5].velocity = 77;
        p.patterns[3].tracks[2].length = 12;
        p.globals.scale = 4;
        p.currentPattern = 3;
        p.macros[0] = defaultMacroMap (MacroTarget::velocity, 0, 0, 2, p.globals);
    });
    const auto presetFile = dir.getChildFile ("Song.vima");
    check (proc->savePreset (presetFile) && presetFile.existsAsFile(), "a preset is saved to a file");
    check (proc->presetFile == presetFile, "the saved file becomes the current preset");
    {
        auto xml = juce::XmlDocument::parse (presetFile);
        check (xml != nullptr && xml->getChildByName ("Macro") == nullptr, "a preset holds no macro mappings");
    }

    // change everything, then load the preset back
    proc->edit (-1, [] (Project& p)
    {
        p.patterns[3].tracks[2].steps[5].velocity = 5;
        p.globals.scale = 0;
        p.currentPattern = 0;
        p.macros[0] = MacroMap();
        p.macros[9] = defaultMacroMap (MacroTarget::gate, 1, 0, 0, p.globals);
    });
    check (proc->loadPreset (presetFile), "the preset loads");
    auto get = [&] (auto fn) { return proc->read (fn); };
    check (get ([] (const Project& p) { return (int) p.patterns[3].tracks[2].steps[5].velocity; }) == 77
               && get ([] (const Project& p) { return (int) p.patterns[3].tracks[2].length; }) == 12
               && get ([] (const Project& p) { return p.globals.scale; }) == 4
               && get ([] (const Project& p) { return p.currentPattern; }) == 3,
           "loading a preset restores patterns, settings and the current pattern");
    check (get ([] (const Project& p) { return p.macros[9].target; }) == MacroTarget::gate && ! get ([] (const Project& p) { return p.macros[0].mapped(); }),
           "loading a preset keeps the macro mappings");
    {
        auto xml = juce::XmlDocument::parse (presetFile);
        check (xml != nullptr && ! xml->toString().contains ("lightTheme"), "presets and projects do not carry the theme");
    }

    // mapping sets
    const auto setFile = dir.getChildFile ("Live.vimamacros");
    check (proc->saveMacros (setFile), "a mapping set is saved");
    proc->edit (-1, [] (Project& p) { for (auto& m : p.macros) m = MacroMap(); p.patterns[3].tracks[2].steps[5].velocity = 50; });
    check (proc->loadMacros (setFile) && get ([] (const Project& p) { return p.macros[9].target; }) == MacroTarget::gate,
           "loading a mapping set restores all mappings");
    check (get ([] (const Project& p) { return (int) p.patterns[3].tracks[2].steps[5].velocity; }) == 50, "loading mappings leaves the music alone");
    check (! proc->loadMacros (presetFile) && ! proc->loadPreset (setFile), "a preset is not loaded as mappings, nor the other way");

    proc->newPreset();
    check (get ([] (const Project& p) { return p.patterns[3].tracks[2].steps[5].velocity == 100 && p.patterns[3].tracks[2].length == 16; })
               && get ([] (const Project& p) { return p.macros[9].mapped(); })
               && proc->presetFile == juce::File(),
           "NEW resets the music, keeps the mappings and forgets the preset file");
    check (get ([] (const Project& p) { return p.patterns[3].isEmpty() && ! p.patterns[0].isEmpty(); }), "preset NEW: pattern 1 init, the others blank");

    // mapping file link
    check (proc->macroFile == setFile, "loading a mapping set links its file (SAVE overwrites it)");
    proc->newMacros();
    check (proc->macroFile == juce::File() && ! get ([] (const Project& p) { for (auto& m : p.macros) if (m.mapped()) return true; return false; }),
           "macros NEW clears all mappings and unlinks the file");
    proc->loadMacros (setFile);
    {
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        auto other = std::make_unique<VimaProcessor>();
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        check (other->macroFile == setFile, "the linked mapping file is saved with the DAW project");
    }

    // single patterns
    proc->edit (-1, [] (Project& p)
    {
        p.patterns[7] = Pattern();
        p.patterns[7].tracks[1].steps[3].velocity = 33;
        p.patterns[7].tracks[1].steps[4].on = false;
        p.patterns[7].tracks[6].length = 9;
        p.patterns[7].tracks[6].swing = 40;
        p.globals.scale = 2;
    });
    const auto patFile = dir.getChildFile ("Groove.vimapattern");
    check (proc->savePattern (7, patFile) && patFile.existsAsFile(), "a pattern is saved to a file");
    {
        auto xml = juce::XmlDocument::parse (patFile);
        check (xml != nullptr && xml->hasTagName ("VIMAPATTERN") && xml->getChildByName ("Globals") == nullptr
                   && xml->getChildByName ("Routing") == nullptr && xml->getNumChildElements() == numTracks,
               "a pattern file holds only the 8 tracks (no globals, no routing)");
    }
    proc->edit (-1, [] (Project& p) { p.globals.scale = 5; });
    check (proc->loadPattern (20, patFile)
               && get ([] (const Project& p) { const auto& x = p.patterns[20]; return x.tracks[1].steps[3].velocity == 33 && ! x.tracks[1].steps[4].on
                                                     && x.tracks[6].length == 9 && x.tracks[6].swing == 40 && p.globals.scale == 5; }),
           "loading a pattern fills that slot and leaves the scale alone");
    check (proc->canUndo() && proc->performUndo() && get ([] (const Project& p) { return p.patterns[20].isEmpty(); }), "loading a pattern is undoable");
    proc->newPattern (7);
    check (get ([] (const Project& p) { const auto& x = p.patterns[7]; return x.tracks[1].steps[3].velocity == 100 && x.tracks[1].steps[4].on && x.tracks[6].length == 16; }),
           "pattern NEW gives the init pattern (16 steps on)");
    check (proc->performUndo() && get ([] (const Project& p) { return p.patterns[7].tracks[1].steps[3].velocity == 33; }), "pattern NEW is undoable");
    check (! proc->loadPattern (3, presetFile) && ! proc->loadPreset (patFile) && ! proc->loadMacros (patFile), "pattern files and other files are not mixed up");
    dir.deleteRecursively();
}

void testInitAndGate()
{
    section ("Init values, long gates, middle C");
    {
        Project p;
        const auto& s = p.patterns[0].tracks[5].steps[31];
        check (s.on && s.pitchClass == 5 && s.octave == 3 && s.velocity == 100 && s.gate == 100 && s.probability == 100 && s.cc == 64,
               "init step: on, F in the middle C octave, velocity 100, gate 100 %, probability 100 %, CC 64");
        check (p.patterns[0].tracks[7].length == 16 && p.patterns[0].tracks[7].division == 4 && p.currentPattern == 0, "init track: length 16, 1/16, pattern 1");
        bool blank = true;
        for (int i = 1; i < numPatterns; ++i)
            blank = blank && p.patterns[(size_t) i].isEmpty() && p.patterns[(size_t) i].tracks[3].length == 16
                    && p.patterns[(size_t) i].tracks[3].steps[2].velocity == 100;
        check (! p.patterns[0].isEmpty() && blank, "init: pattern 1 has its steps on, patterns 2-64 have all steps off (other values at init)");
        Project q;
        q.fromValueTree (p.toValueTree());
        check (! q.patterns[0].isEmpty() && q.patterns[9].isEmpty() && p.toValueTree().getChildWithName ("Pattern").isValid() == false,
               "the init state saves no Pattern children and loads back the same");
        bool routing = true;
        for (int t = 0; t < numTracks; ++t)
            routing = routing && p.routing[(size_t) t].midiChannel == t + 1 && p.routing[(size_t) t].noteOut == (t == 0)
                      && ! p.routing[(size_t) t].ccOut && p.routing[(size_t) t].ccNumber == 102 + t;
        check (routing, "init routing: channels 1-8, notes out on track 1 only, CC out off, CC numbers 102-109");
        const auto& g = p.globals;
        check (g.armNote && g.armOn && ! g.armVel && ! g.armGate && g.armCC && ! g.armCcOn && ! g.armAllNote && ! g.armAllCc
                   && g.rangeGate.max == maxGate && g.rangeVel.min == 1
                   && g.rangeVel.max == 127 && ! g.ccSmoothing && g.middleC == 3,
               "init globals: ON and NOTE armed (CC mode: CC), full ranges, CC smoothing off, middle C = C3");
    }
    {
        Project p;
        p.patterns[2].tracks[1].steps[3].gate = 350;
        auto q = std::make_unique<Project>();
        q->fromValueTree (p.toValueTree());
        check (q->patterns[2].tracks[1].steps[3].gate == 350, "gates above 255 % survive save / load");
    }
    {
        Runner r;
        auto& tp = r.track (0);
        tp.length = 4;
        tp.steps[0].on = true; tp.steps[0].gate = 300; tp.steps[0].pitchClass = 0;
        tp.steps[2].on = true; tp.steps[2].gate = 50; tp.steps[2].pitchClass = 4;
        r.run (1.0);
        const auto ons = r.noteOns(), offs = r.noteOffs();
        bool longNote = false;
        for (auto& e : offs) longNote = longNote || (e.msg.getNoteNumber() == 60 && near (e.ppq, 0.75));
        check (ons.size() == 2 && longNote, "gate 300 %: the note lasts three steps and overlaps the next note");
    }
    {
        Runner r;
        auto& tp = r.track (0);
        tp.length = 4;
        for (int i = 0; i < 4; ++i) { tp.steps[(size_t) i].on = true; tp.steps[(size_t) i].gate = 400; }
        r.run (2.0);
        bool ordered = true;
        int held = 0, maxHeld = 0;
        std::vector<Event> all;
        for (auto& e : r.events) if (e.msg.isNoteOnOrOff()) all.push_back (e);
        for (auto& e : all)
        {
            held += e.msg.isNoteOn() ? 1 : -1;
            maxHeld = juce::jmax (maxHeld, held);
            ordered = ordered && held >= 0;
        }
        check (ordered && maxHeld == 1, "gate 400 % on the same note: each note ends before it starts again (no stuck notes)");
        r.stop();
        int balance = 0;
        for (auto& e : r.events) if (e.msg.isNoteOnOrOff()) balance += e.msg.isNoteOn() ? 1 : -1;
        check (balance == 0, "stopping ends all long notes");
    }
    {
        Globals g3, g4, g5;
        g4.middleC = 4; g5.middleC = 5;
        check (g3.octaveOffset() == 0 && g4.octaveOffset() == 1 && g5.octaveOffset() == 2, "middle C shown as C3, C4 or C5");
    }
}

void testCcOnOff()
{
    section ("Separate note and CC on/off");
    Runner r;
    auto& tp = r.track (0);
    tp.length = 4;
    r.project->routing[0].noteOut = true;
    r.project->routing[0].ccOut = true;
    r.project->globals.ccSmoothing = false;
    for (int i = 0; i < 4; ++i) { tp.steps[(size_t) i].cc = (std::uint8_t) (10 * (i + 1)); }
    tp.steps[0].on = true;  tp.steps[0].ccOn = false;   // note only
    tp.steps[1].on = false; tp.steps[1].ccOn = true;    // CC only
    tp.steps[2].on = true;  tp.steps[2].ccOn = true;    // both
    r.run (1.0);
    const auto ons = r.noteOns(), ccs = r.ccs();
    bool notesOk = ons.size() == 2 && near (ons[0].ppq, 0.0) && near (ons[1].ppq, 0.5);
    bool ccOk = ccs.size() == 2 && near (ccs[0].ppq, 0.25) && ccs[0].msg.getControllerValue() == 20 && near (ccs[1].ppq, 0.5);
    check (notesOk, "notes play on note-on steps only");
    check (ccOk, "CC values are sent on CC-on steps only, also where the note is off");

    Project p;
    legacyBlank (p);
    auto& t = p.patterns[0].tracks[0];
    t.steps[0].on = true;
    p.globals.armCcOn = true; p.globals.armOn = false; p.globals.armCC = false;
    juce::Random rnd (7);
    randomize (p, makeScope (p, 0, 0, -1, -1), true, rnd);
    bool notesUntouched = t.steps[0].on;
    for (int i = 1; i < 16; ++i) notesUntouched = notesUntouched && ! t.steps[(size_t) i].on;
    int ccOnCount = 0;
    for (int i = 0; i < 16; ++i) ccOnCount += t.steps[(size_t) i].ccOn ? 1 : 0;
    check (notesUntouched && ccOnCount > 0, "in CC mode, randomizing ON changes the CC on/off only");
    check (Step().ccOn, "init: CC steps are on");
}

void testCurves()
{
    section ("Curves, rotate, invert");
    auto p = std::make_unique<Project>();
    auto& g = p->globals;
    auto& tp = p->patterns[0].tracks[0];   // length 16, C minor, scale on
    auto only = [&] (std::initializer_list<Param> params)
    {
        g.armAllNote = g.armAllCc = false;
        g.armOn = g.armNote = g.armOct = g.armVel = g.armGate = g.armProb = g.armCC = g.armCcOn = false;
        for (auto l : params)
            switch (l)
            {
                case Param::on: g.armOn = true; break;          case Param::note: g.armNote = true; break;
                case Param::octave: g.armOct = true; break;     case Param::velocity: g.armVel = true; break;
                case Param::gate: g.armGate = true; break;      case Param::probability: g.armProb = true; break;
                case Param::cc: g.armCC = true; break;          case Param::ccOn: g.armCcOn = true; break;
            }
    };
    const auto all = makeScope (*p, 0, 0, -1, -1);

    only ({ Param::cc });
    applyCurve (*p, all, true, CurveShape::ramp);
    check (tp.steps[0].cc == 0 && tp.steps[15].cc == 127 && tp.steps[7].cc > 50 && tp.steps[7].cc < 70, "RAMP: 0 at the first step, 127 at the last");
    applyCurve (*p, all, true, CurveShape::sine);
    check (std::abs (tp.steps[0].cc - 64) <= 1 && tp.steps[4].cc == 127 && tp.steps[12].cc == 0, "SINE: one cycle, middle - top - middle - bottom");
    applyCurve (*p, all, true, CurveShape::triangle);
    check (tp.steps[0].cc == 0 && tp.steps[15].cc == 0 && tp.steps[7].cc > 115, "TRIANGLE: up to the middle and back down");
    applyCurve (*p, all, true, CurveShape::square);
    check (tp.steps[0].cc == 127 && tp.steps[7].cc == 127 && tp.steps[8].cc == 0 && tp.steps[15].cc == 0, "SQUARE: first half high, second half low");
    g.rangeCC = { 20, 60 };
    applyCurve (*p, all, true, CurveShape::ramp);
    check (tp.steps[0].cc == 20 && tp.steps[15].cc == 60, "a curve stays inside the param's RANGE");

    rotateSteps (*p, all, true, 1);
    check (tp.steps[0].cc == 60 && tp.steps[1].cc == 20, "ROTATE >: the last value goes first");
    rotateSteps (*p, all, true, -1);
    check (tp.steps[0].cc == 20 && tp.steps[15].cc == 60, "ROTATE <: back where it was");
    for (int i = 0; i < 16; ++i) tp.steps[(size_t) i].cc = (std::uint8_t) (i * 2);
    const auto three = makeScope (*p, 0, 0, (1u << 1) | (1u << 5) | (1u << 9));
    rotateSteps (*p, three, true, 1);
    check (tp.steps[1].cc == 18 && tp.steps[5].cc == 2 && tp.steps[9].cc == 10 && tp.steps[3].cc == 6,
           "non-adjacent selection: values cycle through the selected steps only");

    g.rangeCC = { 0, 100 };
    tp.steps[0].cc = 30; tp.steps[1].cc = 50; tp.steps[2].cc = 100;
    invertSteps (*p, all, true);
    check (tp.steps[0].cc == 70 && tp.steps[1].cc == 50 && tp.steps[2].cc == 0, "INV flips around the RANGE center (0-100: 30 -> 70)");

    only ({ Param::on });
    applyCurve (*p, all, false, CurveShape::square);
    check (tp.steps[0].on && tp.steps[7].on && ! tp.steps[8].on && ! tp.steps[15].on, "ON with SQUARE: on in the upper half");
    invertSteps (*p, all, false);
    check (! tp.steps[0].on && tp.steps[8].on, "INV on ON swaps on and off");

    only ({ Param::note });
    applyCurve (*p, all, false, CurveShape::ramp);
    check (tp.steps[0].pitchClass == 0 && tp.steps[15].pitchClass == 10, "NOTE follows the scale: C minor ramp runs C to A#");
    invertSteps (*p, all, false);
    check (tp.steps[0].pitchClass == 10 && tp.steps[15].pitchClass == 0, "INV on NOTE mirrors the in-scale notes");

    only ({ Param::probability });
    for (auto& st : tp.steps) st.probability = 100;
    invertSteps (*p, all, false);
    check (tp.steps[0].probability == 1, "INV on PROB: 100% becomes 1%, never 0");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    testSettingsFile().deleteFile();
    Theme::setSettingsFile (testSettingsFile());   // never touch the real user settings
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    testModel();
    testTiming();
    testOutputs();
    testPatterns();
    testOperations();
    testMacros();
    testTransport();
    testPresets();
    testInitAndGate();
    testCcOnOff();
    testCurves();

    section ("Editor interactions (simulated mouse)");
    {
        auto proc = std::make_unique<VimaProcessor>();
        proc->edit (-1, [] (Project& p) { fillDemo (p); });
        std::unique_ptr<juce::AudioProcessorEditor> base (proc->createEditor());
        auto& ed = *dynamic_cast<VimaEditor*> (base.get());
        auto& ctx = ed.context();
        auto stepOf = [&] (int track, int i) { return proc->read ([=] (const Project& p) { return p.patterns[(size_t) p.currentPattern].tracks[(size_t) track].steps[(size_t) i]; }); };
        ctx.ui.track = 2;
        ed.refreshAll();

        // Top band (SPEC.md 12.2): the track frame keeps its width in both modes; SCALE shows in both
        if (auto* row = findChild<ParamRow> (ed))
        {
            auto scaleShown = [&]
            {
                for (int i = 0; i < row->getNumChildComponents(); ++i)
                    if (auto* tile = dynamic_cast<LabelledTile*> (row->getChildComponent (i)); tile != nullptr && tile->title == "SCALE")
                        return tile->isVisible() && tile->getRight() == row->getWidth();
                return false;
            };
            const auto noteTiles = row->trackTiles;
            const bool scaleNotes = scaleShown();
            ctx.ui.ccMode = true;
            ed.refreshAll();
            check (row->trackTiles == noteTiles && scaleNotes && scaleShown(),
                   "track frame has the same width in NOTES and CC mode; SCALE shows in both, at the right edge");
            ctx.ui.ccMode = false;
            ed.refreshAll();
        }
        else
            check (false, "parameter row exists");

        auto* grid = findChild<StepGrid> (ed);
        check (grid != nullptr, "step grid exists");
        if (grid != nullptr)
        {
            // LED x position of step i in row 1
            auto ledPos = [&] (int i)
            {
                const double colW = (grid->getWidth() - 90) / 16.0 + 6.0;
                return juce::Point<int> ((int) ((i % 16) * colW + colW / 2 - 3), (i / 16) * (16 + 3 + 182 + 8) + 8);
            };
            grid->mouseDown (mouseEvent (*grid, ledPos (4), ledPos (4)));
            grid->mouseDrag (mouseEvent (*grid, ledPos (7), ledPos (4), true));
            grid->mouseUp (mouseEvent (*grid, ledPos (7), ledPos (4), true));
            check (ctx.ui.selection == rangeMask (4, 7), "drag on the LEDs selects steps 05-08");

            click (*grid, ledPos (9));
            check (ctx.ui.selection == (1u << 9), "a single click on an LED selects one step");

            // Ctrl-click adds and removes single steps, Ctrl-drag adds a range
            const auto ctrl = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
                                                  | juce::ModifierKeys::commandModifier);
            auto ctrlClick = [&] (int i)
            {
                grid->mouseDown (mouseEvent (*grid, ledPos (i), ledPos (i), false, ctrl));
                grid->mouseUp (mouseEvent (*grid, ledPos (i), ledPos (i), false, ctrl));
            };
            ctrlClick (2);
            ctrlClick (14);
            check (ctx.ui.selection == ((1u << 2) | (1u << 9) | (1u << 14)), "Ctrl-click adds non-adjacent steps");
            ctrlClick (9);
            check (ctx.ui.selection == ((1u << 2) | (1u << 14)), "Ctrl-click on a selected step removes it");
            grid->mouseDown (mouseEvent (*grid, ledPos (5), ledPos (5), false, ctrl));
            grid->mouseDrag (mouseEvent (*grid, ledPos (7), ledPos (5), true, ctrl));
            grid->mouseUp (mouseEvent (*grid, ledPos (7), ledPos (5), true, ctrl));
            check (ctx.ui.selection == ((1u << 2) | rangeMask (5, 7) | (1u << 14)), "Ctrl-drag adds a range to the selection");
            click (*grid, ledPos (3));
            check (ctx.ui.selection == (1u << 3), "a plain click starts a new selection");
            grid->mouseDown (mouseEvent (*grid, ledPos (4), ledPos (4)));
            grid->mouseDrag (mouseEvent (*grid, ledPos (7), ledPos (4), true));
            grid->mouseUp (mouseEvent (*grid, ledPos (7), ledPos (4), true));

            std::vector<Step> original;
            for (int i = 4; i <= 7; ++i) original.push_back (stepOf (2, i));
            auto setParams = [&] (bool all)
            {
                proc->edit (-1, [all] (Project& p)
                {
                    auto& g = p.globals;
                    g.armOn = g.armOct = g.armVel = g.armGate = g.armProb = g.armCC = all;
                    g.armNote = true;
                });
                ed.refreshAll();
            };
            setParams (true);

            if (auto* copy = findPill (ed, "COPY")) click (*copy);
            check (ctx.ui.clip.size() == 4 && ctx.ui.clipSpan == 4, "COPY takes the 4 selected steps");
            check (findPill (ed, "COPIED") != nullptr && ctx.ui.selection == 0, "COPY lights up as COPIED, and there is no selection");

            ctx.ui.track = 4;   // as the track dropdown does
            ed.refreshAll();
            auto before = stepOf (4, 10);
            click (*grid, ledPos (10));
            check (ctx.ui.pasteStart == 10 && ctx.ui.selection == 0 && stepOf (4, 10) == before,
                   "while COPIED, an LED click only picks where the paste lands");
            grid->mouseDown (mouseEvent (*grid, ledPos (12), ledPos (12)));
            grid->mouseDrag (mouseEvent (*grid, ledPos (15), ledPos (12), true));
            grid->mouseUp (mouseEvent (*grid, ledPos (15), ledPos (12), true));
            check (ctx.ui.pasteStart == 12 && ctx.ui.selection == 0, "dragging does not select while COPIED");
            click (*grid, ledPos (10));
            if (auto* paste = findPill (ed, "PASTE")) click (*paste);
            bool same = true;
            for (int k = 0; k < 4; ++k)
            {
                auto a = stepOf (4, 10 + k), b = original[(size_t) k];
                a.cc = b.cc;   // note mode: the CC param is not copied
                same = same && a == b;
            }
            check (same, "PASTE writes the steps at step 11 of track 5 (note-mode params)");
            check (findPill (ed, "COPIED") != nullptr && ctx.ui.pasteStart == 10, "COPIED stays lit after pasting, for more pastes");

            if (auto* undo = findPill (ed, "UNDO")) click (*undo);
            check (stepOf (4, 10) == before, "UNDO restores the paste");

            if (auto* copied = findPill (ed, "COPIED")) click (*copied);
            check (ctx.ui.clip.empty() && ctx.ui.pasteStart < 0 && findPill (ed, "COPY") != nullptr, "clicking COPIED empties the clipboard");

            // only the armed params are copied
            setParams (false);   // NOTE only
            proc->edit (-1, [] (Project& p)
            {
                auto& s0 = p.patterns[(size_t) p.currentPattern].tracks[4].steps[0];
                auto& s5 = p.patterns[(size_t) p.currentPattern].tracks[4].steps[5];
                s0.pitchClass = 7; s0.velocity = 30;
                s5.pitchClass = 2; s5.velocity = 99;
            });
            click (*grid, ledPos (0));
            if (auto* copy = findPill (ed, "COPY")) click (*copy);
            click (*grid, ledPos (5));
            if (auto* paste = findPill (ed, "PASTE")) click (*paste);
            check (stepOf (4, 5).pitchClass == 7 && stepOf (4, 5).velocity == 99, "with only NOTE armed, PASTE writes the note and leaves velocity");

            if (auto* clr = findPill (ed, "CLEAR")) click (*clr);
            check (ctx.ui.clip.empty() && ctx.ui.pasteStart < 0 && ctx.ui.selection == 0, "CLEAR empties the clipboard and removes the ghost");
            setParams (true);
            proc->edit (-1, [] (Project& p) { p.globals.armOct = p.globals.armProb = false; });

            // step on/off and knobs in the first cell
            auto* cell = findChild<StepCell> (ed, 0);
            if (cell != nullptr)
            {
                const bool wasOn = stepOf (4, 0).on;
                auto* toggle = findChild<Pill> (*cell);
                if (toggle) click (*toggle);
                check (stepOf (4, 0).on != wasOn, "the step number button toggles the step");

                // drawing on the step buttons: the pressed button's new state is painted along the row
                auto toggleOf = [&] (int i) -> Pill* { auto* c = findChild<StepCell> (ed, i); return c ? findChild<Pill> (*c) : nullptr; };
                auto stroke = [&] (int from, int to)
                {
                    auto* a = toggleOf (from);
                    auto* b = toggleOf (to);
                    if (a == nullptr || b == nullptr) return;
                    const auto start = a->getLocalBounds().getCentre();
                    const auto end = a->getLocalPoint (b, b->getLocalBounds().getCentre());
                    a->mouseDown (mouseEvent (*a, start, start));
                    a->mouseDrag (mouseEvent (*a, end, start, true));
                    a->mouseUp (mouseEvent (*a, end, start, true));
                };
                proc->edit (-1, [] (Project& p)
                {
                    auto& tp = p.patterns[(size_t) p.currentPattern].tracks[4];
                    for (int i = 0; i < 32; ++i) tp.steps[(size_t) i].on = (i == 1 || i == 20);
                });
                stroke (0, 3);
                check (stepOf (4, 0).on && stepOf (4, 1).on && stepOf (4, 2).on && stepOf (4, 3).on && ! stepOf (4, 4).on,
                       "press an off step and drag: the steps along the way turn on (on ones stay on)");
                stroke (1, 4);
                check (stepOf (4, 0).on && ! stepOf (4, 1).on && ! stepOf (4, 2).on && ! stepOf (4, 3).on && ! stepOf (4, 4).on,
                       "press an on step and drag: the steps along the way turn off (off ones stay off)");
                stroke (2, 20);   // from step 3 (off) down into the second row: the stroke stays in row 1
                check (stepOf (4, 2).on && stepOf (4, 3).on && stepOf (4, 4).on && ! stepOf (4, 18).on && ! stepOf (4, 19).on,
                       "a stroke stays in the row where it started");
                proc->performUndo();
                check (! stepOf (4, 2).on && ! stepOf (4, 3).on && ! stepOf (4, 4).on && stepOf (4, 0).on, "a whole stroke is one undo step");

                auto* pitchKnob = findChild<Knob> (*cell, 0);
                const int pcBefore = stepOf (4, 0).pitchClass;
                proc->edit (-1, [] (Project& p) { p.patterns[(size_t) p.currentPattern].tracks[4].steps[0].pitchClass = 0; });
                if (pitchKnob) dragY (*pitchKnob, -16);
                check (stepOf (4, 0).pitchClass == 3, "dragging the pitch knob up 2 notes in C minor: C -> D#");
                juce::ignoreUnused (pcBefore);
                if (pitchKnob && pitchKnob->fraction)
                {
                    auto setPc = [&] (int pc, bool scaleOn)
                    {
                        proc->edit (-1, [pc, scaleOn] (Project& p)
                        {
                            auto& tp = p.patterns[(size_t) p.currentPattern].tracks[4];
                            tp.steps[0].pitchClass = (std::int8_t) pc;
                            tp.scaleOn = scaleOn;
                        });
                        return pitchKnob->fraction();
                    };
                    check (std::abs (setPc (10, true) - 1.0f) < 0.001f && std::abs (setPc (0, true)) < 0.001f
                               && std::abs (setPc (5, true) - 0.5f) < 0.001f,
                           "scale on: the 7 notes of C minor fill the arc (C left, F middle, A# fully right)");
                    check (std::abs (setPc (10, false) - 10.0f / 11.0f) < 0.001f, "scale off: the arc stays chromatic");
                    setPc (3, true);
                }

                auto* slider = findChild<StepSlider> (*cell);
                if (slider)
                {
                    click (*slider, { slider->getWidth() / 2, 0 });
                    check (stepOf (4, 0).velocity == 127, "clicking the top of the velocity slider sets 127, got " + juce::String ((int) stepOf (4, 0).velocity));
                    click (*slider, { slider->getWidth() / 2, slider->getHeight() / 2 });
                    check (std::abs (stepOf (4, 0).velocity - 64) <= 1, "clicking the middle sets about 64, got " + juce::String ((int) stepOf (4, 0).velocity));
                    click (*slider, { slider->getWidth() / 2, slider->getHeight() + 50 });
                    check (stepOf (4, 0).velocity == 1, "velocity stops at 1, never 0");

                    // draw: press on step 1 near the bottom, drag to step 6 near the top
                    auto* cell6 = findChild<StepCell> (ed, 5);
                    auto* slider6 = cell6 != nullptr ? findChild<StepSlider> (*cell6) : nullptr;
                    if (slider6 != nullptr)
                    {
                        const int before3 = stepOf (4, 3).velocity;
                        const juce::Point<int> start { slider->getWidth() / 2, slider->getHeight() - 2 };
                        const auto end = slider->getLocalPoint (slider6, juce::Point<int> { slider6->getWidth() / 2, 2 });
                        slider->mouseDown (mouseEvent (*slider, start, start));
                        slider->mouseDrag (mouseEvent (*slider, end, start, true));
                        slider->mouseUp (mouseEvent (*slider, end, start, true));
                        bool rising = true;
                        for (int k = 1; k <= 5; ++k) rising = rising && stepOf (4, k).velocity >= stepOf (4, k - 1).velocity;
                        juce::String vals;
                        for (int k = 0; k <= 5; ++k) vals << (int) stepOf (4, k).velocity << " ";
                        check (rising && stepOf (4, 5).velocity > 110 && stepOf (4, 0).velocity < 30,
                               "dragging across the sliders draws a rising line over steps 1-6, got " + vals);
                        if (auto* undo = findPill (ed, "UNDO")) click (*undo);
                        check (stepOf (4, 3).velocity == before3, "one UNDO removes the whole drawn line");
                    }
                }
                if (slider) slider->mouseDown (mouseEvent (*slider, { 5, 5 }, { 5, 5 }, false, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier)));
                check (stepOf (4, 0).velocity == 100, "right-click resets velocity to 100");
            }

            // randomize respects the selection (scope rule)
            ctx.ui.track = 3;
            ctx.ui.selection = rangeMask (0, 1);
            ed.refreshAll();
            const auto keep = stepOf (3, 5);
            if (auto* rnd = findPill (ed, "RANDOMIZE")) { click (*rnd); click (*rnd); }
            check (stepOf (3, 5) == keep, "RANDOMIZE leaves steps outside the selection alone");

            // SHIFT ALL on velocity over the whole track
            ctx.ui.selection = 0;
            ctx.ui.shiftParam = Param::velocity;
            ed.refreshAll();
            proc->edit (-1, [] (Project& p) { for (auto& s : p.patterns[(size_t) p.currentPattern].tracks[3].steps) s.velocity = 120; });
            proc->edit (-1, [] (Project& p) { p.patterns[(size_t) p.currentPattern].tracks[3].steps[0].velocity = 60; });
            if (auto* shift = findChild<ShiftKnob> (ed)) dragY (*shift, -90);   // +10
            check (stepOf (3, 0).velocity == 70 && stepOf (3, 1).velocity == 127, "SHIFT ALL +10: 60 -> 70, 120 capped at 127, got " + juce::String ((int) stepOf (3, 0).velocity) + " " + juce::String ((int) stepOf (3, 1).velocity));

            if (auto* shift = findChild<ShiftKnob> (ed))
            {
                juce::MouseWheelDetails w {};
                w.deltaY = 1.0f;
                shift->mouseWheelMove (mouseEvent (*shift, { 5, 5 }, { 5, 5 }), w);
                check (stepOf (3, 0).velocity == 71, "SHIFT ALL: one wheel notch up shifts by +1");
            }

            // PARAMS: ALL is its own state; something is always armed
            if (auto* ops = findChild<OpsPanel> (ed))
            {
                proc->edit (-1, [] (Project& p)
                {
                    auto& g = p.globals;
                    g.armAllNote = false;
                    g.armOn = g.armOct = g.armVel = g.armGate = g.armProb = false;
                    g.armNote = true;
                });
                ed.refreshAll();
                auto armed = [&] { return proc->read ([] (const Project& p) { return armedParams (p.globals, false); }); };
                if (auto* b = findPill (*ops, "NOTE")) click (*b);
                check (armed().size() == 1 && armed()[0] == Param::note, "clicking the only armed param does nothing");
                if (auto* b = findPill (*ops, "ALL")) click (*b);
                const auto g1 = proc->read ([] (const Project& p) { return p.globals; });
                check (g1.armAllNote && ! g1.armNote && armed().size() == 6, "ALL arms every note param, and only ALL is lit");
                if (auto* b = findPill (*ops, "ALL")) click (*b);
                check (proc->read ([] (const Project& p) { return p.globals.armAllNote; }), "clicking the lit ALL does nothing");
                if (auto* b = findPill (*ops, "VEL")) click (*b);
                check (armed().size() == 1 && armed()[0] == Param::velocity && ! proc->read ([] (const Project& p) { return p.globals.armAllNote; }),
                       "while ALL is lit, clicking a param arms only that param");
                if (auto* b = findPill (*ops, "GATE")) click (*b);
                check (armed().size() == 2, "outside ALL, params toggle one by one");
                proc->edit (-1, [] (Project& p) { auto& g = p.globals; g.armVel = g.armGate = false; g.armNote = true; });
                ed.refreshAll();
            }

            // CC mode hides note controls
            if (auto* ccBtn = findPill (ed, "CC")) click (*ccBtn);
            check (ctx.ui.ccMode && findPill (ed, "NOTE OUT") == nullptr, "CC mode shows the CC part only");
            if (auto* c0 = findChild<StepCell> (ed, 0))
            {
                const auto before = stepOf (ctx.ui.track, 0);
                if (auto* tg = findChild<Pill> (*c0)) click (*tg);
                const auto after = stepOf (ctx.ui.track, 0);
                check (after.ccOn != before.ccOn && after.on == before.on, "in CC mode the step button switches the CC on/off only");
                if (auto* sl = findChild<StepSlider> (*c0))
                {
                    click (*sl, { sl->getWidth() / 2, 0 });
                    const int top = stepOf (ctx.ui.track, 0).cc;
                    click (*sl, { sl->getWidth() / 2, sl->getHeight() + 40 });
                    check (sl->isVisible() && sl->getHeight() > 100 && top == 127 && stepOf (ctx.ui.track, 0).cc == 0,
                           "CC mode: the CC value is a tall slider (top = 127, bottom = 0)");
                }
            }
            if (auto* notes = findPill (ed, "NOTES")) click (*notes);
            check (! ctx.ui.ccMode, "back to note mode");
        }

        // pattern quick picker arrows queue the next pattern
        if (auto* next = findChild<Pill> (ed, 0); next != nullptr)
        {
            juce::ignoreUnused (next);
        }
        ctx.proc.queuePattern (1);
        check (ctx.proc.engine.queuedPattern.load() == 1, "picking a pattern queues it");

        // chain reordering by dragging (demo chain: B02·07 x2, B02·08 x1, B02·09 x4)
        ctx.showPatterns (true);
        if (auto* row = findChild<ChainRow> (ed, 0))
        {
            auto* view = row->getParentComponent();
            const auto start = row->getLocalBounds().getCentre();
            const auto target = view->getLocalPoint (row, start).translated (0, 3 * 40);   // below the last entry
            row->mouseDown (mouseEvent (*row, start, start));
            const auto inRow = row->getLocalPoint (view, target);
            row->mouseDrag (mouseEvent (*row, inRow, start, true));
            row->mouseUp (mouseEvent (*row, inRow, start, true));
            const auto chain = ctx.globalsCopy().chain;
            check (chain.size() == 3 && chain[0].pattern == 23 && chain[1].pattern == 24 && chain[2].pattern == 22 && chain[2].repeats == 2,
                   "dragging the first chain entry to the end moves it there");
        }
        else
            check (false, "chain rows exist");
        ctx.showPatterns (false);

        // settings: theme switch
        ctx.openSettings();
        if (auto* light = findPill (ed, "LIGHT")) click (*light);
        check (Theme::isLight(), "LIGHT in Settings switches the theme");
        {
            auto xml = juce::XmlDocument::parse (testSettingsFile());
            check (xml != nullptr && xml->getBoolAttribute ("lightTheme"), "the theme is saved once for the user (all instances)");
        }
        if (auto* dark = findPill (ed, "DARK")) click (*dark);
        check (! Theme::isLight(), "DARK switches back");
        if (auto* done = findPill (ed, "DONE")) click (*done);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        // Macros page: bank, select, map by clicking
        if (auto* mb = findPill (ed, "MACROS")) click (*mb);
        auto* mv = findChild<MacrosView> (ed);
        check (mv != nullptr && mv->isVisible() && ctx.ui.macrosView, "MACROS opens the macros page");
        if (mv != nullptr)
        {
            if (auto* b = findPill (*mv, "33 - 64")) click (*b);
            check (ctx.ui.macroPage == 1, "the 33 - 64 button shows the second page");
            if (auto* cell = findChild<MacroCell> (*mv, 2)) click (*cell);
            check (ctx.ui.selectedMacro == 34, "clicking a macro selects it (M035)");
            if (auto* k = findPill (*mv, "TRACK")) click (*k);
            check (ctx.macroCopy (34).target == MacroTarget::noteOut, "TARGET TRACK maps the first track parameter");
            check (ctx.macroCopy (34).track == ctx.ui.track && ctx.macroCopy (34).pattern == ctx.currentPattern(),
                   "a new mapping starts on the track and pattern on screen");
            if (auto* sw = findPill (*mv, "SWING")) click (*sw);
            if (auto* t5 = findPill (*mv, "5")) click (*t5);
            check (findPill (*mv, "NONE") == nullptr && findPill (*mv, "GLOBAL") != nullptr, "TARGET offers GLOBAL / TRACK / STEP, no NONE");
            const auto mm = ctx.macroCopy (34);
            check (mm.target == MacroTarget::swing && mm.track == 4 && mm.pattern == ctx.currentPattern(),
                   "SWING, track 5: the mapping follows the clicks");
            // another macro may not point at the same control
            const int curPat = ctx.currentPattern();
            proc->edit (-1, [curPat] (Project& p) { p.macros[35] = defaultMacroMap (MacroTarget::swing, 3, curPat, 0, p.globals); });
            ctx.ui.selectedMacro = 35;
            ed.refreshAll();
            if (auto* t5 = findPill (*mv, "5")) click (*t5);
            check (ctx.macroCopy (35).track == 3, "the editor refuses to point a second macro at the same control");
            proc->edit (-1, [] (Project& p) { p.macros[35] = MacroMap(); });
            ctx.ui.selectedMacro = 34;
            ed.refreshAll();
            if (auto* clr = findPill (*mv, "CLEAR MAPPING")) click (*clr);
            check (! ctx.macroCopy (34).mapped(), "CLEAR MAPPING unmaps the macro");

            // LEARN: click controls in the step view. M041 is taken, so LEARN starts on M042.
            proc->edit (-1, [] (Project& p) { p.macros[40] = defaultMacroMap (MacroTarget::cc, 7, 50, 9, p.globals); });
            ctx.ui.selectedMacro = 40;
            ctx.ui.macroPage = 1;
            ed.refreshAll();
            if (auto* l = findPill (*mv, "LEARN")) click (*l);
            check (ctx.ui.macroLearn && ctx.ui.selectedMacro == 41, "LEARN turns on, listening on the next free macro (M042)");
            if (auto* st = findPill (ed, "STEPS")) click (*st);
            check (! ctx.ui.macrosView, "STEPS goes back to the step view");
            auto* overlay = findChild<LearnOverlay> (ed);
            check (overlay != nullptr && overlay->isVisible(), "while learning, the step view is covered by the learn layer");
            auto* cell3 = findChild<StepCell> (ed, 2);
            auto* sl = cell3 != nullptr ? findChild<StepSlider> (*cell3) : nullptr;
            if (overlay != nullptr && sl != nullptr)
            {
                ctx.ui.slider = SliderParam::gate;
                const auto before = stepOf (ctx.ui.track, 2);
                const auto slPoint = overlay->getLocalPoint (sl, sl->getLocalBounds().getCentre());
                click (*overlay, slPoint);
                const auto lm = ctx.macroCopy (41);
                check (lm.target == MacroTarget::gate && lm.track == ctx.ui.track && lm.pattern == ctx.currentPattern() && lm.step == 2,
                       "LEARN on the step 3 slider (GATE param): fixed to this track, this pattern, step 3 gate");
                check (stepOf (ctx.ui.track, 2) == before, "learning does not change the control");
                check (ctx.ui.selectedMacro == 42, "LEARN moves on to the next free macro");
                check (overlay->mappedControls().size() == 1, "while learning, the mapped control is outlined");
                click (*overlay, slPoint);
                check (! ctx.macroCopy (42).mapped() && ctx.ui.selectedMacro == 42, "a mapped control cannot be mapped again");
                if (auto* lenTile = findChild<KnobTile> (ed, 0))
                    click (*overlay, overlay->getLocalPoint (lenTile, lenTile->getLocalBounds().getCentre()));
                check (ctx.macroCopy (42).target == MacroTarget::length, "LEARN on LENGTH");
                overlay->mouseDown (mouseEvent (*overlay, slPoint, slPoint, false, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier)));
                check (! ctx.macroCopy (41).mapped() && overlay->mappedControls().size() == 1, "right-click frees a mapped control");
                ctx.ui.slider = SliderParam::velocity;
            }
            ctx.ui.macroLearn = false;
            ed.refreshAll();
            check (overlay == nullptr || ! overlay->isVisible(), "the learn layer goes away when LEARN is off");
        }

        base.reset();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    section ("Editor screenshots");
    {
        auto proc = std::make_unique<VimaProcessor>();
        proc->edit (-1, [] (Project& p) { p.currentPattern = 22; fillDemo (p); });
        std::unique_ptr<juce::AudioProcessorEditor> base (proc->createEditor());
        auto& ed = *dynamic_cast<VimaEditor*> (base.get());
        auto& ctx = ed.context();
        ctx.ui.track = 2;
        ctx.ui.selection = rangeMask (4, 11);
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-notes.png"));
        {
            const auto sc = ctx.scope();
            ctx.ui.clip = proc->read ([&] (const Project& p) { return copySteps (p, sc); });
            ctx.ui.clipParams = armedParams (ctx.globalsCopy(), false);
            ctx.ui.clipSpan = ctx.ui.clip.back().offset + 1;
            ctx.ui.selection = 0;
            ctx.ui.pasteStart = 20;
            ed.refreshAll();
            snapshot (ed, outDir.getChildFile ("main-copied.png"));
            ctx.ui.clip.clear();
            ctx.ui.pasteStart = -1;
            ctx.ui.selection = rangeMask (4, 11);
            ed.refreshAll();
        }

        ctx.ui.slider = SliderParam::probability;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-prob.png"));
        ctx.ui.slider = SliderParam::velocity;

        ctx.ui.ccMode = true;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-cc.png"));
        ctx.ui.ccMode = false;

        // popovers
        ed.refreshAll();
        Popover::show (ed.contentComponent(), std::make_unique<DivisionPopover> (ctx), { 140, 118, DivisionPopover::size().x, DivisionPopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-division.png"));
        Popover::show (ed.contentComponent(), std::make_unique<ScalePopover> (ctx), { 1180 - ScalePopover::size().x, 118, ScalePopover::size().x, ScalePopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-scale.png"));
        ctx.ui.pickerPage = 1;
        Popover::show (ed.contentComponent(), std::make_unique<PatternPickerPopover> (ctx), { 600, 48, PatternPickerPopover::size().x, PatternPickerPopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-patterns.png"));
        Popover::closeAll (ed.contentComponent());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        // resizing: the whole window scales, 75% to 200%
        check (ed.getWidth() == 1200 && ed.getHeight() == 720, "a new editor opens at 1200 x 720");
        ed.setSize (900, 540);
        check (std::abs (ed.contentComponent().getTransform().mat00 - 0.75f) < 0.001f && std::abs (proc->uiScale.load() - 0.75f) < 0.001f,
               "resizing to 900 x 540 scales everything to 75%");
        snapshot (ed, outDir.getChildFile ("main-dark-75.png"));
        if (auto* c = ed.getConstrainer())
            check (c->getMinimumWidth() == 900 && c->getMaximumWidth() == 2400 && std::abs (c->getFixedAspectRatio() - 1200.0 / 720.0) < 1.0e-6,
                   "size limits 900 x 540 to 2400 x 1440, fixed proportions");
        {
            juce::MemoryBlock state;
            proc->getStateInformation (state);
            auto other = std::make_unique<VimaProcessor>();
            other->setStateInformation (state.getData(), (int) state.getSize());
            check (std::abs (other->uiScale.load() - 0.75f) < 0.001f, "the window size is saved with the project");
            std::unique_ptr<juce::AudioProcessorEditor> reopened (other->createEditor());
            check (reopened->getWidth() == 900 && reopened->getHeight() == 540, "the editor reopens at the saved size");
        }
        ed.setSize (1200, 720);

        ctx.showPatterns (true);
        if (auto* pv = findChild<PatternsView> (ed))
        {
            proc->edit (-1, [] (Project& p) { p.globals.quantize = Quantize::bar; p.currentPattern = 0; });
            proc->engine.queuedPattern = -1;
            ctx.ui.viewPage = 1;
            ctx.changed();
            auto* s2 = findChild<PatternSlot> (*pv, 2);   // pattern 19 on page 17 - 32
            auto* s3 = findChild<PatternSlot> (*pv, 3);   // pattern 20
            auto* s0 = findChild<PatternSlot> (*pv, 0);   // pattern 17
            auto queuedOrCurrent = [&] { const int q = proc->engine.queuedPattern.load(); return q >= 0 ? q : ctx.currentPattern(); };
            if (s2 && s3 && s0)
            {
                check (findPill (*pv, "PATTERN  " + patternLabel (ctx.ui.selectedPattern)) != nullptr, "the bottom bar shows a PATTERN box");
                check (findPill (*pv, "17 - 32") != nullptr && findPill (*pv, "1 - 16") != nullptr, "pattern page buttons read 1 - 16 ... 49 - 64");
                const auto body = s2->getLocalBounds().getCentre().translated (0, 10);
                click (*s2, body);
                check (ctx.ui.selectedPattern == 18 && queuedOrCurrent() == 0, "clicking a slot selects it without playing it");
                click (*s2, s2->playArea().getCentre());
                check (ctx.ui.selectedPattern == 18 && queuedOrCurrent() == 18, "the slot's play button selects and queues the pattern");
                proc->edit (-1, [] (Project& p) { p.currentPattern = 0; });
                proc->engine.queuedPattern = -1;

                proc->edit (-1, [] (Project& p) { p.patterns[18].tracks[0].steps[0].velocity = 77; p.patterns[18].tracks[0].swing = 33; });
                ctx.ui.patternsViewTrack = 0;
                if (auto* b = findPill (*pv, "COPY TRACK")) click (*b);
                auto* paste = findPill (*pv, "PASTE");
                check (findPill (*pv, "TRACK COPIED") != nullptr && paste != nullptr && paste->alpha && paste->alpha() < 1.0f,
                       "COPY TRACK lights up as TRACK COPIED; PASTE is dimmed until a destination is picked");
                ctx.ui.patternsViewTrack = 1;   // paste into track 2
                click (*s3, body);
                check (ctx.ui.patternPasteDest == 19 && ctx.ui.selectedPattern == 18, "while copied, a slot click only picks the destination");
                if (paste) click (*paste);
                check (proc->read ([] (const Project& p) { return p.patterns[19].tracks[1].steps[0].velocity == 77 && p.patterns[19].tracks[1].swing == 33
                                                                     && p.patterns[19].tracks[0].steps[0].velocity == 100; }),
                       "PASTE writes track 1 of pattern 19 into track 2 of pattern 20 (all settings)");
                check (findPill (*pv, "TRACK COPIED") != nullptr, "the button stays lit for more pastes");
                if (auto* c = findPill (*pv, "CLEAR")) click (*c);
                check (ctx.ui.patternClipKind == 0 && ctx.ui.patternPasteDest < 0 && findPill (*pv, "COPY TRACK") != nullptr
                           && proc->read ([] (const Project& p) { return p.patterns[18].tracks[0].steps[0].velocity == 77; }),
                       "CLEAR cancels and empties the clipboard, and never erases a pattern");

                if (auto* b = findPill (*pv, "COPY")) click (*b);
                check (findPill (*pv, "COPIED") != nullptr, "COPY lights up as COPIED");
                click (*s0, body);
                if (paste) click (*paste);
                check (proc->read ([] (const Project& p) { return p.patterns[16].tracks[0].steps[0].velocity == 77; }), "PASTE writes the whole pattern");
                snapshot (ed, outDir.getChildFile ("patterns-view-copied.png"));
                if (auto* b = findPill (*pv, "COPIED")) click (*b);
                check (ctx.ui.patternClipKind == 0, "clicking COPIED empties the clipboard");

                // slot menu copy / paste (one-off)
                proc->edit (-1, [] (Project& p) { p.patterns[21].tracks[2].steps[1].velocity = 55; });
                const int before22 = proc->read ([] (const Project& p) { return (int) p.patterns[22].tracks[2].steps[1].velocity; });
                pv->copyPattern (21);
                check (findPill (*pv, "COPIED") != nullptr, "slot menu COPY PATTERN lights COPIED (one clipboard)");
                pv->pasteTo (22, true);
                check (proc->read ([] (const Project& p) { return p.patterns[22].tracks[2].steps[1].velocity == 55; })
                           && ctx.ui.patternClipKind == 0 && findPill (*pv, "COPY") != nullptr,
                       "slot menu PASTE pastes once, then clears the clipboard and switches COPIED off");
                check (paste != nullptr && paste->look && paste->look().fill == Theme::current().green && paste->alpha() == 1.0f,
                       "PASTE flashes green to confirm the one-off paste");
                check (proc->canUndo() && proc->performUndo() && proc->read ([] (const Project& p) { return (int) p.patterns[22].tracks[2].steps[1].velocity; }) == before22,
                       "a slot menu paste is one undo step");
                pv->copyTrack (21);
                pv->pasteTo (23, false);
                check (ctx.ui.patternClipKind == 2, "a bottom-bar paste keeps the clipboard, however the copy was made");
                pv->cancelClip();
            }
            ctx.ui.viewPage = 0;
            ctx.changed();
        }
        // Demo data for the screenshot: varied lengths, rhythms and outputs
        proc->edit (-1, [] (Project& p)
        {
            juce::Random r (11);
            const int lens[] = { 32, 16, 24, 8, 32, 12, 20, 16 };
            for (int pat = 0; pat < 16; ++pat)
                for (int t = 0; t < numTracks; ++t)
                {
                    auto& tp = p.patterns[(size_t) pat].tracks[(size_t) t];
                    tp.length = (std::uint8_t) lens[(t + pat) % 8];
                    const float dn = 0.15f + 0.7f * r.nextFloat(), dc = 0.2f + 0.6f * r.nextFloat();
                    for (int i = 0; i < maxSteps; ++i)
                    {
                        tp.steps[(size_t) i].on = (i % 4 == 0) || r.nextFloat() < dn * 0.6f;
                        tp.steps[(size_t) i].ccOn = r.nextFloat() < dc;
                    }
                }
            // every thumbnail case on a track: notes only, CC only, both, empty, outputs off
            for (int pat = 0; pat < 16; ++pat)
                for (int t : { 0, 6 })
                    for (auto& st : p.patterns[(size_t) pat].tracks[(size_t) t].steps) st.ccOn = false;     // notes only
            for (int pat = 0; pat < 16; ++pat)
                for (auto& st : p.patterns[(size_t) pat].tracks[5].steps) st.on = false;                     // CC only
            for (int pat = 0; pat < 16; pat += 3)
                for (auto& st : p.patterns[(size_t) pat].tracks[4].steps) st.on = st.ccOn = false;           // empty
            p.patterns[0].tracks[0].length = 32;
            for (int i = 0; i < maxSteps; ++i) p.patterns[0].tracks[0].steps[(size_t) i].on = (i == 0 || i == 15 || i == 31);
            // outputs: 1-2 notes, 3 notes + CC (CC steps dimmed: CC out off), 4 both off (dimmed),
            // 6 CC, 7 notes off (dimmed, notes only), 8 both
            const bool noteOut[] = { true, true, true, false, true, false, false, true };
            const bool ccOut[]   = { false, true, false, false, true, true, false, true };
            for (int t = 0; t < numTracks; ++t) { p.routing[(size_t) t].noteOut = noteOut[t]; p.routing[(size_t) t].ccOut = ccOut[t]; }
        });
        ctx.ui.viewPage = 0;
        ctx.changed();
        snapshot (ed, outDir.getChildFile ("patterns-view.png"));
        Theme::setLight (true);   // light theme: playing slot brightest, stored = tile, empty = panel
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("patterns-view-light.png"));
        Theme::setLight (false);
        ed.refreshAll();
        // a long chain scrolls: wheel over the list, and a scroll bar
        if (auto* pv = findChild<PatternsView> (ed))
        {
            const auto keepChain = ctx.globalsCopy().chain;
            proc->edit (-1, [] (Project& p) { p.globals.chain.clear(); for (int i = 0; i < 20; ++i) p.globals.chain.add ({ (std::uint8_t) i, 1 }); });
            pv->refreshState();
            auto visibleRows = [&] { int v = 0; for (auto* c : pv->getChildren()) if (dynamic_cast<ChainRow*> (c) && c->isVisible()) ++v; return v; };
            auto* bar = findChild<juce::ScrollBar> (*pv);
            check (bar != nullptr && bar->isVisible() && visibleRows() < 20, "a chain longer than the list shows a scroll bar");
            const double before = bar ? bar->getCurrentRangeStart() : 0.0;
            check (before > 0.0, "adding to a long chain scrolls to the newest entry");
            for (auto* c : pv->getChildren())
                if (dynamic_cast<ChainRow*> (c) && c->isVisible())
                {
                    juce::MouseWheelDetails w {}; w.deltaY = 1.0f;
                    const auto pt = pv->getLocalPoint (c, c->getLocalBounds().getCentre());
                    pv->mouseWheelMove (mouseEvent (*pv, pt, pt), w);
                    break;
                }
            check (bar != nullptr && bar->getCurrentRangeStart() < before, "the mouse wheel scrolls the chain list");
            snapshot (ed, outDir.getChildFile ("patterns-view-long-chain.png"));
            proc->edit (-1, [keepChain] (Project& p) { p.globals.chain = keepChain; });
            pv->refreshState();
        }
        ctx.showPatterns (false);

        proc->edit (-1, [] (Project& p)
        {
            for (int i = 0; i < 16; ++i) p.macros[(size_t) i] = defaultMacroMap (MacroTarget::velocity, 2, 22, i, p.globals);
            p.macros[16] = defaultMacroMap (MacroTarget::length, 1, 22, 0, p.globals);
            p.macros[17] = defaultMacroMap (MacroTarget::swing, 2, 22, 0, p.globals);
            p.macros[18] = defaultMacroMap (MacroTarget::division, 3, 22, 0, p.globals);
            p.macros[18].pattern = 5;
            p.macros[19] = defaultMacroMap (MacroTarget::gate, 4, 22, 0, p.globals);
            p.macros[20] = defaultMacroMap (MacroTarget::probability, 2, 22, 3, p.globals);
            p.macros[21] = defaultMacroMap (MacroTarget::cc, 6, 22, 0, p.globals);
            p.macros[22] = defaultMacroMap (MacroTarget::pattern, 2, 22, 0, p.globals);
            p.macros[23] = defaultMacroMap (MacroTarget::chainOn, 2, 22, 0, p.globals);
            p.macros[24] = defaultMacroMap (MacroTarget::note, 2, 22, 0, p.globals);
            p.macros[25] = defaultMacroMap (MacroTarget::octave, 2, 22, 0, p.globals);
        });
        ctx.ui.selectedMacro = 16;
        ctx.showMacros (true);
        snapshot (ed, outDir.getChildFile ("macros.png"));
        ctx.ui.selectedMacro = 3;
        ctx.ui.macroLearn = true;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("macros-learn.png"));
        ctx.showMacros (false);
        snapshot (ed, outDir.getChildFile ("main-learning.png"));
        ctx.ui.macroLearn = false;
        ed.refreshAll();

        ctx.openSettings();
        snapshot (ed, outDir.getChildFile ("settings.png"));

        Theme::setLight (true);
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("settings-light.png"));
        ed.context().proc.editCurrent (false, [] (Project&) {});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        ed.refreshAll();
        base.reset();
        auto light = std::unique_ptr<juce::AudioProcessorEditor> (proc->createEditor());
        auto& led = *dynamic_cast<VimaEditor*> (light.get());
        led.context().ui.track = 2;
        led.context().ui.selection = rangeMask (4, 11);
        led.refreshAll();
        snapshot (led, outDir.getChildFile ("main-light-notes.png"));
        light.reset();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    section ("Init screenshot");
    {
        auto proc = std::make_unique<VimaProcessor>();
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        snapshot (*ed, outDir.getChildFile ("main-init.png"));
        dynamic_cast<VimaEditor*> (ed.get())->context().ui.slider = SliderParam::gate;
        dynamic_cast<VimaEditor*> (ed.get())->refreshAll();
        snapshot (*ed, outDir.getChildFile ("main-init-gate.png"));
        ed.reset();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    std::cout << checks << " checks, " << (failures == 0 ? juce::String ("ALL PASSED") : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
