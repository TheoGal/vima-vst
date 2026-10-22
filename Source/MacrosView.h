#pragma once

// Macros page (SPEC.md 13.1, 13.2): 4 banks of 32 macro knobs, the mapping editor for the
// selected macro, LEARN, and SAVE / LOAD of mapping sets (13.3).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class MacroCell : public juce::Component
{
public:
    MacroCell (Context&, int slot);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int macroIndex() const;

private:
    void setNormalised (float);
    Context& ctx;
    const int slot;
    float dragStart = 0.0f;
    int dragStartY = 0;
    bool dragging = false;
};

class MacrosView : public juce::Component
{
public:
    explicit MacrosView (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();

    static juce::Colour targetColour (MacroTarget);

private:
    void editMap (std::function<void (MacroMap&, const Globals&)>);
    void pickKind (MacroKind);
    void pickTarget (MacroTarget);

    Context& ctx;
    juce::OwnedArray<Pill> bankBtns, kindBtns, targetBtns, trackBtns;
    juce::String conflict;   // "ALREADY ON M003" after a refused change
    int conflictMacro = -1;
    juce::OwnedArray<MacroCell> cells;
    std::vector<MacroTarget> shownTargets;
    Pill learnBtn, clearBtn, saveSetBtn, loadSetBtn;
    std::unique_ptr<juce::FileChooser> chooser;
    void chooseSetFile (bool save);
    Knob patternKnob, stepKnob, minKnob, maxKnob;
    juce::Rectangle<int> left, aside, banksLabel, gridArea, learnBanner, asideHead, kindLabel, targetLabel, trackLabel,
                         patternLabel_, stepLabel, rangeLabel, minLabel, maxLabel, footer, emptyText, patternText;
};
} // namespace vima
