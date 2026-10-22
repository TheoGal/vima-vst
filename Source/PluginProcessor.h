#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "Engine.h"
#include "Operations.h"
#include "MacroParams.h"

// VIMA: instrument plugin that outputs MIDI (notes and CC) from a 16-track step sequencer.
//
// Threading: the Project is shared between the message thread (editing) and the audio
// thread (playback), guarded by a SpinLock that is only ever held briefly. All edits go
// through edit(), which also records undo.
class VimaProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    VimaProcessor();
    ~VimaProcessor() override { stopTimer(); }

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

    // ---- shared state ---------------------------------------------------------------
    using Lock = juce::SpinLock::ScopedLockType;
    juce::SpinLock lock;
    vima::Project project;      // access only while holding `lock`
    vima::Engine engine;
    vima::UndoHistory undo;     // message thread only

    // Read a value from the project under the lock
    template <typename Fn> auto read (Fn&& fn)
    {
        const Lock sl (lock);
        return fn (static_cast<const vima::Project&> (project));
    }

    // Change the project; if undoPattern >= 0, that pattern is saved for undo first
    template <typename Fn> void edit (int undoPattern, Fn&& fn)
    {
        const Lock sl (lock);
        if (undoPattern >= 0)
            undo.push (project, undoPattern);
        fn (project);
        ++revision;
    }

    // Change the project; if withUndo, the current pattern is saved for undo first
    template <typename Fn> void editCurrent (bool withUndo, Fn&& fn)
    {
        const Lock sl (lock);
        if (withUndo)
            undo.push (project, project.currentPattern);
        fn (project);
        ++revision;
    }

    bool performUndo();
    bool canUndo() { const Lock sl (lock); return undo.canUndo(); }
    void queuePattern (int index);   // follows the quantize setting

    std::atomic<int> revision { 0 };   // bumped on every edit, for UI refresh
    std::atomic<bool> hostPlaying { false };

    // Transport button (SPEC.md 3): stop VIMA while the host plays, or run it on its own
    // (from step 1, at the host tempo) while the host is stopped.
    void toggleTransport();
    std::atomic<bool> userStopped { false };   // stopped by the button while the host plays
    std::atomic<bool> freeRun { false };       // started by the button while the host is stopped
    std::atomic<bool> freeRunRestart { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<float> uiScale { 1.0f };   // editor size (1.0 = 1200 x 720), saved with the project

    vima::MacroParams macros { *this };      // the 128 automatable macros (SPEC 13.1)
    void syncParamsToHost();                 // message thread; also run by the timer
    std::atomic<int> shownTrack { 0 };       // track shown in the editor (macro targets "SHOWN")

private:
    void timerCallback() override { syncParamsToHost(); }
    double sampleRate = 44100.0;
    double freePpq = 0.0;              // audio thread: position while running on its own
    bool lastHostPlaying = false;      // audio thread
    juce::MidiBuffer outBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaProcessor)
};
