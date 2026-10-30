#include "PatternsView.h"

namespace vima
{
namespace
{
    juce::String pad2 (int v) { return juce::String (v).paddedLeft ('0', 2); }

    juce::String barsText (double ppq)
    {
        const double bars = ppq / 4.0;   // shown in 4/4 bars
        juce::String s = std::abs (bars - std::round (bars)) < 1.0e-6 ? juce::String ((int) std::round (bars)) : juce::String (bars, 1);
        return s + (std::abs (bars - 1.0) < 1.0e-6 ? " BAR" : " BARS");
    }
}

// ---------------------------------------------------------------------------------------
PatternSlot::PatternSlot (Context& c, int s) : ctx (c), slot (s)
{
    setLearnTarget (*this, MacroTarget::pattern);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<int> PatternSlot::playArea() const
{
    return juce::Rectangle<int> (30, 20).withCentre ({ getWidth() / 2, 18 });
}

void PatternSlot::mouseMove (const juce::MouseEvent& e)
{
    const bool h = playArea().contains (e.getPosition());
    if (h != hoverPlay) { hoverPlay = h; repaint(); }
}

void PatternSlot::mouseExit (const juce::MouseEvent&)
{
    if (hoverPlay) { hoverPlay = false; repaint(); }
}

void PatternSlot::mouseDown (const juce::MouseEvent&)
{
    slotDragging = false;
}

void PatternSlot::mouseDrag (const juce::MouseEvent& e)
{
    // drag and drop (SPEC.md 7.1.3): starts after a few pixels; not while COPIED / LEARN
    if (e.mods.isPopupMenu() || ctx.ui.patternClipKind != 0 || ctx.ui.macroLearn) return;
    if (! slotDragging && e.getDistanceFromDragStart() < 5) return;
    if (! slotDragging) dragSource = ctx.ui.viewPage * patternsPerPage + slot;   // the page may change while dragging
    slotDragging = true;
    const int idx = dragSource;
    const bool copy = e.mods.isCommandDown();
    setMouseCursor (copy ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::DraggingHandCursor);
    if (onSlotDrag) onSlotDrag (idx, e.getEventRelativeTo (getParentComponent()).getPosition(), copy);
}

void PatternSlot::mouseUp (const juce::MouseEvent& e)
{
    if (slotDragging)
    {
        slotDragging = false;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        if (onSlotDrop) onSlotDrop (dragSource, e.getEventRelativeTo (getParentComponent()).getPosition(), e.mods.isCommandDown());
        return;
    }
    if (! getLocalBounds().contains (e.getPosition())) return;
    const int idx = ctx.ui.viewPage * patternsPerPage + slot;
    if (e.mods.isPopupMenu())
    {
        // right-click: the slot's file menu (not while LEARN is on, where right-click frees mappings)
        if (! ctx.ui.macroLearn && onMenu != nullptr)
        {
            ctx.ui.selectedPattern = idx;
            ctx.changed();
            onMenu (idx);
        }
        return;
    }
    if (playArea().contains (e.getPosition()))
    {
        // play button: select and queue (switches at the quantize point)
        ctx.ui.selectedPattern = idx;
        if (idx != ctx.currentPattern())
            ctx.proc.queuePattern (idx);
    }
    else
        ctx.ui.selectedPattern = idx;    // select only (COPY / COPY TRACK, and where PASTE writes)
    ctx.changed();
}

void PatternSlot::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const int idx = ctx.ui.viewPage * patternsPerPage + slot;
    const auto pattern = ctx.proc.read ([idx] (const Project& p) { return p.patterns[(size_t) idx]; });
    const auto routing = ctx.proc.read ([] (const Project& p) { return p.routing; });
    const bool used = ! pattern.isEmpty();
    const bool playing = idx == ctx.currentPattern();
    const int q = ctx.proc.engine.queuedPattern.load();
    const bool queued = idx == q && ! playing;
    const bool sel = idx == ctx.ui.selectedPattern;   // also where PASTE writes (SPEC.md 7.1.2)
    const auto chain = ctx.globalsCopy().chain;
    bool inChain = false;
    for (const auto& e : chain) inChain = inChain || e.pattern == idx;

    // Cues (SPEC.md 7.1): playing = brightest background, stored = tile, empty = panel;
    // selected (also where PASTE writes) = thick white outline;
    // queued = blinking play button.
    auto b = getLocalBounds().toFloat();
    const auto fill = playing ? t.selected : (used ? t.tile : t.panel);
    // Drag and drop (SPEC.md 7.1.3): empty slots are drop targets (a mid outline), the one under
    // the mouse gets the selected outline; stored slots get none. The selection outline is hidden.
    const bool slotDrag = ctx.ui.slotDragSource >= 0;
    const bool target = slotDrag && idx == ctx.ui.slotDropTarget;
    const bool allowed = slotDrag && ! used && idx != ctx.ui.slotDragSource;
    const bool outlined = slotDrag ? target : sel;
    const auto border = outlined ? t.neutral : (allowed ? t.muted : t.border3);
    const float bw = outlined ? 2.0f : (allowed ? 1.5f : 1.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, 7.0f);
    g.setColour (border);
    g.drawRoundedRectangle (b.reduced (bw * 0.5f), 7.0f, bw);

    // play button (top center)
    {
        const auto pa = playArea().toFloat();
        auto pc = (playing || queued) ? t.neutral : (hoverPlay ? t.text : t.text2);
        if (queued && ctx.ui.blink) pc = pc.withAlpha (0.15f);   // queued: the play button blinks
        g.setColour (hoverPlay ? t.selected : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (pa, 5.0f);
        auto bc = (playing || queued) ? t.neutral : (hoverPlay ? t.dim2 : t.border3);
        if (queued && ctx.ui.blink) bc = bc.withAlpha (0.15f);
        g.setColour (bc);
        g.drawRoundedRectangle (pa.reduced (0.5f), 5.0f, 1.0f);
        juce::Path tri;
        const auto c = pa.getCentre();
        tri.addTriangle (c.x - 3.5f, c.y - 5.0f, c.x - 3.5f, c.y + 5.0f, c.x + 5.0f, c.y);
        g.setColour (pc);
        g.fillPath (tri);
    }

    auto inner = getLocalBounds().reduced (10, 8);
    auto top = inner.removeFromTop (20);
    g.setColour (used ? t.text : t.muted);
    g.setFont (Fonts::monoSemi (15.0f));
    g.drawText (patternLabel (idx), top, juce::Justification::centredLeft);
    const juce::String tag = playing ? "PLAYING" : queued ? "NEXT" : used ? "STORED" : "EMPTY";
    g.setFont (Fonts::label (9.0f));
    const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), tag) + 2;
    g.setColour (playing || queued ? t.text : t.muted);
    g.drawText (tag, top.removeFromRight (tw), juce::Justification::centredRight);

