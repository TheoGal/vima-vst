#pragma once

// Shared UI state and helpers for all editor components.

#include "PluginProcessor.h"
#include "Theme.h"
#include <vector>

namespace vima
{
enum class SliderLane { velocity, gate, probability };

struct UiState
{
    int track = 0;               // selected track (0..15)
    bool ccMode = false;         // NOTES / CC view
    SliderLane slider = SliderLane::velocity;

    std::uint32_t selection = 0;     // selected steps (bit n = step n); 0 = none = all steps

    std::vector<ClipStep> clip;  // copied steps (with their spacing)
    int clipFrom = -1;           // track the steps were copied from
    bool pasteArmed = false;
    int pasteHover = -1;

    bool patternsView = false;
    int viewBank = 0;            // bank shown in the patterns view
    int selectedPattern = 0;     // selected slot in the patterns view
    int pickerBank = 0;          // bank shown in the header quick picker
    int patternClipKind = 0;     // 0 none, 1 pattern, 2 track
    int patternClipSource = -1;  // pattern index copied from
    int patternClipTrack = -1;
    Pattern patternClip;
    TrackPattern trackClip;
    int patternsViewTrack = 0;   // track for COPY TRACK

    Lane shiftLane = Lane::note;   // SHIFT ALL lane in note mode (CC mode always shifts CC)
    int shiftAmount = 0;           // running offset while the shift knob is dragged

    bool settingsOpen = false;
    bool blink = false;
};

class Context
{
public:
    explicit Context (VimaProcessor& p) : proc (p) {}

    VimaProcessor& proc;
    UiState ui;
    juce::Component* root = nullptr;
    std::function<void()> refresh;        // repaint / relayout everything
    std::function<void (bool)> showPatterns;
    std::function<void()> openSettings;

    int currentPattern() { return proc.read ([] (const Project& p) { return p.currentPattern; }); }

    TrackPattern trackCopy (int track = -1)
    {
        const int t = track < 0 ? ui.track : track;
        return proc.read ([t] (const Project& p) { return p.patterns[(size_t) p.currentPattern].tracks[(size_t) t]; });
    }
    TrackRouting routingCopy (int track = -1)
    {
        const int t = track < 0 ? ui.track : track;
        return proc.read ([t] (const Project& p) { return p.routing[(size_t) t]; });
    }
    Globals globalsCopy() { return proc.read ([] (const Project& p) { return p.globals; }); }

    template <typename Fn> void edit (bool withUndo, Fn&& fn)
    {
        proc.editCurrent (withUndo, std::forward<Fn> (fn));
        changed();
    }

    // Save the current pattern for undo (start of a drag gesture)
    void beginGesture() { proc.editCurrent (true, [] (Project&) {}); }

    Scope scope()
    {
        const int t = ui.track;
        const auto sel = ui.selection;
        return proc.read ([=] (const Project& p) { return makeScope (p, p.currentPattern, t, sel); });
    }
    bool hasSelection()
    {
        const auto len = trackCopy().length;
        return (ui.selection & rangeMask (0, len - 1)) != 0;
    }

    TrackPattern& liveTrack (Project& p) { return p.patterns[(size_t) p.currentPattern].tracks[(size_t) ui.track]; }

    void changed() { if (refresh) refresh(); }

    juce::Colour modeAccent() const { return ui.ccMode ? Theme::current().cyan : Theme::current().green; }
};
} // namespace vima
