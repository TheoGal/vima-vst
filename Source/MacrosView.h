#pragma once

// Macros page (SPEC.md 13.1, 13.2): 4 pages of 32 macro knobs, the mapping editor for the
// selected macro and LEARN. Mapping sets load / save from the FILE menu (13.3).

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
    juce::OwnedArray<Pill> pageBtns, kindBtns, targetBtns, trackBtns;
    juce::String conflict;   // "ALREADY ON M003" after a refused change
    int conflictMacro = -1;
    juce::OwnedArray<MacroCell> cells;
    std::vector<MacroTarget> shownTargets;
    Pill learnBtn, clearBtn;
    Knob patternKnob, stepKnob;
    juce::Rectangle<int> left, aside, pagesLabel, gridArea, learnBanner, asideHead, kindLabel, targetLabel, trackLabel,
                         patternLabel_, stepLabel, footer, emptyText, patternText;
};
} // namespace vima