    // Thumbnail (7.1.1): one horizontal row per track, track 01 at the top, using the cell's
    // width and height. Each row has two thin lines, notes (green) above and CC (cyan) below:
    // grey over the track length (32 steps = full width, step 1 at the left) with a stripe for
    // every step that is on. A part whose output is off is dimmed; a part with no step on is
    // not shown (the other part then uses the full row).
    auto thumb = inner.withTrimmedTop (6).toFloat();
    const float gap = 2.0f;
    const float h = (thumb.getHeight() - gap * (numTracks - 1)) / (float) numTracks;
    const float stepW = thumb.getWidth() / (float) maxSteps;
    auto drawBar = [&g, &t, stepW] (juce::Rectangle<float> row, const TrackPattern& tp, bool cc, juce::Colour c)
    {
        const int len = juce::jlimit (minLength, maxSteps, (int) tp.length);
        if ((cc ? tp.ccOnStepCount() : tp.onStepCount()) == 0)
            return;   // a part with no step on is not shown
        g.setColour (t.ledOff);   // the track's length
        g.fillRect (row.withWidth (stepW * (float) len));
        g.setColour (c);
        for (int i = 0; i < len; ++i)
        {
            const auto& st = tp.steps[(size_t) i];
            if (cc ? st.ccOn : st.on)
                g.fillRect (row.getX() + stepW * (float) i, row.getY(), juce::jmax (1.0f, stepW - 1.0f), row.getHeight());
        }
    };
    for (int k = 0; k < numTracks; ++k)
    {
        const auto& tp = pattern.tracks[(size_t) k];
        const auto& r = routing[(size_t) k];
        auto row = thumb.withY (thumb.getY() + k * (h + gap)).withHeight (h);
        // notes above, CC below; a part whose output is off is dimmed. A part with no step on
        // is not shown, and the other part then takes the full row height.
        const auto noteColour = r.noteOut ? t.green : t.green.withMultipliedAlpha (0.35f);
        const auto ccColour = r.ccOut ? t.cyan : t.cyan.withMultipliedAlpha (0.35f);
        const bool hasNotes = tp.onStepCount() > 0, hasCC = tp.ccOnStepCount() > 0;
        if (hasNotes && hasCC)
        {
            const float half = (row.getHeight() - 1.0f) * 0.5f;
            drawBar (row.withHeight (half), tp, false, noteColour);
            drawBar (row.withTrimmedTop (row.getHeight() - half), tp, true, ccColour);
        }
        else if (hasNotes) drawBar (row, tp, false, noteColour);
        else if (hasCC)    drawBar (row, tp, true, ccColour);
    }
    if (inChain)
    {
        g.setColour (t.neutral);
        g.fillRect (b.withTop (b.getBottom() - 2.0f).reduced (4.0f, 0.0f));
    }
}

