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
        bool playing = false;
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
    std::atomic<bool> isPlaying { false };
    std::atomic<int> chainPosition { 0 };
    std::atomic<float> chainEntryProgress { 0.0f };
    std::atomic<int> chainRepeat { 0 };
    // Beats until the next scheduled switch to a different pattern (a chain switch, or a picked
    // pattern waiting for quantize); -1 when none, or when the playing pattern lasts 1 beat or less
    // (SPEC.md 7.5: the last-beat blink)
    std::atomic<double> beatsToSwitch { -1.0 };

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

    // Testing hook: deterministic probability rolls
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); }

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

    juce::Random random;
};
} // namespace vima
