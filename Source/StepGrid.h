#pragma once

// The 2 x 16 step grid with an LED bar above each row (playhead + selection, SPEC.md 8, 12.1).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class LaneSlider : public Knob
{
public:
    std::function<juce::Colour()> barColour;
    void paint (juce::Graphics&) override;
};

class StepCell : public juce::Component
{
public:
    StepCell (Context&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();   // visibility and dimming

private:
    Context& ctx;
    const int index;
    Pill toggle;
    Knob pitch, octave, cc;
    LaneSlider slider;
    juce::Rectangle<int> labelArea;
    Step stepCopy() const;
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void refreshCells();
    void repaintLeds();

    static constexpr int ledHeight = 16, ledGap = 3, cellHeight = 182, rowGap = 8;

private:
    int ledIndexAt (juce::Point<int>, bool clampToRow) const;
    juce::Rectangle<int> ledBounds (int step) const;
    juce::Rectangle<int> cellBounds (int step) const;

    Context& ctx;
    juce::OwnedArray<StepCell> cells;
    int dragFrom = -1;
    std::uint32_t dragBase = 0;   // selection kept while Ctrl-dragging (added to)
};
} // namespace vima