// ---------------------------------------------------------------------------------------
ChainRow::ChainRow (Context& c, int i) : ctx (c), index (i)
{
    auto small = [] { const auto& t = Theme::current(); return Pill::outline (t.border2, t.text2); };
    minus.look = plus.look = small;
    minus.radius = plus.radius = 4.0f;
    minus.text = [] { return juce::String ("-"); };
    plus.text = [] { return juce::String ("+"); };
    minus.font = plus.font = [] { return Fonts::mono (13.0f); };
    auto change = [this] (int d)
    {
        ctx.edit (false, [this, d] (Project& p)
        {
            auto& chain = p.globals.chain;
            if (index < chain.size())
                chain.getReference (index).repeats = (std::uint8_t) juce::jlimit (1, 99, chain[index].repeats + d);
        });
    };
    minus.onClick = [change] { change (-1); };
    plus.onClick = [change] { change (1); };
    remove.look = [] { const auto& t = Theme::current(); return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.muted, 0.0f, false }; };
    remove.text = [] { return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")); };
    remove.font = [] { return Fonts::mono (14.0f); };
    remove.onClick = [this]
    {
        ctx.edit (false, [this] (Project& p)
        {
            if (index < p.globals.chain.size())
            {
                p.globals.chain.remove (index);
                ctx.proc.engine.chainEntryRemoved (index);
            }
        });
    };
    addAndMakeVisible (minus);
    addAndMakeVisible (plus);
    addAndMakeVisible (remove);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle ("Chain entry " + juce::String (i + 1) + ", drag to reorder");
}

void ChainRow::mouseDown (const juce::MouseEvent&)
{
    dragging = false;
}

void ChainRow::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    if (onDrag) onDrag (index, e.getEventRelativeTo (getParentComponent()).y);
}

void ChainRow::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onDragEnd) onDragEnd();
    dragging = false;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ChainRow::resized()
{
    auto r = getLocalBounds().reduced (4, 0);
    remove.setBounds (r.removeFromRight (24).withSizeKeepingCentre (24, 24));
    r.removeFromRight (6);
    auto rep = r.removeFromRight (92);
    minus.setBounds (rep.removeFromLeft (24).withSizeKeepingCentre (24, 24));
    plus.setBounds (rep.removeFromRight (24).withSizeKeepingCentre (24, 24));
}

