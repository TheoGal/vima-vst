#pragma once

// Parameters exposed to the host for automation (SPEC.md 13.1).
// Global: Pattern, Chain On. Per track: Notes Out, CC Out, Length, Division, Direction, Swing.
//
// Two-way sync with the Project:
//  - host -> project: on the audio thread (lock held), a parameter whose value changed since it
//    was last applied is written into the project (Pattern queues a pattern change).
//  - project -> host: on the message thread, a project value that changed (from the UI, a
//    pattern switch, the chain, a loaded state) is sent to the host.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "Engine.h"
#include <vector>

namespace vima
{
class HostParams
{
public:
    explicit HostParams (juce::AudioProcessor&);

    // Audio thread, project lock held. Returns true if the project changed.
    bool applyFromHost (Project&, Engine&);

    // Under the project lock: the plain value of every parameter, as the project has it now.
    void capture (const Project&, std::vector<int>& values) const;

    // Message thread, lock NOT held: send values that changed in the project to the host.
    void pushToHost (const std::vector<int>& values);

    juce::RangedAudioParameter* find (const juce::String& id) const;

    int size() const { return (int) entries.size(); }

private:
    enum class Kind { pattern, chainOn, noteOut, ccOut, length, division, direction, swing };

    struct Entry
    {
        juce::RangedAudioParameter* param = nullptr;
        Kind kind = Kind::pattern;
        int track = -1;
        float lastApplied = 0.0f;       // audio thread only
        int lastPushed = -1000000;      // message thread only (plain value)
    };

    static int projectValue (const Project&, const Entry&);
    std::vector<Entry> entries;
};
} // namespace vima
