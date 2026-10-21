#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

// Milestone 1: MIDI routing test.
// VIMA loads as an instrument and, while the host plays, sends a fixed test
// pattern on MIDI channels 1-4 (notes plus a CC 74 sweep). This lets the
// routing in the DAW (for example Ableton Live) be checked before the full
// sequencer is built. See SPEC.md, section 2 and 14.
class VimaProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numTestChannels = 4;

    struct TestChannel
    {
        int midiChannel;      // 1-16
        int note;             // MIDI note number
        const char* rhythm;   // human readable description
    };

    static const std::array<TestChannel, numTestChannels>& testChannels();

    VimaProcessor();
    ~VimaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Shared with the editor (message thread reads / toggles, audio thread uses)
    std::array<std::atomic<bool>, numTestChannels> notesEnabled;
    std::array<std::atomic<bool>, numTestChannels> ccEnabled;
    std::array<std::atomic<int>, numTestChannels> noteCounter;   // increments on every note-on
    std::array<std::atomic<int>, numTestChannels> ccValue;       // last CC value sent

    std::atomic<bool> hostPlaying { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostPpq { 0.0 };

private:
    struct PendingOff
    {
        double ppq;
        int channel;
        int note;
    };

    void flushNotes (juce::MidiBuffer& midi, int samplePos);

    double sampleRate = 44100.0;
    bool wasPlaying = false;
    double lastPpqEnd = -1.0;
    std::vector<PendingOff> pending;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaProcessor)
};
