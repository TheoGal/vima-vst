#pragma once

// Track parameter row (SPEC.md 12.4), the lower half of the step view's top band (12.2):
// track group (MIDI CH; note mode NOTE OUT and SLIDER, CC mode CC OUT and CC NUM), pattern group
// (LENGTH, DIVISION, DIRECTION, SWING, USE SCALE in note mode) and the global SCALE on the right.

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
    bool frameLook = false;   // drawn like the track / pattern frames (SCALE)
};

class ParamRow : public juce::Component
{
public:
    explicit ParamRow (Context&);
    void resized() override;
    void paint (juce::Graphics&) override {}
    void updateVisibility();
    // The track group and the pattern group tiles (local coordinates), for the frames drawn
    // behind the header and this row (SPEC.md 12.2)
    juce::Rectangle<int> trackTiles, patternTiles;
    static constexpr int trackX = 108, patternX = 476;
    static constexpr int tileInset = 6;   // tiles sit 6 px inside the row; SCALE fills it, like the frames   // where the groups start (local x; the row sits at x 20)

private:
    void openDivision();
    void openScale();

    Context& ctx;
    KnobTile lengthTile { "LENGTH", "steps" }, swingTile { "SWING", "" }, channelTile { "MIDI CH", "" }, ccNumberTile { "CC NUM", "" };
    LabelledTile divisionTile { "DIVISION" }, directionTile { "DIRECTION" }, scaleTile { "SCALE" },
                 noteOutTile { "NOTE OUT" }, ccOutTile { "CC OUT" }, sliderTile { "SLIDER" }, useScaleTile { "USE SCALE" };
    Pill divisionBtn, scaleNameBtn, scaleOnBtn, noteOutBtn, ccOutBtn;
    juce::OwnedArray<Pill> directionBtns, sliderBtns;
};
} // namespace vima
