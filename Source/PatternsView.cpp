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

void PatternSlot::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()) || e.mods.isPopupMenu()) return;
    const int idx = ctx.ui.viewPage * patternsPerPage + slot;
    if (playArea().contains (e.getPosition()))
    {
        // play button: select and queue (switches at the quantize point)
        if (ctx.ui.patternClipKind == 0)
            ctx.ui.selectedPattern = idx;
        if (idx != ctx.currentPattern())
            ctx.proc.queuePattern (idx);
    }
    else if (ctx.ui.patternClipKind != 0)
        ctx.ui.patternPasteDest = idx;   // COPIED: the click only picks where the paste lands
    else
        ctx.ui.selectedPattern = idx;    // select only (for COPY / COPY TRACK)
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
    const bool copying = ctx.ui.patternClipKind != 0;
    const bool sel = ! copying && idx == ctx.ui.selectedPattern;
    const bool dest = copying && idx == ctx.ui.patternPasteDest;
    const auto chain = ctx.globalsCopy().chain;
    bool inChain = false;
    for (const auto& e : chain) inChain = inChain || e.pattern == idx;

    auto b = getLocalBounds().toFloat();
    const auto fill = sel ? t.selected : (used ? t.tile : t.panel);
    const auto border = (playing || queued) ? t.neutral : (sel ? t.dim2 : t.border3);
    const float bw = (playing || queued) ? 2.0f : 1.0f;
    g.setColour (fill);
    g.fillRoundedRectangle (b, 7.0f);
    g.setColour (border);
    if (queued)
    {
        juce::Path p, dashed;
        p.addRoundedRectangle (b.reduced (bw * 0.5f), 7.0f);
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (bw).createDashedStroke (dashed, p, dashes, 2);
        g.fillPath (dashed);
    }
    else
    {
        g.drawRoundedRectangle (b.reduced (bw * 0.5f), 7.0f, bw);
    }
    if (dest)
    {
        // ghost preview: where the paste lands
        juce::Path p, dashed;
        const auto r = b.reduced (playing || queued ? 4.0f : 1.0f);
        p.addRoundedRectangle (r, 6.0f);
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (t.neutral.withAlpha (0.8f));
        g.fillPath (dashed);
        g.setColour (t.neutral.withAlpha (0.07f));
        g.fillRoundedRectangle (r, 6.0f);
    }

    // play button (top center)
    {
        const auto pa = playArea().toFloat();
        const auto pc = (playing || queued) ? t.neutral : (hoverPlay ? t.text : t.text2);
        g.setColour (hoverPlay ? t.selected : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (pa, 5.0f);
        g.setColour (playing ? t.neutral : (hoverPlay ? t.dim2 : t.border3));
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
    if (playing)
        draw::led (g, { (float) top.getRight() - 7.0f, (float) top.getCentreY() }, 6.0f, t.neutral, true);

    // Thumbnail (7.1.1): one bar per track. Height = how many steps are on, out of the length.
    // Notes out only: the note part in green. CC out only: the CC part in cyan. Both: two thin
    // bars side by side (notes left, CC right). Neither: grey (the busier part).
    auto thumb = inner.removeFromBottom (22).toFloat();
    const float gap = 2.0f;
    const float w = (thumb.getWidth() - gap * (numTracks - 1)) / (float) numTracks;
    auto drawBar = [&g, &t] (juce::Rectangle<float> col, int on, int length, juce::Colour c)
    {
        if (on == 0)
        {
            g.setColour (t.sliderTrack);
            g.fillRect (col.removeFromBottom (2.0f));
            return;
        }
        g.setColour (c);
        g.fillRect (col.removeFromBottom (3.0f + 19.0f * (float) on / (float) juce::jmax (1, length)));
    };
    for (int k = 0; k < numTracks; ++k)
    {
        const auto& tp = pattern.tracks[(size_t) k];
        const auto& r = routing[(size_t) k];
        const int notes = tp.onStepCount(), ccs = tp.ccOnStepCount(), len = (int) tp.length;
        auto col = thumb.withX (thumb.getX() + k * (w + gap)).withWidth (w);
        if (r.noteOut && r.ccOut)
        {
            const float half = (col.getWidth() - 1.0f) * 0.5f;
            drawBar (col.withWidth (half), notes, len, t.green);
            drawBar (col.withTrimmedLeft (col.getWidth() - half), ccs, len, t.cyan);
        }
        else if (r.noteOut) drawBar (col, notes, len, t.green);
        else if (r.ccOut)   drawBar (col, ccs, len, t.cyan);
        else                drawBar (col, juce::jmax (notes, ccs), len, t.dim);
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
    const bool cur = gl.chainOn && ctx.proc.engine.isPlaying && ctx.proc.engine.chainPosition.load() == index;
    draw::tile (g, getLocalBounds().toFloat(), cur || dragging ? t.selected : t.tile, cur ? t.neutral : (dragging ? t.dim2 : t.border), 6.0f);

    auto r = getLocalBounds().reduced (4, 0);
    // grip: shows the entry can be dragged
    g.setColour (t.dim);
    for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 2; ++gx)
            g.fillEllipse ((float) r.getX() + 3.0f + gx * 4.0f, (float) r.getCentreY() - 5.0f + gy * 4.0f, 2.0f, 2.0f);
    r.removeFromLeft (12);
    draw::led (g, { (float) r.getX() + 9.0f, (float) r.getCentreY() }, 7.0f, cur ? t.neutral : t.ledOff, cur);
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
        addAndMakeVisible (slots.add (new PatternSlot (ctx, s)));

    auto smallFont = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    auto plain = [] { const auto& t = Theme::current(); return Pill::outline (t.border2, t.text2); };

    // COPY / COPY TRACK -> lit COPIED / TRACK COPIED (one clipboard). While lit, a slot click
    // picks the destination; PASTE writes there and stays lit. Clicking the lit button, or
    // CLEAR, empties the clipboard. CLEAR never erases a pattern.
    auto lit = [] { const auto& t = Theme::current(); return Pill::filled (t.neutral, t.onNeutral); };
    auto cancel = [this]
    {
        ctx.ui.patternClipKind = 0;
        ctx.ui.patternClipSource = ctx.ui.patternClipTrack = ctx.ui.patternPasteDest = -1;
        ctx.ui.patternClip = Pattern();
        ctx.ui.trackClip = TrackPattern();
        ctx.changed();
    };
    copyBtn.text = [this] { return juce::String (ctx.ui.patternClipKind == 1 ? "COPIED" : "COPY"); };
    copyBtn.font = smallFont;
    copyBtn.look = [this, plain, lit] { return ctx.ui.patternClipKind == 1 ? lit() : plain(); };
    copyBtn.onClick = [this, cancel]
    {
        if (ctx.ui.patternClipKind == 1) { cancel(); return; }
        const int sel = ctx.ui.selectedPattern;
        ctx.ui.patternClip = ctx.proc.read ([sel] (const Project& p) { return p.patterns[(size_t) sel]; });
        ctx.ui.trackClip = TrackPattern();
        ctx.ui.patternClipKind = 1;
        ctx.ui.patternClipSource = sel;
        ctx.ui.patternClipTrack = -1;
        ctx.ui.patternPasteDest = -1;
        ctx.changed();
    };
    pasteBtn.text = [] { return juce::String ("PASTE"); };
    pasteBtn.font = smallFont;
    pasteBtn.look = [this]
    {
        const auto& t = Theme::current();
        const bool ready = ctx.ui.patternClipKind != 0 && ctx.ui.patternPasteDest >= 0;
        return Pill::outline (ready ? t.neutral : t.border2, t.text2);
    };
    pasteBtn.alpha = [this] { return (ctx.ui.patternClipKind != 0 && ctx.ui.patternPasteDest >= 0) ? 1.0f : 0.4f; };
    pasteBtn.onClick = [this]
    {
        const int dest = ctx.ui.patternPasteDest;
        if (ctx.ui.patternClipKind == 0 || dest < 0) return;
        if (ctx.ui.patternClipKind == 1)
        {
            const auto clip = ctx.ui.patternClip;
            ctx.proc.edit (dest, [dest, &clip] (Project& p) { p.patterns[(size_t) dest] = clip; });
        }
        else
        {
            const auto clip = ctx.ui.trackClip;
            const int t = ctx.ui.patternsViewTrack;   // the destination track: the TRACK dropdown
            ctx.proc.edit (dest, [dest, t, &clip] (Project& p) { p.patterns[(size_t) dest].tracks[(size_t) t] = clip; });
        }
        ctx.changed();
    };
    clearBtn.text = [] { return juce::String ("CLEAR"); };
    clearBtn.font = smallFont; clearBtn.look = plain;
    clearBtn.alpha = [this] { return ctx.ui.patternClipKind != 0 ? 1.0f : 0.4f; };
    clearBtn.onClick = [cancel] { cancel(); };
    for (auto* p : { &copyBtn, &pasteBtn, &clearBtn }) { p->radius = 5.0f; addAndMakeVisible (p); }

    trackBox.look = [] { const auto& t = Theme::current(); return Pill::Look { t.tile, t.border, t.text, 1.0f, false }; };
    trackBox.radius = 5.0f;
    trackBox.justification = juce::Justification::centredLeft;
    trackBox.textInset = 10;
    trackBox.font = [] { return Fonts::monoSemi (12.0f); };
    trackBox.text = [this] { return "TRACK  " + pad2 (ctx.ui.patternsViewTrack + 1); };
    trackBox.onClick = [this] { showTrackMenu(); };
    addAndMakeVisible (trackBox);

    copyTrackBtn.text = [this] { return juce::String (ctx.ui.patternClipKind == 2 ? "TRACK COPIED" : "COPY TRACK"); };
    copyTrackBtn.font = smallFont; copyTrackBtn.radius = 5.0f;
    copyTrackBtn.look = [this, plain, lit] { return ctx.ui.patternClipKind == 2 ? lit() : plain(); };
    copyTrackBtn.onClick = [this, cancel]
    {
        if (ctx.ui.patternClipKind == 2) { cancel(); return; }
        ctx.ui.patternClip = Pattern();
        ctx.ui.patternPasteDest = -1;
        const int sel = ctx.ui.selectedPattern, t = ctx.ui.patternsViewTrack;
        ctx.ui.trackClip = ctx.proc.read ([sel, t] (const Project& p) { return p.patterns[(size_t) sel].tracks[(size_t) t]; });
        ctx.ui.patternClipKind = 2;
        ctx.ui.patternClipSource = sel;
        ctx.ui.patternClipTrack = t;
        ctx.changed();
    };
    addAndMakeVisible (copyTrackBtn);

    auto toggleLook = [] (bool on)
    {
        const auto& t = Theme::current();
        return on ? Pill::Look { t.neutral, t.neutral, t.onNeutral, 1.5f, false } : Pill::Look { juce::Colours::transparentBlack, t.dim, t.muted, 1.5f, false };
    };
    setLearnTarget (chainOnBtn, MacroTarget::chainOn);
    chainOnBtn.radius = loopBtn.radius = 4.0f;
    chainOnBtn.font = loopBtn.font = [] { return Fonts::monoSemi (10.0f); };
    chainOnBtn.look = [this, toggleLook] { return toggleLook (ctx.globalsCopy().chainOn); };
    chainOnBtn.text = [this] { return juce::String (ctx.globalsCopy().chainOn ? "CHAIN ON" : "CHAIN OFF"); };
    chainOnBtn.onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.chainOn = ! p.globals.chainOn; }); };
    loopBtn.look = [this, toggleLook] { return toggleLook (ctx.globalsCopy().chainLoop); };
    loopBtn.text = [this] { return juce::String (ctx.globalsCopy().chainLoop ? "LOOP ON" : "LOOP OFF"); };
    loopBtn.onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.chainLoop = ! p.globals.chainLoop; }); };
    addAndMakeVisible (chainOnBtn);
    addAndMakeVisible (loopBtn);

    addBtn.radius = 6.0f;
    addBtn.font = [] { return Fonts::sansBold (11.0f).withExtraKerningFactor (0.12f); };
    addBtn.look = [] { const auto& t = Theme::current(); return Pill::outline (t.neutral, t.text); };
    addBtn.text = [this] { return "+ ADD " + patternLabel (ctx.ui.selectedPattern); };
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
}

