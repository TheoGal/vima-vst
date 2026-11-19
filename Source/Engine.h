#pragma once

// VIMA sequencer engine (audio thread). Turns the project + host transport into MIDI.
// Step positions are derived from the host song position (stateless), so loops, jumps
// and pattern switches stay in sync. See SPEC.md sections 3-10.

#include <juce_audio_basics/juce_audio_basics.h>
#include "Model.h"
#include <array>
#include <atomic>

namespace vima
{
class Engine
{
public:
    struct Host
    {
        bool playing = false;       // Vima runs (the DAW, or the play button): all tracks play
        bool dawPlaying = false;    // the DAW itself rolls (ppq is then the DAW's position), even while Vima is held
        double ppq = 0.0;
        double bpm = 120.0;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;
    };

    Engine();

    void prepare (double sampleRate);
    void reset();

    // Audio thread. The caller holds the project lock for the duration of the call.
    // The engine may change project.currentPattern (pattern switches, chain).
    void process (Project& project, const Host& host, int numSamples,
                  const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut);

    // Message thread -> audio thread: request a pattern change (follows the quantize setting)
    std::atomic<int> queuedPattern { -1 };

    // Audio thread -> message thread: display state
    std::array<std::atomic<int>, numTracks> playStep;   // -1 when stopped
    std::atomic<bool> isPlaying { false };       // Vima's own play state (the DAW, the play button): never set by keys
    std::atomic<bool> triggerPlaying { false };  // NOTE INPUT TRIGGER: keys play the chosen tracks while Vima itself is stopped
    std::atomic<double> triggerClock { -1.0 };   // ... the position of that run on Vima's own clock (-1: none), so the play button can pick it up
    // Note trigger (SPEC.md 3.1): the transposition each track plays with, in SHIFT steps (display)
    std::array<std::atomic<int>, numTracks> shownTranspose;
    std::atomic<int> chainPosition { 0 };
    std::atomic<float> chainEntryProgress { 0.0f };
    std::atomic<int> chainRepeat { 0 };
    // Beats until the next scheduled switch to a different pattern (a chain switch, or a picked
    // pattern waiting for quantize); -1 when none, or when the playing pattern lasts 1 beat or less
    // (SPEC.md 7.5: the last-beat blink)
    std::atomic<double> beatsToSwitch { -1.0 };
    // A hand pick while the chain plays is a one-off insertion after the current entry (SPEC.md 7.2):
    // the inserted pattern (waiting or playing, -1 = none), whether it plays, and its progress
    std::atomic<int> insertPattern { -1 };
    std::atomic<bool> insertPlaying { false };
    std::atomic<float> insertProgress { 0.0f };
    static constexpr double switchWarnBeats = 1.5;   // the blink before a switch: three 8th notes

    // Position of step n on the track's timeline (in steps), for a given length and direction
    static int stepIndexFor (long long n, int length, Direction dir);
    static double swingOffset (long long n, double stepPpq, int swingPercent);

    // Call with the project lock held after moving a chain entry, so playback keeps
    // following the entry that is playing
    void chainEntryMoved (int from, int to)
    {
        if (chainIndex == from) chainIndex = to;
        else if (from < chainIndex && chainIndex <= to) --chainIndex;
        else if (to <= chainIndex && chainIndex < from) ++chainIndex;
    }

    void chainEntryRemoved (int index)
    {
        if (index < chainIndex) --chainIndex;
    }

    // Call with the project lock held after inserting a chain entry (drag from the patterns view)
    void chainEntryInserted (int index)
    {
        if (chainActive && index <= chainIndex) ++chainIndex;
    }

    // Testing hook: deterministic probability rolls
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); ccRollSeed = (std::uint32_t) random.nextInt(); }

