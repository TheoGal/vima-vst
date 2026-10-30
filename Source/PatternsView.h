#pragma once

// Patterns view and chain (SPEC.md 7.1, 7.1.1, 7.2).

#include "Context.h"
#include "Widgets.h"
#include "FileActions.h"

namespace vima
{
class PatternSlot : public juce::Component
{
public:
    PatternSlot (Context&, int slot);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Rectangle<int> playArea() const;   // the play button at the top center
    std::function<void (int pattern)> onMenu;   // right-click: LOAD PATTERN... / SAVE PATTERN AS...
    // drag and drop (SPEC.md 7.1.3): position in the parent view, copy = Ctrl / Cmd held
    std::function<void (int pattern, juce::Point<int>, bool copy)> onSlotDrag;
    std::function<void (int pattern, juce::Point<int>, bool copy)> onSlotDrop;

private:
    Context& ctx;
    const int slot;
    bool hoverPlay = false;
    bool slotDragging = false;
    int dragSource = -1;
};

class ChainRow : public juce::Component
{
public:
    ChainRow (Context&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Drag to reorder: the parent view tracks where the entry would land
    std::function<void (int index, int yInParent)> onDrag;
    std::function<void()> onDragEnd;
    bool dragging = false;

private:
    Context& ctx;
    const int index;
    Pill minus, plus, remove;
};

class PatternsView : public juce::Component, private juce::ScrollBar::Listener
{
public:
    explicit PatternsView (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshState();
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Pattern / track clipboard (SPEC.md 7.1.2), shared by the bottom bar and the slot menu
    void copyPattern (int index);
    void copyTrack (int index);          // the track in the TRACK dropdown
    void pasteTo (int dest, bool oneOff);   // oneOff (slot menu): clear the clipboard after, flash PASTE
    void cancelClip();

private:
    void showTrackMenu();
    juce::uint32 pasteFlashUntil = 0;   // PASTE shows green until then (one-off paste confirmation)
    void showSlotMenu (int pattern);
    Context& ctx;
    FileActions fileActions { ctx, *this };
    juce::OwnedArray<Pill> pageBtns;
    juce::OwnedArray<PatternSlot> slots;
    Pill copyBtn, pasteBtn, clearBtn, patternBox, trackBox, copyTrackBtn;
    Pill loopBtn, addBtn, clearChainBtn;
    juce::OwnedArray<ChainRow> chainRows;
    int dragIndex = -1, dropIndex = -1;   // chain reordering
    void paintOverChildren (juce::Graphics&) override;
    int dropIndexAt (int y) const;
    // slot drag and drop (SPEC.md 7.1.3)
    void slotDragMoved (int pattern, juce::Point<int> pos, bool copy);
    void slotDropped (int pattern, juce::Point<int> pos, bool copy);
    int slotIndexAt (juce::Point<int> pos) const;   // pattern under pos in the grid, -1 = none
    juce::Point<int> slotDragPos;
    bool slotDragCopy = false;
    int chainInsertIndex = -1;   // where a dragged slot would be added to the chain
    juce::Rectangle<int> left, aside, pagesLabel, slotsLabel, slotsArea, selectedBar, chainHeader, columnHeader, chainList, footer;
    int lastChainSize = -1;
    // chain list scrolling (by whole entries): the first entry shown, and the scroll bar
    int chainFirst = 0;
    juce::ScrollBar chainScroll { true };
    int chainVisibleCount() const;
    void scrollChainTo (int first);
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
};
} // namespace vima
