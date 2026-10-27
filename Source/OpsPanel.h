#pragma once

// Operations panel (SPEC.md 12.6): selection with COPY / PASTE / CLEAR, param chips, ranges,
// RANDOMIZE and SMOOTH, SHIFT ALL.

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class RangeBar : public juce::Component
{
public:
    RangeBar (Context&, Param);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    ParamRange get() const;
    void set (ParamRange);
    void axis (int& lo, int& hi) const;
    juce::String valueText (int) const;
    juce::Rectangle<float> barArea() const;
    int valueAt (float x) const;
    std::vector<int> notePositions() const;
    bool armed() const;
    void paintBar (juce::Graphics&);

    Context& ctx;
    Param param;
    int dragging = 0;   // 1 = min handle, 2 = max handle
};

class ShiftKnob : public juce::Component
{
public:
    explicit ShiftKnob (Context&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    Context& ctx;
    ShiftGesture gesture;
    int startY = 0;
    bool active = false;
};

class OpsPanel : public juce::Component, private juce::Timer
{
public:
    explicit OpsPanel (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();

private:
    juce::String opLabel (const juce::String& op);
    void timerCallback() override;
    void smoothOnce();

    Context& ctx;
    Pill copyBtn, pasteBtn, clearBtn, randomBtn, smoothBtn;
    juce::OwnedArray<Pill> shapeBtns;   // SINE, TRIANGLE, RAMP, SQUARE, ROTATE <, ROTATE >, INVERT
    juce::OwnedArray<Pill> paramBtns, shiftChips;
    juce::OwnedArray<RangeBar> ranges;
    ShiftKnob shiftKnob;
    juce::Rectangle<int> selCol, paramsCol, rangesCol, buttonsCol, shiftCol;
    bool repeating = false;
};
} // namespace vima