private:
    struct PendingOff
    {
        double ppq;
        std::uint8_t channel;
        std::uint8_t note;
    };

    void renderSegment (const Project& project, int patternIndex, double a, double b, double blockStartPpq,
                        double ppqPerSample, int numSamples, juce::MidiBuffer& out);
    void renderCCSmoothing (const Project& project, int patternIndex, double a, double b, double blockStartPpq,
                            double ppqPerSample, int numSamples, juce::MidiBuffer& out);
    void emitDueNoteOffs (double beforePpq, double blockStartPpq, double ppqPerSample, int numSamples, juce::MidiBuffer& out);
    void flushAllNotes (juce::MidiBuffer& out, int samplePos);
    void addPendingOff (double ppq, int channel, int note);
    void releaseSameNote (int channel, int note, int samplePos, juce::MidiBuffer& out);

    double nextQuantizePoint (const Project& project, const Host& host, double ppq) const;

    // Note trigger and MIDI thru (SPEC.md 3.1)
    void readTriggerNotes (const Project& project, const juce::MidiBuffer& midiIn);
    void passThru (const Project& project, const juce::MidiBuffer& midiIn, juce::MidiBuffer& out) const;
    void updateTriggerTracks (const Project& project, const Host& host, double a, double ppqPerSample, bool openUnchosen);
    void scheduleTranspose (const Project& project, const Host& host, double a, bool running);
    void commitTranspose (double ppqEnd);
    void finishTriggerBlock (double ppqEnd);
    int scopeOf (const Project& project, int track) const;
    static int transposeNote (int pitch, int steps, bool inScale, std::uint16_t mask, int root);
    static double triggerQuantizePoint (const Globals& g, const Host& host, double ppq);

    static constexpr int numScopes = 1;    // one scope: the tracks the keys drive
    static constexpr int maxHeld = 16;
    std::array<std::array<int, maxHeld>, numScopes> held {};   // held keys, oldest first (last note wins)
    std::array<int, numScopes> numHeld {};
    std::array<int, numScopes> pressSample {};                 // first key press in this block (-1 = none)
    std::array<int, numTracks> transpose {};                   // per track, in SHIFT steps (sounding)
    std::array<int, numTracks> targetTranspose {};             // what the keys ask for
    std::array<int, numTracks> pendTranspose {};               // ... scheduled at pendAt (Quantize)
    std::array<double, numTracks> pendAt {};
    double unchosenOrigin = 0.0;   // step 1 time of the unchosen tracks when Vima's play joined a key run
    bool gating = false;           // TRIGGER mode while Vima runs: the chosen tracks play only while keys are held
    NoteTrigger lastTriggerMode = NoteTrigger::off;
    bool trigRunning = false;      // Vima runs because keys started it (not the DAW or the play button)
    bool trigUsesDaw = false;      // ... on the DAW's clock (the DAW rolls, Vima was held) or its own
    double trigClock = 0.0;
    std::array<bool, numTracks> trackOn {}, startPending {}, stopPending {};
    std::array<double, numTracks> trackFrom {}, trackTo {}, trackOrigin {};   // play window and step 1 time
    double chainEntryEnd (const Project& project) const;

    double sampleRate = 44100.0;
    bool wasPlaying = false;
    double lastPpqEnd = -1.0;

    static constexpr int maxPending = 512;
    std::array<PendingOff, maxPending> pending {};
    int numPending = 0;

    std::array<int, numTracks> lastCCSent {};
    int lastChainOnState = -1;
    int chainIndex = 0;
    double chainEntryStart = 0.0;
    bool chainActive = false;
    bool chainFinished = false;     // LOOP OFF: the chain has played once (keeps its last pattern)
    bool chainJoinPending = false;  // chain switched on while playing: its first entry waits for quantize
    bool inserting = false;         // a hand-picked pattern plays once between two chain entries
    double insertStart = 0.0;

    juce::Random random;

    // CC probability (SPEC.md 5): one roll per step play, apart from the note roll. A hash of the
    // step's position in the song, so the CC smoothing glide and the step send agree on it.
    std::uint32_t ccRollSeed = 0;
    bool ccRollPasses (const Step& step, int track, long long n) const
    {
        if (step.ccProbability >= 100)
            return true;
        auto h = (std::uint64_t) n * 0x9E3779B97F4A7C15ull ^ ((std::uint64_t) (track + 1) << 56) ^ ccRollSeed;
        h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ull; h ^= h >> 33;
        return (int) (h % 100) < (int) step.ccProbability;
    }
};
} // namespace vima