void ChainRow::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const auto gl = ctx.globalsCopy();
    if (index >= gl.chain.size()) return;
    const auto e = gl.chain[index];
    // Cues like the slots (SPEC.md 7.2): the playing entry = brightest background and a play icon;
    // entries of the selected pattern = thick white outline
    const bool cur = gl.chainOn && ctx.proc.engine.isPlaying && ctx.proc.engine.chainPosition.load() == index;
    const bool sel = (int) e.pattern == ctx.ui.selectedPattern;
    draw::tile (g, getLocalBounds().toFloat(), cur || dragging ? t.selected : t.tile, dragging ? t.dim2 : t.border, 6.0f);
    if (sel && ! dragging)
    {
        g.setColour (t.neutral);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
    }

    auto r = getLocalBounds().reduced (4, 0);
    // grip: shows the entry can be dragged
    g.setColour (t.dim);
    for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 2; ++gx)
            g.fillEllipse ((float) r.getX() + 3.0f + gx * 4.0f, (float) r.getCentreY() - 5.0f + gy * 4.0f, 2.0f, 2.0f);
    r.removeFromLeft (12);
    if (cur)
    {
        const juce::Point<float> c ((float) r.getX() + 9.0f, (float) r.getCentreY());
        juce::Path tri;
        tri.addTriangle (c.x - 3.0f, c.y - 4.0f, c.x - 3.0f, c.y + 4.0f, c.x + 4.0f, c.y);
        g.setColour (t.neutral);
        g.fillPath (tri);
    }
    r.removeFromLeft (18 + 6);
    g.setColour (t.muted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (pad2 (index + 1), r.removeFromLeft (26), juce::Justification::centredLeft);
    r.removeFromLeft (6);
    g.setColour (t.text);
    g.setFont (Fonts::monoSemi (12.0f));
    g.drawText (patternLabel (e.pattern), r.removeFromLeft (70), juce::Justification::centredLeft);
    r.removeFromLeft (6);
    r.removeFromRight (24 + 6 + 92 + 6);
    const double len = ctx.proc.read ([&] (const Project& p) { return p.patterns[(size_t) e.pattern].lengthPpq(); });
    g.setColour (t.text2);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (barsText (len * e.repeats), r.withTrimmedBottom (14).withTrimmedTop (6), juce::Justification::centredLeft);
    auto prog = r.withTrimmedTop (r.getHeight() - 11).withHeight (3).toFloat();
    g.setColour (t.border);
    g.fillRoundedRectangle (prog, 1.5f);
    if (cur)
    {
        g.setColour (t.neutral);
        g.fillRoundedRectangle (prog.withWidth (prog.getWidth() * ctx.proc.engine.chainEntryProgress.load()), 1.5f);
    }
    // repeats
    auto rep = getLocalBounds().reduced (4, 0).withTrimmedRight (24 + 6);
    rep = rep.removeFromRight (92).reduced (24, 0);
    g.setColour (t.text);
    g.setFont (Fonts::monoSemi (12.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String ((int) e.repeats), rep, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------
PatternsView::PatternsView (Context& c) : ctx (c)
{
    const auto neutralChoice = [] (bool on)
    {
        const auto& t = Theme::current();
        return on ? Pill::filled (t.neutral, t.onNeutral) : Pill::outline (t.border2, t.text2);
    };

    for (int b = 0; b < numPages; ++b)
    {
        auto* p = pageBtns.add (new Pill());
        p->radius = 5.0f;
        p->font = [] { return Fonts::monoSemi (12.0f); };
        p->text = [b] { return pageRangeText (b); };
        p->look = [this, b, neutralChoice] { return neutralChoice (ctx.ui.viewPage == b); };
        p->cornerDot = [this, b]
        {
            const int q = ctx.proc.engine.queuedPattern.load();
            return ctx.currentPattern() / patternsPerPage == b || (q >= 0 && q / patternsPerPage == b);
        };
        p->onClick = [this, b] { ctx.ui.viewPage = b; ctx.changed(); };
        addAndMakeVisible (p);
    }
    for (int s = 0; s < patternsPerPage; ++s)
    {
        auto* slot = slots.add (new PatternSlot (ctx, s));
        slot->onSlotDrag = [this] (int pat, juce::Point<int> pos, bool copy) { slotDragMoved (pat, pos, copy); };
        slot->onSlotDrop = [this] (int pat, juce::Point<int> pos, bool copy) { slotDropped (pat, pos, copy); };
        slot->onMenu = [this] (int pattern) { showSlotMenu (pattern); };
        addAndMakeVisible (slot);
    }

    auto smallFont = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    auto plain = [] { const auto& t = Theme::current(); return Pill::outline (t.border2, t.text2); };

    // COPY / COPY TRACK -> lit COPIED / TRACK COPIED (one clipboard). While lit, a slot click
    // picks the destination; PASTE writes there and stays lit. Clicking the lit button, or
    // CLEAR, empties the clipboard. CLEAR never erases a pattern.
    auto lit = [] { const auto& t = Theme::current(); return Pill::filled (t.neutral, t.onNeutral); };
    auto cancel = [this] { cancelClip(); };
    copyBtn.text = [this] { return juce::String (ctx.ui.patternClipKind == 1 ? "COPIED" : "COPY"); };
    copyBtn.font = smallFont;
    copyBtn.look = [this, plain, lit] { return ctx.ui.patternClipKind == 1 ? lit() : plain(); };
    copyBtn.onClick = [this, cancel]
    {
        if (ctx.ui.patternClipKind == 1) { cancel(); return; }
        copyPattern (ctx.ui.selectedPattern);
    };
    pasteBtn.text = [] { return juce::String ("PASTE"); };
    pasteBtn.font = smallFont;
    auto flashing = [this] { return juce::Time::getMillisecondCounter() < pasteFlashUntil; };
    pasteBtn.look = [this, flashing]
    {
        const auto& t = Theme::current();
        if (flashing()) return Pill::filled (t.green, t.onAccent);   // one-off paste done
        return Pill::outline (canPasteOnSelection() ? t.neutral : t.border2, t.text2);
    };
    pasteBtn.alpha = [this, flashing] { return (flashing() || canPasteOnSelection()) ? 1.0f : 0.4f; };
    pasteBtn.onClick = [this] { if (canPasteOnSelection()) pasteTo (ctx.ui.selectedPattern, false); };
    clearBtn.text = [] { return juce::String ("CLEAR"); };
    clearBtn.font = smallFont; clearBtn.look = plain;
    clearBtn.alpha = [this] { return ctx.ui.patternClipKind != 0 ? 1.0f : 0.4f; };
    clearBtn.onClick = [cancel] { cancel(); };
    pasteBtn.pressedLook = clearBtn.pressedLook = [] { const auto& t = Theme::current(); return Pill::filled (t.green, t.onAccent); };   // green while held
    for (auto* p : { &copyBtn, &pasteBtn, &clearBtn }) { p->radius = 5.0f; addAndMakeVisible (p); }

    trackBox.look = [] { const auto& t = Theme::current(); return Pill::Look { t.tile, t.border, t.text, 1.0f, false }; };
    trackBox.radius = 5.0f;
    trackBox.justification = juce::Justification::centredLeft;
    trackBox.textInset = 10;
    trackBox.font = [] { return Fonts::monoSemi (12.0f); };
    trackBox.text = [this] { return "TRACK  " + pad2 (ctx.ui.track + 1); };
    trackBox.onClick = [this] { showTrackMenu(); };
    trackBox.chevron = true;
    addAndMakeVisible (trackBox);

    // PATTERN box: the selected pattern, styled like the TRACK box (display only)
    patternBox.look = trackBox.look;
    patternBox.radius = 5.0f;
    patternBox.justification = juce::Justification::centredLeft;
    patternBox.textInset = 10;
    patternBox.font = trackBox.font;
    patternBox.text = [this] { return "PATTERN  " + patternLabel (ctx.ui.selectedPattern); };
    patternBox.setMouseCursor (juce::MouseCursor::NormalCursor);
    addAndMakeVisible (patternBox);

    copyTrackBtn.text = [this] { return juce::String (ctx.ui.patternClipKind == 2 ? "TRACK COPIED" : "COPY TRACK"); };
    copyTrackBtn.font = smallFont; copyTrackBtn.radius = 5.0f;
    copyTrackBtn.look = [this, plain, lit] { return ctx.ui.patternClipKind == 2 ? lit() : plain(); };
    copyTrackBtn.onClick = [this, cancel]
    {
        if (ctx.ui.patternClipKind == 2) { cancel(); return; }
        copyTrack (ctx.ui.selectedPattern);
    };
    addAndMakeVisible (copyTrackBtn);

    auto toggleLook = [] (bool on)
    {
        const auto& t = Theme::current();
        return on ? Pill::Look { t.neutral, t.neutral, t.onNeutral, 1.5f, false } : Pill::Look { juce::Colours::transparentBlack, t.dim, t.muted, 1.5f, false };
    };
    loopBtn.radius = 4.0f;
    loopBtn.font = [] { return Fonts::monoSemi (10.0f); };
    loopBtn.look = [this, toggleLook] { return toggleLook (ctx.globalsCopy().chainLoop); };
    loopBtn.text = [this] { return juce::String (ctx.globalsCopy().chainLoop ? "LOOP ON" : "LOOP OFF"); };
    loopBtn.onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.chainLoop = ! p.globals.chainLoop; }); };
    addAndMakeVisible (loopBtn);

    addBtn.radius = 6.0f;
    addBtn.font = [] { return Fonts::sansBold (11.0f).withExtraKerningFactor (0.12f); };
    addBtn.look = [] { const auto& t = Theme::current(); return Pill::outline (t.neutral, t.text); };
    addBtn.text = [this] { return "+ ADD PATTERN " + patternLabel (ctx.ui.selectedPattern); };
    addBtn.onClick = [this]
    {
        const int sel = ctx.ui.selectedPattern;
        ctx.edit (false, [sel] (Project& p)
        {
            if (p.globals.chain.size() < maxChainEntries)
                p.globals.chain.add ({ (std::uint8_t) sel, 1 });
        });
    };
    addAndMakeVisible (addBtn);
    clearChainBtn.radius = 6.0f;
    clearChainBtn.font = smallFont;
    clearChainBtn.look = plain;
    clearChainBtn.text = [] { return juce::String ("CLEAR"); };
    clearChainBtn.onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.chain.clear(); }); };
    addAndMakeVisible (clearChainBtn);
    chainScroll.setAutoHide (false);
    chainScroll.addListener (this);
    addChildComponent (chainScroll);
}

