#pragma once

// Shared UI state and helpers for all editor components.

#include "PluginProcessor.h"
#include "Theme.h"
#include "Macros.h"
#include <vector>

namespace vima
{
enum class SliderParam { velocity, gate, probability };

struct UiState
{
    int track = 0;               // selected track (0..7)
    bool ccMode = false;         // NOTES / CC view
    SliderParam slider = SliderParam::velocity;

    std::uint32_t selection = 0;     // selected steps (bit n = step n); 0 = none = all steps

    // Copy / paste (SPEC.md 9.1): while the clipboard holds steps ("COPIED"), there is no
    // selection; an LED click only picks where the paste lands.
    std::vector<ClipStep> clip;  // copied steps (with their spacing); empty = nothing copied
    std::vector<Param> clipParams; // params armed when copying: only these are pasted
    int clipSpan = 0;            // first to last copied step
    int pasteStart = -1;         // landing step of the paste (-1 = not picked yet)

    bool patternsView = false;
    int viewPage = 0;            // page shown in the patterns view
    int selectedPattern = 0;     // selected slot in the patterns view
    int pickerPage = 0;          // page shown in the header quick picker
    int patternClipKind = 0;     // 0 none, 1 pattern, 2 track
    int patternClipSource = -1;  // pattern index copied from
    int patternClipTrack = -1;
    Pattern patternClip;
    TrackPattern trackClip;

    // Slot drag and drop (SPEC.md 7.1.3)
    int slotDragSource = -1;     // pattern being dragged, -1 = none
    int slotDropTarget = -1;     // allowed (empty) slot under the mouse, -1 = none

    Param shiftParam = Param::pitch;   // SHIFT param in note mode (CC mode always shifts CC)
    int shiftAmount = 0;           // running offset while the shift knob is dragged

    bool macrosView = false;
    int macroPage = 0;             // page shown on the Macros page (32 macros each)
    int selectedMacro = 0;
    bool macroLearn = false;       // LEARN: the next touched control is mapped to selectedMacro

    // Pattern edit mode (SPEC.md 7.5): the step view shows editPattern instead of the playing
    // pattern; the header picker only chooses it. A working state: not saved, off on open.
    bool editMode = false;
    int editPattern = 0;

    bool settingsOpen = false;
    bool blink = false;
};

// LEARN: controls carry their macro target as component properties (SPEC.md 13.1).
// "learnTarget" = (int) MacroTarget, or -1 for the step slider (its current param);
// "learnStep" = step index, on the step cell.
inline void setLearnTarget (juce::Component& c, int target) { c.getProperties().set ("learnTarget", target); }
inline void setLearnTarget (juce::Component& c, MacroTarget t) { setLearnTarget (c, (int) t); }

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
    std::function<void (bool)> showMacros;

    // The mapping a control on screen would get: the track and pattern on screen, fixed
    MacroMap controlMap (MacroTarget target, int step)
    {
        const int t = ui.track;
        return proc.read ([this, target, step, t] (const Project& p) { return defaultMacroMap (target, t, shownIn (p), step, p.globals); });
    }

    // The macro mapped to a control on screen, or -1
    int macroFor (MacroTarget target, int step)
    {
        const auto want = controlMap (target, step);
        return proc.read ([&want] (const Project& p)
        {
            for (int i = 0; i < numMacros; ++i)
                if (sameControl (p.macros[(size_t) i], want)) return i;
            return -1;
        });
    }

    // The next unmapped macro from `from` on (wrapping), or -1 when all 128 are mapped
    int nextFreeMacro (int from)
    {
        return proc.read ([from] (const Project& p)
        {
            for (int k = 0; k < numMacros; ++k)
            {
                const int i = (from + k) % numMacros;
                if (! p.macros[(size_t) i].mapped()) return i;
            }
            return -1;
        });
    }

    void selectMacro (int i)
    {
        ui.selectedMacro = juce::jlimit (0, numMacros - 1, i);
        ui.macroPage = ui.selectedMacro / macrosPerPage;
    }

    // LEARN (SPEC.md 13.2): map the listening macro to a control on screen, then move on to the
    // next free macro. A control that is already mapped is left alone. LEARN switches off when
    // no macro is free. Returns false when not learning.
    bool learn (MacroTarget target, int step)
    {
        if (! ui.macroLearn || target == MacroTarget::none)
            return false;
        if (macroFor (target, step) >= 0)
            return true;   // already mapped: no second mapping
        int i = juce::jlimit (0, numMacros - 1, ui.selectedMacro);
        if (macroCopy (i).mapped()) i = nextFreeMacro (i);
        if (i < 0) { ui.macroLearn = false; changed(); return true; }
        const auto m = controlMap (target, step);
        proc.edit (-1, [i, m] (Project& p) { p.macros[(size_t) i] = m; });
        const int next = nextFreeMacro (i);
        if (next < 0) ui.macroLearn = false;
        else selectMacro (next);
        changed();
        return true;
    }

