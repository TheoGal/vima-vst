#pragma once

// The 2 x 16 step grid with an LED bar above each row (playhead + selection, SPEC.md 8, 12.1).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
// Step slider: a click sets the value at the click position; dragging across neighbouring
// sliders draws values over several steps (SPEC.md 12.3). Wheel, double-click typing and
// right-click / Alt + click reset work as on knobs.
class LaneSlider : public Knob
{
public:
    std::function<juce::Colour()> barColour;
    std::function<void (juce::Point<int> screenPos, bool start)> onDraw;   // set the slider under this point
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int valueAtScreenY (int screenY) const;

private:
    bool drawing = false;
};

class StepCell : public juce::Component
{
public:
    StepCell (Context&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();   // visibility and dimming
    void drawAt (juce::Point<int> screenPos);   // slider drawing: set this step from the point's height

private:
    Context& ctx;
    const int index;
    Pill toggle;
    Knob pitch, octave;
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
    void drawSliders (juce::Point<int> screenPos, bool start, int row);   // slider drawing: the steps under the mouse path

    static constexpr int ledHeight = 16, ledGap = 3, cellHeight = 182, rowGap = 8;

private:
    int ledIndexAt (juce::Point<int>, bool clampToRow) const;
    juce::Rectangle<int> ledBounds (int step) const;
    juce::Rectangle<int> cellBounds (int step) const;

    Context& ctx;
    juce::OwnedArray<StepCell> cells;
    int dragFrom = -1;
    juce::Point<int> lastDraw;
    int drawRow = 0;   // drawing stays in the row where it started
    void drawOne (juce::Point<int> screenPos);
    std::uint32_t dragBase = 0;   // selection kept while Ctrl-dragging (added to)
};
} // namespace vima