// PASTE writes onto the selected slot; dimmed when that would change nothing (the copied pattern
// itself, or for a track copy the same track of the same pattern)
bool PatternsView::canPasteOnSelection() const
{
    const int sel = ctx.ui.selectedPattern;
    if (ctx.ui.patternClipKind == 1) return sel != ctx.ui.patternClipSource;
    if (ctx.ui.patternClipKind == 2) return sel != ctx.ui.patternClipSource || ctx.ui.track != ctx.ui.patternClipTrack;
    return false;
}

void PatternsView::copyPattern (int index)
{
    ctx.ui.patternClip = ctx.proc.read ([index] (const Project& p) { return p.patterns[(size_t) index]; });
    ctx.ui.trackClip = TrackPattern();
    ctx.ui.patternClipKind = 1;
    ctx.ui.patternClipSource = index;
    ctx.ui.patternClipTrack = -1;
    ctx.changed();
}

void PatternsView::copyTrack (int index)
{
    const int t = ctx.ui.track;
    ctx.ui.trackClip = ctx.proc.read ([index, t] (const Project& p) { return p.patterns[(size_t) index].tracks[(size_t) t]; });
    ctx.ui.patternClip = Pattern();
    ctx.ui.patternClipKind = 2;
    ctx.ui.patternClipSource = index;
    ctx.ui.patternClipTrack = t;
    ctx.changed();
}

void PatternsView::pasteTo (int dest, bool oneOff)
{
    if (ctx.ui.patternClipKind == 0 || dest < 0 || dest >= numPatterns) return;
    if (ctx.ui.patternClipKind == 1)
    {
        const auto clip = ctx.ui.patternClip;
        ctx.proc.edit (dest, [dest, &clip] (Project& p) { p.patterns[(size_t) dest] = clip; },
                       "PASTE PATTERN " + patternLabel (ctx.ui.patternClipSource) + " > " + patternLabel (dest));
    }
    else
    {
        const auto clip = ctx.ui.trackClip;
        const int t = ctx.ui.track;   // the destination track: the TRACK dropdown
        ctx.proc.edit (dest, [dest, t, &clip] (Project& p) { p.patterns[(size_t) dest].tracks[(size_t) t] = clip; },
                       "PASTE TRACK TR" + pad2 (ctx.ui.patternClipTrack + 1) + Context::dot() + "PAT" + patternLabel (ctx.ui.patternClipSource)
                           + " > TR" + pad2 (t + 1) + Context::dot() + "PAT" + patternLabel (dest));
    }
    if (oneOff)
    {
        // slot menu paste: a one-off. Clear the clipboard and flash PASTE to confirm.
        cancelClip();
        pasteFlashUntil = juce::Time::getMillisecondCounter() + 350;
        juce::Component::SafePointer<PatternsView> safe (this);
        juce::Timer::callAfterDelay (380, [safe] { if (safe != nullptr) safe->pasteBtn.repaint(); });
        pasteBtn.repaint();
        return;
    }
    ctx.changed();
}

void PatternsView::cancelClip()
{
    ctx.ui.patternClipKind = 0;
    ctx.ui.patternClipSource = ctx.ui.patternClipTrack = -1;
    ctx.ui.patternClip = Pattern();
    ctx.ui.trackClip = TrackPattern();
    ctx.changed();
}

