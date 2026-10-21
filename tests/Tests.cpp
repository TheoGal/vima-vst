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

#include <map>
#include <memory>
#include <vector>

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
    for (auto& s : tp.steps) { s.on = true; s.pitchClass = (std::int8_t) pc; s.octave = (std::int8_t) oct; }
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
    check (noteDisplayName (1, 3, false) == "C#3" && noteDisplayName (1, 3, true) == "C#4", "note names follow the middle C setting");
    check (patternLabel (0) == juce::String (juce::CharPointer_UTF8 ("B01 \xc2\xb7 01")) && patternLabel (17).startsWith ("B02"), "pattern labels");

    check (Engine::stepIndexFor (0, 4, Direction::forward) == 0 && Engine::stepIndexFor (5, 4, Direction::forward) == 1, "forward positions");
    check (Engine::stepIndexFor (0, 4, Direction::backward) == 3 && Engine::stepIndexFor (3, 4, Direction::backward) == 0, "backward positions");
    std::vector<int> pp;
    for (int n = 0; n < 8; ++n) pp.push_back (Engine::stepIndexFor (n, 4, Direction::pingPong));
    check (pp == std::vector<int> { 0, 1, 2, 3, 2, 1, 0, 1 }, "ping-pong positions (ends played once)");
    check (Engine::stepIndexFor (-1, 4, Direction::forward) == 3, "negative positions wrap");

    // state round trip
    auto p = std::make_unique<Project>();
    p->patterns[5].tracks[3].steps[7] = { true, 4, 5, 33, 77, 60, 99 };
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
    p->globals.laneProb = true;
    p->globals.rangeVel = { 10, 20 };
    p->currentPattern = 5;
    auto q = std::make_unique<Project>();
    q->fromValueTree (p->toValueTree());
    const auto& qs = q->patterns[5].tracks[3];
    check (qs.steps[7] == p->patterns[5].tracks[3].steps[7], "step survives save/load");
    check (qs.length == 24 && qs.division == 8 && qs.direction == Direction::pingPong && qs.swing == 40, "track settings survive save/load");
    check (q->routing[3].midiChannel == 9 && q->routing[3].ccOut && q->routing[3].ccNumber == 21, "routing survives save/load");
    check (q->globals.chain.size() == 1 && q->globals.chain[0].repeats == 3 && q->globals.quantize == Quantize::off, "chain and quantize survive");
    check (q->globals.scale == 4 && q->globals.root == 7 && q->globals.laneProb && q->globals.rangeVel.min == 10, "globals survive");
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
        for (auto& s : r.track (0).steps) s.probability = 0;
        r.run (4.0);
        check (r.noteOns().empty(), "probability 0% plays nothing");
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
        tp.steps[0] = { true, 0, 3, 100, 50, 100, 0 };
        tp.steps[2] = { true, 0, 3, 100, 50, 100, 100 };
        r.project->routing[0].noteOut = false;
        r.project->routing[0].ccOut = true;
        r.project->globals.ccSmoothing = true;
        r.project->globals.ccSmoothingTime = 100;
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

        Runner r2;
        r2.project->patterns[0] = r.project->patterns[0];
        r2.project->routing[0] = r.project->routing[0];
        r2.project->globals.ccSmoothingTime = 25;
        r2.run (2.0);
        bool holdsFirst = true;
        for (auto& e : r2.ccs (1))
            if (e.ppq > 0.01 && e.ppq < 0.37) holdsFirst = holdsFirst && e.msg.getControllerValue() == 0;
        check (holdsFirst, "CC smoothing 25%: holds the value, then glides only in the last quarter");
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
    auto& tp = p->patterns[0].tracks[0];
    juce::Random rnd (42);

    {
        auto sc = makeScope (*p, 0, 0, -1, -1);
        check (sc.first == 0 && sc.last == 15, "no selection = all steps within the length");
        sc = makeScope (*p, 0, 0, 4, 40);
        check (sc.first == 4 && sc.last == 15, "selection is clipped to the length");
    }
    {
        p->globals.laneOn = false;
        p->globals.laneNote = true; p->globals.laneVel = true; p->globals.laneOct = true;
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
        for (int i = 16; i < 32; ++i) untouched = untouched && tp.steps[(size_t) i] == Step();
        check (untouched, "randomize does not touch steps beyond the length");
    }
    {
        for (auto& s : tp.steps) s = Step();
        tp.steps[0].on = tp.steps[1].on = tp.steps[2].on = true;
        tp.steps[0].cc = 0; tp.steps[1].cc = 64; tp.steps[2].cc = 127;
        tp.steps[3].cc = 0;   // off step
        p->globals.laneCC = true;
        smooth (*p, makeScope (*p, 0, 0, 0, 3), true);
        check (tp.steps[0].cc == 1 && tp.steps[1].cc == 64 && tp.steps[2].cc == 126, "smooth: 0, 64, 127 -> 1, 64, 126");
        check (tp.steps[3].cc == 0, "smooth leaves off steps alone");
    }
    {
        for (auto& s : tp.steps) s = Step();
        tp.steps[0].velocity = 120; tp.steps[1].velocity = 60;
        auto gst = beginShift (*p, makeScope (*p, 0, 0, 0, 1), Lane::velocity);
        applyShift (*p, gst, 20);
        check (tp.steps[0].velocity == 127 && tp.steps[1].velocity == 80, "shift caps at the limit (127)");
        applyShift (*p, gst, 0);
        check (tp.steps[0].velocity == 120 && tp.steps[1].velocity == 60, "turning back in the same gesture restores the shape");
        auto gst2 = beginShift (*p, makeScope (*p, 0, 0, 0, 1), Lane::velocity);
        applyShift (*p, gst2, -200);
        check (tp.steps[0].velocity == 1 && tp.steps[1].velocity == 1, "velocity never goes below 1");
    }
    {
        for (auto& s : tp.steps) s = Step();
        tp.steps[0].pitchClass = 10; tp.steps[0].octave = 3;   // A#3 in C minor
        auto gst = beginShift (*p, makeScope (*p, 0, 0, 0, 0), Lane::note);
        applyShift (*p, gst, 1);
        check (tp.steps[0].pitchClass == 0 && tp.steps[0].octave == 4, "note shift moves in scale steps and rolls into the next octave");
        tp.scaleOn = false;
        auto gst2 = beginShift (*p, makeScope (*p, 0, 0, 0, 0), Lane::note);
        applyShift (*p, gst2, 1);
        check (tp.steps[0].pitchClass == 1 && tp.steps[0].octave == 4, "scale off: note shift moves in semitones");
        tp.scaleOn = true;
    }
    {
        for (auto& s : tp.steps) s = Step();
        tp.steps[0].pitchClass = 0;
        nudgePitchClass (*p, 0, 0, 0, 1);
        check (tp.steps[0].pitchClass == 2, "pitch knob only lands on scale notes (C -> D in C minor)");
        nudgePitchClass (*p, 0, 0, 0, 1);
        check (tp.steps[0].pitchClass == 3, "D -> D# in C minor");
    }
    {
        for (auto& s : tp.steps) s = Step();
        for (int i = 0; i < 4; ++i) { tp.steps[(size_t) i].on = true; tp.steps[(size_t) i].cc = (std::uint8_t) (10 + i); }
        auto clip = copySteps (*p, makeScope (*p, 0, 0, 0, 3));
        auto& other = p->patterns[0].tracks[5];
        other.length = 8;
        const int last = pasteSteps (*p, 0, 5, 6, clip);
        check (last == 7 && other.steps[6].cc == 10 && other.steps[7].cc == 11 && other.steps[8] == Step(),
               "paste onto another track is cut off at its length");
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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    testModel();
    testTiming();
    testOutputs();
    testPatterns();
    testOperations();

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
            check (ctx.ui.selFirst == 4 && ctx.ui.selLast == 7, "drag on the LEDs selects steps 05-08");

            click (*grid, ledPos (9));
            check (ctx.ui.selFirst == 9 && ctx.ui.selLast == 9, "a single click on an LED selects one step");
            grid->mouseDown (mouseEvent (*grid, ledPos (4), ledPos (4)));
            grid->mouseDrag (mouseEvent (*grid, ledPos (7), ledPos (4), true));
            grid->mouseUp (mouseEvent (*grid, ledPos (7), ledPos (4), true));

            std::vector<Step> original;
            for (int i = 4; i <= 7; ++i) original.push_back (stepOf (2, i));

            if (auto* copy = findPill (ed, "COPY")) click (*copy);
            check (ctx.ui.clip.size() == 4, "COPY takes the 4 selected steps");

            ctx.ui.track = 4;   // as the track dropdown does
            ctx.ui.selFirst = ctx.ui.selLast = -1;
            ed.refreshAll();
            auto before = stepOf (4, 10);
            if (auto* paste = findPill (ed, "PASTE 4")) click (*paste);
            check (ctx.ui.pasteArmed, "PASTE arms paste mode");
            click (*grid, ledPos (10));
            bool same = true;
            for (int k = 0; k < 4; ++k) same = same && stepOf (4, 10 + k) == original[(size_t) k];
            check (same && ! ctx.ui.pasteArmed, "clicking LED 11 on track 5 pastes the steps there");
            check (ctx.ui.selFirst == 10 && ctx.ui.selLast == 13, "the pasted range becomes the selection");

            if (auto* undo = findPill (ed, "UNDO")) click (*undo);
            check (stepOf (4, 10) == before, "UNDO restores the paste");

            // step on/off and knobs in the first cell
            auto* cell = findChild<StepCell> (ed, 0);
            if (cell != nullptr)
            {
                const bool wasOn = stepOf (4, 0).on;
                auto* toggle = findChild<Pill> (*cell);
                if (toggle) click (*toggle);
                check (stepOf (4, 0).on != wasOn, "the step number button toggles the step");

                auto* pitchKnob = findChild<Knob> (*cell, 0);
                const int pcBefore = stepOf (4, 0).pitchClass;
                proc->edit (-1, [] (Project& p) { p.patterns[(size_t) p.currentPattern].tracks[4].steps[0].pitchClass = 0; });
                if (pitchKnob) dragY (*pitchKnob, -16);
                check (stepOf (4, 0).pitchClass == 3, "dragging the pitch knob up 2 notes in C minor: C -> D#");
                juce::ignoreUnused (pcBefore);

                auto* slider = findChild<LaneSlider> (*cell);
                proc->edit (-1, [] (Project& p) { p.patterns[(size_t) p.currentPattern].tracks[4].steps[0].velocity = 100; });
                if (slider) dragY (*slider, 30);
                check (stepOf (4, 0).velocity == 80, "dragging the velocity slider down lowers velocity (100 -> 80)");
                if (slider) dragY (*slider, 1000);
                check (stepOf (4, 0).velocity == 1, "velocity stops at 1, never 0");
                if (slider) slider->mouseDown (mouseEvent (*slider, { 5, 5 }, { 5, 5 }, false, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier)));
                check (stepOf (4, 0).velocity == 100, "right-click resets velocity to 100");
            }

            // randomize respects the selection (scope rule)
            ctx.ui.track = 3;
            ctx.ui.selFirst = 0; ctx.ui.selLast = 1;
            ed.refreshAll();
            const auto keep = stepOf (3, 5);
            if (auto* rnd = findPill (ed, "RANDOMIZE")) { click (*rnd); click (*rnd); }
            check (stepOf (3, 5) == keep, "RANDOMIZE leaves steps outside the selection alone");

            // SHIFT ALL on velocity over the whole track
            ctx.ui.selFirst = ctx.ui.selLast = -1;
            ctx.ui.shiftLane = Lane::velocity;
            ed.refreshAll();
            proc->edit (-1, [] (Project& p) { for (auto& s : p.patterns[(size_t) p.currentPattern].tracks[3].steps) s.velocity = 120; });
            proc->edit (-1, [] (Project& p) { p.patterns[(size_t) p.currentPattern].tracks[3].steps[0].velocity = 60; });
            if (auto* shift = findChild<ShiftKnob> (ed)) dragY (*shift, -90);   // +10
            check (stepOf (3, 0).velocity == 70 && stepOf (3, 1).velocity == 127, "SHIFT ALL +10: 60 -> 70, 120 capped at 127");

            // CC mode hides note controls
            if (auto* ccBtn = findPill (ed, "CC")) click (*ccBtn);
            check (ctx.ui.ccMode && findPill (ed, "NOTE OUT") == nullptr, "CC mode shows the CC part only");
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

        // settings: theme switch
        ctx.openSettings();
        if (auto* light = findPill (ed, "LIGHT")) click (*light);
        check (ctx.globalsCopy().lightTheme && Theme::isLight(), "LIGHT in Settings switches the theme");
        if (auto* dark = findPill (ed, "DARK")) click (*dark);
        check (! ctx.globalsCopy().lightTheme, "DARK switches back");
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
        ctx.ui.selFirst = 4;
        ctx.ui.selLast = 11;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-notes.png"));

        ctx.ui.slider = SliderLane::probability;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-prob.png"));
        ctx.ui.slider = SliderLane::velocity;

        ctx.ui.ccMode = true;
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("main-dark-cc.png"));
        ctx.ui.ccMode = false;

        // popovers
        ed.refreshAll();
        Popover::show (ed, std::make_unique<DivisionPopover> (ctx), { 140, 118, DivisionPopover::size().x, DivisionPopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-division.png"));
        Popover::show (ed, std::make_unique<ScalePopover> (ctx), { 616, 118, ScalePopover::size().x, ScalePopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-scale.png"));
        ctx.ui.pickerBank = 1;
        Popover::show (ed, std::make_unique<PatternPickerPopover> (ctx), { 600, 48, PatternPickerPopover::size().x, PatternPickerPopover::size().y });
        snapshot (ed, outDir.getChildFile ("popover-patterns.png"));
        Popover::closeAll (ed);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        ctx.showPatterns (true);
        snapshot (ed, outDir.getChildFile ("patterns-view.png"));
        ctx.showPatterns (false);

        ctx.openSettings();
        snapshot (ed, outDir.getChildFile ("settings.png"));

        proc->edit (-1, [] (Project& p) { p.globals.lightTheme = true; });
        ed.refreshAll();
        snapshot (ed, outDir.getChildFile ("settings-light.png"));
        ed.context().proc.editCurrent (false, [] (Project&) {});
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        ed.refreshAll();
        base.reset();
        auto light = std::unique_ptr<juce::AudioProcessorEditor> (proc->createEditor());
        auto& led = *dynamic_cast<VimaEditor*> (light.get());
        led.context().ui.track = 2;
        led.context().ui.selFirst = 4;
        led.context().ui.selLast = 11;
        led.refreshAll();
        snapshot (led, outDir.getChildFile ("main-light-notes.png"));
        light.reset();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    std::cout << checks << " checks, " << (failures == 0 ? juce::String ("ALL PASSED") : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
