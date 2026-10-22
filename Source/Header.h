#pragma once

// Top bar: name, track dropdown, NOTES/CC switch with output LEDs, pattern quick picker,
// HOST box (start / stop, BPM), FILE menu (presets), UNDO, MACROS, SETTINGS (SPEC.md 12, layout item 1; 7.4).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class Header : public juce::Component
{
public:
    explicit Header (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showTrackMenu();
    void showFileMenu();
    void chooseFile (bool save);
    std::unique_ptr<juce::FileChooser> chooser;
    Context& ctx;
    Pill trackBox, notesBtn, ccBtn, stepsBtn, prevBtn, nextBtn, patternBtn, undoBtn, macrosBtn, settingsBtn, transportBtn, fileBtn;
    juce::Rectangle<int> modeArea, patternArea, hostArea, nameArea;
};
} // namespace vima