void PatternsView::showSlotMenu (int pattern)
{
    juce::PopupMenu m;
    m.addSectionHeader ("PATTERN " + patternLabel (pattern));
    m.addItem (3, "COPY PATTERN");
    m.addItem (4, "COPY TRACK " + pad2 (ctx.ui.track + 1));
    // PASTE names what it pastes (SPEC.md 7.1)
    const juce::String pasteText = ctx.ui.patternClipKind == 1 ? "PASTE PATTERN " + patternLabel (ctx.ui.patternClipSource)
                                 : ctx.ui.patternClipKind == 2 ? "PASTE ON TRACK " + pad2 (ctx.ui.track + 1)
                                 : juce::String ("PASTE");
    m.addItem (5, pasteText, ctx.ui.patternClipKind != 0);
    m.addSeparator();
    m.addItem (6, "NEW PATTERN");
    m.addItem (7, "CLEAR PATTERN");
    m.addItem (1, "LOAD PATTERN...");
    m.addItem (2, "SAVE PATTERN AS...");
    juce::Component::SafePointer<PatternsView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMinimumWidth (180), [safe, pattern] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->fileActions.loadPattern (pattern);
        else if (r == 2) safe->fileActions.savePatternAs (pattern);
        else if (r == 3) safe->copyPattern (pattern);
        else if (r == 4) safe->copyTrack (pattern);
        else if (r == 5) safe->pasteTo (pattern, true);
        else if (r == 6) safe->fileActions.newPattern (pattern);
        else if (r == 7) safe->fileActions.clearPattern (pattern);
    });
}

void PatternsView::showTrackMenu()
{
    juce::PopupMenu m;
    for (int t = 0; t < numTracks; ++t)
        m.addItem (t + 1, "TRACK " + pad2 (t + 1), true, t == ctx.ui.track);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&trackBox), [this] (int r)
    {
        if (r > 0) { ctx.ui.track = r - 1; ctx.changed(); }
    });
}

void PatternsView::refreshState()
{
    const int n = ctx.globalsCopy().chain.size();
    if (n != lastChainSize)
    {
        const bool added = lastChainSize >= 0 && n > lastChainSize;
        lastChainSize = n;
        chainRows.clear();
        for (int i = 0; i < n; ++i)
        {
            auto* row = chainRows.add (new ChainRow (ctx, i));
            row->onDrag = [this] (int idx, int y)
            {
                // dragging past the top or bottom of the list scrolls it
                if (y > chainList.getBottom()) scrollChainTo (chainFirst + 1);
                else if (y < chainList.getY()) scrollChainTo (chainFirst - 1);
                dragIndex = idx;
                dropIndex = dropIndexAt (y);
                repaint();
            };
            row->onDragEnd = [this]
            {
                const int from = dragIndex;
                int to = dropIndex;
                dragIndex = dropIndex = -1;
                if (from >= 0 && to >= 0)
                {
                    if (to > from) --to;   // the drop line is between entries
                    if (to != from)
                        ctx.edit (false, [this, from, to] (Project& p)
                        {
                            p.globals.chain.move (from, to);
                            ctx.proc.engine.chainEntryMoved (from, to);
                        });
                }
                repaint();
            };
            addAndMakeVisible (row);
        }
        if (added) chainFirst = juce::jmax (0, n - chainVisibleCount());   // + ADD: show the newest entry
        resized();
    }
    repaint();
}

int PatternsView::chainVisibleCount() const
{
    return juce::jmax (1, (chainList.getHeight() + 4) / 40);   // rows of 36 px plus 4 px gaps
}

void PatternsView::scrollChainTo (int first)
{
    const int maxFirst = juce::jmax (0, chainRows.size() - chainVisibleCount());
    first = juce::jlimit (0, maxFirst, first);
    if (first == chainFirst) return;
    chainFirst = first;
    resized();
    repaint();
}

void PatternsView::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    scrollChainTo ((int) std::lround (newRangeStart));
}

void PatternsView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // the wheel over the chain list scrolls it (rows pass their wheel events up to here)
    const auto pos = e.getEventRelativeTo (this).getPosition();
    if (chainList.contains (pos) && ! juce::exactlyEqual (w.deltaY, 0.0f))
        scrollChainTo (chainFirst + (w.deltaY > 0 ? -1 : 1));
}

