#pragma once

// Top bar: name, track dropdown, NOTES/CC switch with output LEDs, pattern quick picker,
// host status, UNDO, MACROS, SETTINGS (SPEC.md 12, layout item 1; 7.4).

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
    Context& ctx;
    Pill trackBox, notesBtn, ccBtn, stepsBtn, prevBtn, nextBtn, patternBtn, undoBtn, macrosBtn, settingsBtn;
    juce::Rectangle<int> modeArea, patternArea, hostArea, nameArea;
};
} // namespace vima