    // LEARN: right-click on a mapped control frees it
    bool unlearn (MacroTarget target, int step)
    {
        const int i = macroFor (target, step);
        if (! ui.macroLearn || i < 0)
            return false;
        proc.edit (-1, [i] (Project& p) { p.macros[(size_t) i] = MacroMap(); });
        changed();
        return true;
    }
    MacroMap macroCopy (int i) { return proc.read ([i] (const Project& p) { return p.macros[(size_t) i]; }); }

    int currentPattern() { return proc.read ([] (const Project& p) { return p.currentPattern; }); }   // the playing pattern

    // The pattern on screen: the playing one, or the edited one in edit mode (SPEC.md 7.5)
    int shownIn (const Project& p) const { return ui.editMode ? juce::jlimit (0, numPatterns - 1, ui.editPattern) : p.currentPattern; }
    int shown() { return proc.read ([this] (const Project& p) { return shownIn (p); }); }
    bool showsPlaying() { return proc.read ([this] (const Project& p) { return shownIn (p) == p.currentPattern; }); }

    // Edit mode on / off. Off: the view goes back to the playing pattern.
    void setEditMode (bool on)
    {
        ui.editMode = on;
        if (on) ui.editPattern = currentPattern();
        changed();
    }
    void editPatternTo (int index)
    {
        ui.editPattern = juce::jlimit (0, numPatterns - 1, index);
        changed();
    }

    // The last beat before a scheduled switch (SPEC.md 7.5): two pulses on the 8th notes.
    // True while the blinking text is dimmed.
    bool switchPulseDim()
    {
        const double b = proc.engine.beatsToSwitch.load();
        if (b <= 0.0 || b > 1.0) return false;
        const double e = 1.0 - b;
        return std::fmod (e * 2.0, 1.0) < 0.5;
    }

    TrackPattern trackCopy (int track = -1)
    {
        const int t = track < 0 ? ui.track : track;
        return proc.read ([this, t] (const Project& p) { return p.patterns[(size_t) shownIn (p)].tracks[(size_t) t]; });
    }
    TrackRouting routingCopy (int track = -1)
    {
        const int t = track < 0 ? ui.track : track;
        return proc.read ([t] (const Project& p) { return p.routing[(size_t) t]; });
    }
    Globals globalsCopy() { return proc.read ([] (const Project& p) { return p.globals; }); }

    template <typename Fn> void edit (bool withUndo, Fn&& fn, const juce::String& undoLabel = {})
    {
        proc.editShown (ui.editMode ? ui.editPattern : -1, withUndo, std::forward<Fn> (fn), undoLabel);
        changed();
    }

    // Save the current pattern for undo (start of a drag gesture), with what it is
    void beginGesture (const juce::String& undoLabel = {}) { proc.editShown (ui.editMode ? ui.editPattern : -1, true, [] (Project&) {}, undoLabel); }

    // Undo labels (SPEC.md 9): " · TR03 · PAT23" for the track and pattern on screen
    juce::String where()
    {
        return juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 TR")) + juce::String (ui.track + 1).paddedLeft ('0', 2)
             + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 PAT")) + patternLabel (shown());
    }
    static juce::String dot() { return juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")); }
    // the steps in scope: "ALL STEPS", "STEP 05", "STEPS 3-9", "4 STEPS"
    juce::String scopeText()
    {
        const auto sel = ui.selection;
        if (sel == 0) return "ALL STEPS";
        int first = -1, last = -1, n = 0;
        for (int i = 0; i < 32; ++i)
            if ((sel >> i) & 1u) { if (first < 0) first = i; last = i; ++n; }
        if (n == 1) return "STEP " + juce::String (first + 1).paddedLeft ('0', 2);
        if (n == last - first + 1) return "STEPS " + juce::String (first + 1) + "-" + juce::String (last + 1);
        return juce::String (n) + " STEPS";
    }

    Scope scope()
    {
        const int t = ui.track;
        const auto sel = ui.selection;
        return proc.read ([=] (const Project& p) { return makeScope (p, shownIn (p), t, sel); });
    }
    bool hasSelection()
    {
        const auto len = trackCopy().length;
        return (ui.selection & rangeMask (0, len - 1)) != 0;
    }

    TrackPattern& liveTrack (Project& p) { return p.patterns[(size_t) shownIn (p)].tracks[(size_t) ui.track]; }

    void changed() { if (refresh) refresh(); }

    juce::Colour modeAccent() const { return ui.ccMode ? Theme::current().cyan : Theme::current().green; }
};
} // namespace vima
