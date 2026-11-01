#pragma once

// FILE menu actions (SPEC.md 13.3): presets, single patterns and macro mapping sets. Shared by
// the header's FILE menu and the patterns view's slot right-click menu. Message thread only.

#include "Context.h"

namespace vima
{
class FileActions
{
public:
    FileActions (Context&, juce::Component& owner);

    // The three sections of the FILE menu. Item ids: 1-4 preset, 11-13 pattern, 21-24 macros.
    void addMenu (juce::PopupMenu&, int patternIndex);
    void perform (int itemId, int patternIndex);

    void newPreset();
    void loadPreset();
    void savePreset();
    void savePresetAs();

    void newPattern (int index);      // asks first; undoable
    void loadPattern (int index);     // asks before replacing a pattern that has steps on; undoable
    void savePatternAs (int index);

    void newMacros();                 // asks first
    void loadMacros();                // asks before replacing existing mappings
    void saveMacros();
    void saveMacrosAs();

private:
    Context& ctx;
    juce::Component& owner;
    std::unique_ptr<juce::FileChooser> chooser;

    void choose (bool save, const juce::String& title, const juce::File& start, const char* extension,
                 std::function<void (const juce::File&)> done);
    void confirm (const juce::String& title, const juce::String& text, std::function<void()> ok);
};
} // namespace vima