void PatternsView::resized()
{
    auto r = getLocalBounds().reduced (20, 0).withTrimmedTop (14).withTrimmedBottom (16);
    aside = r.removeFromRight (360);
    r.removeFromRight (16);
    left = r;

    auto l = left;
    pagesLabel = l.removeFromTop (12);
    l.removeFromTop (6);
    auto banks = l.removeFromTop (30);
    const int bw = juce::jmin (120, (banks.getWidth() - 4 * (numPages - 1)) / numPages);
    for (int i = 0; i < numPages; ++i) pageBtns[i]->setBounds (banks.getX() + i * (bw + 4), banks.getY(), bw, 30);
    l.removeFromTop (12);
    selectedBar = l.removeFromBottom (50);
    l.removeFromBottom (12);
    slotsLabel = l.removeFromTop (12);
    l.removeFromTop (6);
    slotsArea = l;
    const int sw = (slotsArea.getWidth() - 3 * 8) / 4, sh = (slotsArea.getHeight() - 3 * 8) / 4;
    for (int i = 0; i < patternsPerPage; ++i)
        slots[i]->setBounds (slotsArea.getX() + (i % 4) * (sw + 8), slotsArea.getY() + (i / 4) * (sh + 8), sw, sh);

    {
        auto b = selectedBar.reduced (12, 11);
        patternBox.setBounds (b.removeFromLeft (112)); b.removeFromLeft (8);
        copyBtn.setBounds (b.removeFromLeft (72)); b.removeFromLeft (8);
        trackBox.setBounds (b.removeFromLeft (112)); b.removeFromLeft (8);
        copyTrackBtn.setBounds (b.removeFromLeft (110)); b.removeFromLeft (16);
        pasteBtn.setBounds (b.removeFromLeft (66)); b.removeFromLeft (8);
        clearBtn.setBounds (b.removeFromLeft (66));
    }

    auto a = aside.reduced (14, 12);
    chainHeader = a.removeFromTop (24);
    {
        auto h = chainHeader;
        loopBtn.setBounds (h.removeFromRight (78));
    }
    a.removeFromTop (10);
    columnHeader = a.removeFromTop (12);
    a.removeFromTop (8);
    footer = a.removeFromBottom (14);
    a.removeFromBottom (8);
    auto buttons = a.removeFromBottom (34);
    clearChainBtn.setBounds (buttons.removeFromRight (70));
    buttons.removeFromRight (6);
    addBtn.setBounds (buttons);
    a.removeFromBottom (10);
    chainList = a;
    const int n = chainRows.size(), visible = chainVisibleCount();
    chainFirst = juce::jlimit (0, juce::jmax (0, n - visible), chainFirst);
    auto list = chainList;
    const bool scrolling = n > visible;
    chainScroll.setVisible (scrolling);
    if (scrolling)
    {
        const auto& t = Theme::current();
        chainScroll.setBounds (list.removeFromRight (8));
        list.removeFromRight (6);
        chainScroll.setColour (juce::ScrollBar::thumbColourId, t.dim2);
        chainScroll.setColour (juce::ScrollBar::trackColourId, t.tile);
        chainScroll.setRangeLimits (0.0, (double) n, juce::dontSendNotification);
        chainScroll.setCurrentRange ((double) chainFirst, (double) visible, juce::dontSendNotification);
    }
    for (int i = 0; i < n; ++i)
    {
        auto* row = chainRows[i];
        const bool shown = i >= chainFirst && i < chainFirst + visible;
        row->setVisible (shown);
        if (shown)
            row->setBounds (list.getX(), list.getY() + (i - chainFirst) * 40, list.getWidth(), 36);
    }
}

// ---------------------------------------------------------------------------------------
// Slot drag and drop (SPEC.md 7.1.3)
int PatternsView::slotIndexAt (juce::Point<int> pos) const
{
    for (int i = 0; i < slots.size(); ++i)
        if (slots[i]->getBounds().contains (pos))
            return ctx.ui.viewPage * patternsPerPage + i;
    return -1;
}

void PatternsView::slotDragMoved (int pattern, juce::Point<int> pos, bool copy)
{
    ctx.ui.slotDragSource = pattern;
    slotDragPos = pos;
    slotDragCopy = copy;

    // hovering a PAGE button switches to that page
    for (int b = 0; b < pageBtns.size(); ++b)
        if (pageBtns[b]->getBounds().contains (pos) && ctx.ui.viewPage != b)
            ctx.ui.viewPage = b;

    // a drop target: an empty slot other than the source
    ctx.ui.slotDropTarget = -1;
    const int over = slotIndexAt (pos);
    if (over >= 0 && over != pattern && ctx.proc.read ([over] (const Project& p) { return p.patterns[(size_t) over].isEmpty(); }))
        ctx.ui.slotDropTarget = over;

    // over the chain panel: where the pattern would be added
    chainInsertIndex = -1;
    if (aside.contains (pos) && ctx.globalsCopy().chain.size() < maxChainEntries)
    {
        // near the top or bottom edge of the list: scroll it
        if (pos.y > chainList.getBottom() && pos.y < chainList.getBottom() + 24) scrollChainTo (chainFirst + 1);
        else if (pos.y < chainList.getY() && pos.y > chainList.getY() - 24) scrollChainTo (chainFirst - 1);
        chainInsertIndex = dropIndexAt (pos.y);
    }
    for (auto* sl : slots) sl->repaint();
    repaint();
}

void PatternsView::slotDropped (int pattern, juce::Point<int> pos, bool copy)
{
    slotDragMoved (pattern, pos, copy);
    const int target = ctx.ui.slotDropTarget;
    const int insertAt = chainInsertIndex;
    ctx.ui.slotDragSource = ctx.ui.slotDropTarget = -1;
    chainInsertIndex = -1;
    if (pattern >= 0 && target >= 0)
    {
        if (copy ? ctx.proc.copyPatternTo (pattern, target) : ctx.proc.movePattern (pattern, target))
            ctx.ui.selectedPattern = target;
    }
    else if (pattern >= 0 && insertAt < 0 && slotIndexAt (pos) == pattern)
    {
        ctx.ui.selectedPattern = pattern;   // released on its own slot: a click that slipped, select it
    }
    else if (pattern >= 0 && insertAt >= 0)
    {
        ctx.edit (false, [this, pattern, insertAt] (Project& p)
        {
            if (p.globals.chain.size() >= maxChainEntries) return;
            p.globals.chain.insert (insertAt, { (std::uint8_t) pattern, 1 });
            ctx.proc.engine.chainEntryInserted (insertAt);
        });
    }
    ctx.changed();
}