void PatternsView::showTrackMenu()
{
    juce::PopupMenu m;
    for (int t = 0; t < numTracks; ++t)
        m.addItem (t + 1, "TRACK " + pad2 (t + 1), true, t == ctx.ui.patternsViewTrack);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&trackBox), [this] (int r)
    {
        if (r > 0) { ctx.ui.patternsViewTrack = r - 1; ctx.changed(); }
    });
}

void PatternsView::refreshState()
{
    const int n = ctx.globalsCopy().chain.size();
    if (n != lastChainSize)
    {
        lastChainSize = n;
        chainRows.clear();
        for (int i = 0; i < n; ++i)
        {
            auto* row = chainRows.add (new ChainRow (ctx, i));
            row->onDrag = [this] (int idx, int y)
            {
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
        resized();
    }
    repaint();
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
        b.removeFromLeft (64 + 8 + 36 + 8);   // "SELECTED" label + number
        copyBtn.setBounds (b.removeFromLeft (72)); b.removeFromLeft (8);
        trackBox.setBounds (b.removeFromLeft (100)); b.removeFromLeft (8);
        copyTrackBtn.setBounds (b.removeFromLeft (110)); b.removeFromLeft (16);
        pasteBtn.setBounds (b.removeFromLeft (66)); b.removeFromLeft (8);
        clearBtn.setBounds (b.removeFromLeft (66));
    }

    auto a = aside.reduced (14, 12);
    chainHeader = a.removeFromTop (24);
    {
        auto h = chainHeader;
        loopBtn.setBounds (h.removeFromRight (78));
        h.removeFromRight (8);
        chainOnBtn.setBounds (h.removeFromRight (86));
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
    auto list = chainList;
    for (auto* row : chainRows)
    {
        if (list.getHeight() < 36) { row->setVisible (false); continue; }
        row->setVisible (true);
        row->setBounds (list.removeFromTop (36));
        list.removeFromTop (4);
    }
}

int PatternsView::dropIndexAt (int y) const
{
    const int n = chainRows.size();
    const int pitch = 36 + 4;
    return juce::jlimit (0, n, (int) std::floor ((y - chainList.getY() + pitch / 2) / (double) pitch));
}

void PatternsView::paintOverChildren (juce::Graphics& g)
{
    if (dragIndex < 0 || dropIndex < 0) return;
    if (dropIndex == dragIndex || dropIndex == dragIndex + 1) return;   // would not move
    const int y = chainList.getY() + dropIndex * 40 - 2;
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
        auto b = selectedBar.reduced (12, 0);
        draw::label (g, "SELECTED", b.removeFromLeft (64), t.muted);
        b.removeFromLeft (8);
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (13.0f));
        g.drawText (patternLabel (ctx.ui.selectedPattern), b.removeFromLeft (40), juce::Justification::centredLeft);
        juce::String clip;   // what is copied: "PATTERN 05", "PATTERN 05 - TRACK 02"
        if (ctx.ui.patternClipKind == 1) clip = "PATTERN " + patternLabel (ctx.ui.patternClipSource);
        else if (ctx.ui.patternClipKind == 2) clip = "PATTERN " + patternLabel (ctx.ui.patternClipSource) + " - TRACK " + pad2 (ctx.ui.patternClipTrack + 1);
        g.setColour (t.text2);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (clip, selectedBar.reduced (12, 0), juce::Justification::centredRight);
    }

    draw::tile (g, aside.toFloat(), t.panel, t.border, 8.0f);
    draw::label (g, "CHAIN", chainHeader, t.muted);
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
