#pragma once

// Track parameter row (SPEC.md 12.4): LENGTH, DIVISION, DIRECTION, SWING, MIDI CH (track-wide,
// neutral), SCALE and NOTE OUT (note mode, green), CC OUT with CC number (CC mode, cyan),
// SLIDER lane switch (note mode).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class KnobTile : public juce::Component
{
public:
    KnobTile (juce::String title, juce::String unit);
    Knob knob;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title, unit;
};

class LabelledTile : public juce::Component
{
public:
    explicit LabelledTile (juce::String t) : title (std::move (t)) {}
    void paint (juce::Graphics&) override;
    juce::String title;
    std::function<juce::String()> rightText;
    std::function<juce::Colour()> borderColour;
};

class ParamRow : public juce::Component
{
public:
    explicit ParamRow (Context&);
    void resized() override;
    void paint (juce::Graphics&) override {}
    void updateVisibility();

private:
    void openDivision();
    void openScale();

    Context& ctx;
    KnobTile lengthTile { "LENGTH", "steps" }, swingTile { "SWING", "" }, channelTile { "MIDI CH", "" }, ccNumberTile { "CC NUM", "" };
    LabelledTile divisionTile { "DIVISION" }, directionTile { "DIRECTION" }, scaleTile { "SCALE" },
                 noteOutTile { "NOTE OUT" }, ccOutTile { "CC OUT" }, sliderTile { "SLIDER" };
    Pill divisionBtn, scaleNameBtn, scaleOnBtn, noteOutBtn, ccOutBtn;
    juce::OwnedArray<Pill> directionBtns, sliderBtns;
};
} // namespace vima
