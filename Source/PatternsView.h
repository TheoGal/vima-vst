#pragma once

// Patterns view and chain (SPEC.md 7.1, 7.1.1, 7.2).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class PatternSlot : public juce::Component
{
public:
    PatternSlot (Context&, int slot);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Context& ctx;
    const int slot;
};

class ChainRow : public juce::Component
{
public:
    ChainRow (Context&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    const int index;
    Pill minus, plus, remove;
};

class PatternsView : public juce::Component
{
public:
    explicit PatternsView (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();

private:
    void showTrackMenu();
    Context& ctx;
    juce::OwnedArray<Pill> bankBtns;
    juce::OwnedArray<PatternSlot> slots;
    Pill copyBtn, pasteBtn, clearBtn, trackBox, copyTrackBtn;
    Pill chainOnBtn, loopBtn, addBtn, clearChainBtn;
    juce::OwnedArray<ChainRow> chainRows;
    juce::Rectangle<int> left, aside, banksLabel, slotsLabel, slotsArea, selectedBar, chainHeader, columnHeader, chainList, footer;
    int lastChainSize = -1;
};
} // namespace vima
