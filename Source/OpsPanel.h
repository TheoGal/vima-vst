#pragma once

// Operations panel (SPEC.md 12.4): selection with COPY / PASTE / CLEAR, lane chips, ranges,
// RANDOMIZE and SMOOTH, SHIFT ALL.

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class RangeBar : public juce::Component
{
public:
    RangeBar (Context&, Lane);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    LaneRange get() const;
    void set (LaneRange);
    void axis (int& lo, int& hi) const;
    juce::String valueText (int) const;
    juce::Rectangle<float> barArea() const;
    int valueAt (float x) const;

    Context& ctx;
    Lane lane;
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
    void timerCallback() override;
    void smoothOnce();

    Context& ctx;
    Pill copyBtn, pasteBtn, clearBtn, randomBtn, smoothBtn;
    juce::OwnedArray<Pill> laneChips, shiftChips;
    juce::OwnedArray<RangeBar> ranges;
    ShiftKnob shiftKnob;
    juce::Rectangle<int> selCol, lanesCol, rangesCol, buttonsCol, shiftCol;
    bool repeating = false;
};
} // namespace vima
