#pragma once

// Top bar: name, track dropdown, NOTES/CC switch with output LEDs, pattern quick picker,
// HOST box (start / stop, BPM), FILE menu (preset, pattern, macros), UNDO, MACROS, SETTINGS (SPEC.md 12.2, layout item 1; 7.4).

#include "Context.h"
#include "Widgets.h"
#include "FileActions.h"

namespace vima
{
class Header : public juce::Component
{
public:
    explicit Header (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;
    // Step view: the TRACK box with the NOTES / CC switch, and the PATTERN box. Their frames
    // continue down around the track and pattern tiles of the parameter row (SPEC.md 12.2).
    juce::Rectangle<int> trackGroupTop() const { return trackBox.getBounds().getUnion (modeArea); }
    juce::Rectangle<int> patternGroupTop() const { return chainBtn.isVisible() ? patternArea.getUnion (chainBtn.getBounds()) : patternArea; }
    // The "βήμα" wordmark (SPEC.md 12.1), fitted into `area`, left-aligned and centred vertically
    static void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);
    // The square "β" logo (with its short green bar), fitted into `area`, centred (step view)
    static void drawIcon (juce::Graphics&, juce::Rectangle<float> area);

private:
    void showTrackMenu();
    void showFileMenu();
public:
    int fileMenuPattern();   // the pattern the FILE menu's PATTERN section acts on
private:
    void pickToEdit (int index);
    Context& ctx;
    FileActions fileActions { ctx, *this };
    Pill trackBox, notesBtn, ccBtn, stepsBtn, prevBtn, nextBtn, patternBtn, undoBtn, macrosBtn, settingsBtn, transportBtn, fileBtn, chainBtn, editBtn, playingBtn;
    juce::Rectangle<int> modeArea, patternArea, hostArea, nameArea;
};
} // namespace vima
