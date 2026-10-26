#pragma once

// The 128 macros exposed to the host (SPEC.md 13.1). They are the only automatable parameters.
//
// Two-way sync with the Project:
//  - host -> project (audio thread, project lock held): a macro whose value changed since it was
//    last applied writes its mapped target. Values VIMA itself sent are not applied back.
//  - project -> host (message thread): when a mapped target changes (UI, undo, preset,
//    remapping; pattern switches for the Pattern target), the macro is moved to match, as a
//    short gesture.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "Engine.h"
#include "Macros.h"
#include <array>
#include <atomic>

namespace vima
{
class MacroParameter : public juce::AudioParameterFloat
{
public:
    MacroParameter (int index, std::function<juce::String (int, float)> text, std::function<juce::String (int)> name);
    juce::String getText (float normalised, int maximumLength) const override;
    juce::String getName (int maximumLength) const override;   // follows the mapping (SPEC.md 13.1)

private:
    const int index;
    std::function<juce::String (int, float)> textFn;
    std::function<juce::String (int)> nameFn;
};

class MacroParams
{
public:
    explicit MacroParams (juce::AudioProcessor&);

    // Audio thread, project lock held. Returns true if the project changed.
    bool applyFromHost (Project&, Engine&);

    // Message thread, project lock held: take the mappings and target values.
    void capture (const Project&, int queuedPattern = -1);   // queued pattern: shown by Pattern macros
    // Message thread, lock NOT held: move macros whose targets changed.
    void pushToHost();

    // Set a macro from the plugin's own UI (sent to the host and applied on the next block)
    void beginGesture (int index);
    void setFromUi (int index, float normalised);
    void endGesture (int index);

    juce::RangedAudioParameter* param (int index) const { return entries[(size_t) index].param; }

    // Host name of a macro: "Vel St05 Tr03 Pat23" when mapped, "Macro 1" when not (any thread)
    juce::String nameFor (int index);

private:
    struct Entry
    {
        MacroParameter* param = nullptr;
        float lastApplied = 0.0f;           // audio thread only
        std::atomic<float> pushed { -1.0f };   // last value VIMA sent itself
        int lastPushed = -1000000;          // message thread only
        MacroMap lastMap;
    };
    std::array<Entry, numMacros> entries;

    // message thread snapshot from capture()
    std::array<MacroMap, numMacros> maps {};
    std::array<int, numMacros> values {};
    std::array<bool, numMacros> valid {};
    std::array<MacroNotes, numMacros> notes {};
    std::array<juce::String, numMacros> lastNames;   // names last announced to the host
    juce::AudioProcessor& processor;

    // copy used by getText (any thread)
    juce::SpinLock textLock;
    std::array<MacroMap, numMacros> textMaps {};
    std::array<MacroNotes, numMacros> textNotes {};
    Globals textGlobals;
    juce::String textFor (int index, float normalised);
};
} // namespace vima
