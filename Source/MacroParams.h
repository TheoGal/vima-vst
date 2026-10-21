#pragma once

// The 128 macros exposed to the host (SPEC.md 13.1). They are the only automatable parameters.
//
// Two-way sync with the Project:
//  - host -> project (audio thread, project lock held): a macro whose value changed since it was
//    last applied writes its mapped target. Values VIMA itself sent are not applied back.
//  - project -> host (message thread): when a mapped target changes (UI, pattern switch, chain,
//    shown track, remapping), the macro is moved to match, as a short gesture.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "Engine.h"
#include <array>
#include <atomic>

namespace vima
{
class MacroParameter : public juce::AudioParameterFloat
{
public:
    MacroParameter (int index, std::function<juce::String (int, float)> text);
    juce::String getText (float normalised, int maximumLength) const override;

private:
    const int index;
    std::function<juce::String (int, float)> textFn;
};

class MacroParams
{
public:
    explicit MacroParams (juce::AudioProcessor&);

    // Audio thread, project lock held. Returns true if the project changed.
    bool applyFromHost (Project&, Engine&, int shownTrack);

    // Message thread, project lock held: take the mappings and target values.
    void capture (const Project&, int shownTrack);
    // Message thread, lock NOT held: move macros whose targets changed.
    void pushToHost();

    // Set a macro from the plugin's own UI (sent to the host and applied on the next block)
    void beginGesture (int index);
    void setFromUi (int index, float normalised);
    void endGesture (int index);

    juce::RangedAudioParameter* param (int index) const { return entries[(size_t) index].param; }

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

    // copy used by getText (any thread)
    juce::SpinLock textLock;
    std::array<MacroMap, numMacros> textMaps {};
    Globals textGlobals;
    juce::String textFor (int index, float normalised);
};
} // namespace vima