int PatternsView::dropIndexAt (int y) const
{
    const int n = chainRows.size();
    const int pitch = 36 + 4;
    return juce::jlimit (0, n, chainFirst + (int) std::floor ((y - chainList.getY() + pitch / 2) / (double) pitch));
}

void PatternsView::paintOverChildren (juce::Graphics& g)
{
    if (ctx.ui.slotDragSource >= 0)
    {
        const auto& t = Theme::current();
        // where the dragged pattern would be added to the chain
        if (chainInsertIndex >= 0 && chainInsertIndex >= chainFirst && chainInsertIndex <= chainFirst + chainVisibleCount())
        {
            const int y = chainList.getY() + (chainInsertIndex - chainFirst) * 40 - 2;
            g.setColour (t.neutral);
            g.fillRoundedRectangle ((float) chainList.getX(), (float) y - 1.0f, (float) chainList.getWidth(), 3.0f, 1.5f);
        }
        // a small ghost of the dragged slot follows the mouse ("+" when copying)
        auto ghost = juce::Rectangle<float> (64.0f, 30.0f).withCentre (slotDragPos.toFloat()).translated (14.0f, 12.0f);
        g.setColour (t.selected.withAlpha (0.92f));
        g.fillRoundedRectangle (ghost, 6.0f);
        g.setColour (t.neutral);
        g.drawRoundedRectangle (ghost.reduced (0.75f), 6.0f, 1.5f);
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (13.0f));
        g.drawText ((slotDragCopy ? "+ " : "") + patternLabel (ctx.ui.slotDragSource), ghost.toNearestInt(), juce::Justification::centred);
        return;
    }
    if (dragIndex < 0 || dropIndex < 0) return;
    if (dropIndex == dragIndex || dropIndex == dragIndex + 1) return;   // would not move
    if (dropIndex < chainFirst || dropIndex > chainFirst + chainVisibleCount()) return;   // scrolled out of view
    const int y = chainList.getY() + (dropIndex - chainFirst) * 40 - 2;
    g.setColour (Theme::current().neutral);
    g.fillRoundedRectangle ((float) chainList.getX(), (float) y - 1.0f, (float) chainList.getWidth(), 3.0f, 1.5f);
}

void PatternsView::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    g.fillAll (t.bg);
    draw::label (g, "PAGE", pagesLabel, t.muted);
    draw::label (g, "PATTERNS " + pageRangeText (ctx.ui.viewPage), slotsLabel, t.muted);

    draw::tile (g, selectedBar.toFloat(), t.panel, t.border, 8.0f);
    {
        juce::String clip;   // what is copied: "PATTERN 05", "PATTERN 05 - TRACK 02"
        if (ctx.ui.patternClipKind == 1) clip = "PATTERN " + patternLabel (ctx.ui.patternClipSource);
        else if (ctx.ui.patternClipKind == 2) clip = "PATTERN " + patternLabel (ctx.ui.patternClipSource) + " - TRACK " + pad2 (ctx.ui.patternClipTrack + 1);
        g.setColour (t.text2);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (clip, selectedBar.reduced (12, 0), juce::Justification::centredRight);
    }

    draw::tile (g, aside.toFloat(), t.panel, t.border, 8.0f);
    // the chain state as a title (the switch is the header CHAIN): grey CHAIN OFF, white CHAIN ON
    {
        const bool on = ctx.globalsCopy().chainOn;
        draw::label (g, on ? "CHAIN ON" : "CHAIN OFF", chainHeader, on ? t.text : t.muted);
    }
    {
        auto h = columnHeader.reduced (4, 0);
        g.setColour (t.muted);
        g.setFont (Fonts::label (9.0f));
        h.removeFromLeft (36);
        g.drawText ("#", h.removeFromLeft (32), juce::Justification::centredLeft);
        g.drawText ("PATTERN", h.removeFromLeft (76), juce::Justification::centredLeft);
        h.removeFromRight (24 + 6);
        g.drawText ("REPEAT", h.removeFromRight (92), juce::Justification::centred);
        g.drawText ("LENGTH", h, juce::Justification::centredLeft);
    }
    const auto gl = ctx.globalsCopy();
    double total = 0.0;
    for (const auto& e : gl.chain)
        total += ctx.proc.read ([&] (const Project& p) { return p.patterns[(size_t) e.pattern].lengthPpq(); }) * e.repeats;
    g.setColour (t.muted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (juce::String (gl.chain.size()) + (gl.chain.size() == 1 ? " ENTRY" : " ENTRIES"), footer, juce::Justification::centredLeft);
    g.drawText ("TOTAL " + barsText (total), footer, juce::Justification::centredRight);
}
} // namespace vima
