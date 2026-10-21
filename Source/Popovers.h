#pragma once

// Popover contents: DIVISION, SCALE and the header's quick pattern picker.

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class PopoverPanel : public juce::Component
{
public:
    void paint (juce::Graphics& g) override { paintPanel (g, getLocalBounds().toFloat().reduced (0.5f)); }

protected:
    Pill& addPill();
    juce::OwnedArray<Pill> pills;
    struct Label { juce::String text; juce::Rectangle<int> area; };
    juce::Array<Label> labels;
    void paintOverChildren (juce::Graphics&) override;
};

class DivisionPopover : public PopoverPanel
{
public:
    explicit DivisionPopover (Context&);
    static juce::Point<int> size() { return { 292, 128 }; }
    void resized() override;

private:
    Context& ctx;
};

class ScalePopover : public PopoverPanel
{
public:
    explicit ScalePopover (Context&);
    static juce::Point<int> size() { return { 316, 272 }; }
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    Context& ctx;
};

class PatternPickerPopover : public PopoverPanel, private juce::Timer
{
public:
    explicit PatternPickerPopover (Context&);
    static juce::Point<int> size() { return { 316, 306 }; }
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    Context& ctx;
};
} // namespace vima
