#include "OpsPanel.h"

namespace vima
{
namespace
{
    bool& laneFlag (Globals& g, Lane l)
    {
        switch (l)
        {
            case Lane::on:          return g.laneOn;
            case Lane::note:        return g.laneNote;
            case Lane::octave:      return g.laneOct;
            case Lane::velocity:    return g.laneVel;
            case Lane::gate:        return g.laneGate;
            case Lane::probability: return g.laneProb;
            case Lane::cc:          return g.laneCC;
        }
        return g.laneOn;
    }

    LaneRange& laneRange (Globals& g, Lane l)
    {
        switch (l)
        {
            case Lane::note:        return g.rangeNote;
            case Lane::octave:      return g.rangeOct;
            case Lane::velocity:    return g.rangeVel;
            case Lane::gate:        return g.rangeGate;
            case Lane::probability: return g.rangeProb;
            case Lane::cc:          return g.rangeCC;
            case Lane::on:          break;
        }
        return g.rangeVel;
    }

    const char* laneName (Lane l)
    {
        switch (l)
        {
            case Lane::on:          return "ON";
            case Lane::note:        return "NOTE";
            case Lane::octave:      return "OCT";
            case Lane::velocity:    return "VEL";
            case Lane::gate:        return "GATE";
            case Lane::probability: return "PROB";
            case Lane::cc:          return "CC";
        }
        return "";
    }

    juce::Colour laneColour (Lane l)
    {
        const auto& t = Theme::current();
        switch (l)
        {
            case Lane::note: case Lane::velocity: return t.green;
            case Lane::gate:        return t.purple;
            case Lane::probability: return t.amber;
            case Lane::cc:          return t.cyan;
            case Lane::octave: case Lane::on: break;
        }
        return t.text2;
    }

    const std::vector<Lane>& noteLanes()
    {
        static const std::vector<Lane> l { Lane::on, Lane::note, Lane::octave, Lane::velocity, Lane::gate, Lane::probability };
        return l;
    }
    const std::vector<Lane>& ccLanes()
    {
        static const std::vector<Lane> l { Lane::on, Lane::cc };
        return l;
    }

    Pill::Look chipLook (bool on, juce::Colour accent)
    {
        const auto& t = Theme::current();
        return on ? Pill::tinted (accent) : Pill::outline (t.border2, t.text2);
    }
}

// ---------------------------------------------------------------------------------------
RangeBar::RangeBar (Context& c, Lane l) : ctx (c), lane (l)
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

LaneRange RangeBar::get() const
{
    auto g = ctx.globalsCopy();
    return laneRange (g, lane);
}

void RangeBar::set (LaneRange r)
{
    ctx.edit (false, [this, r] (Project& p) { laneRange (p.globals, lane) = r; });
}

void RangeBar::axis (int& lo, int& hi) const
{
    switch (lane)
    {
        case Lane::note:        lo = 0; hi = 11; return;
        case Lane::octave:      lo = minOctave; hi = maxOctave; return;
        case Lane::velocity:    lo = 1; hi = 127; return;
        case Lane::gate:        lo = 1; hi = maxGate; return;
        case Lane::probability: lo = 0; hi = 100; return;
        case Lane::cc:          lo = 0; hi = 127; return;
        case Lane::on:          lo = 0; hi = 1; return;
    }
}

juce::String RangeBar::valueText (int v) const
{
    switch (lane)
    {
        case Lane::note:   return pitchClassName (v);
        case Lane::octave: return juce::String (v + ctx.globalsCopy().octaveOffset());
        case Lane::gate: case Lane::probability: return juce::String (v) + "%";
        case Lane::on: case Lane::velocity: case Lane::cc: break;
    }
    return juce::String (v);
}

juce::Rectangle<float> RangeBar::barArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (38 + 10 + 32 + 10).withTrimmedRight (32 + 10);
}

int RangeBar::valueAt (float x) const
{
    int lo = 0, hi = 0;
    axis (lo, hi);
    const auto b = barArea();
    const float f = juce::jlimit (0.0f, 1.0f, (x - b.getX()) / juce::jmax (1.0f, b.getWidth()));
    return juce::roundToInt (lo + f * (hi - lo));
}

void RangeBar::mouseDown (const juce::MouseEvent& e)
{
    int lo = 0, hi = 0;
    axis (lo, hi);
    const auto r = get();
    const auto b = barArea();
    auto xOf = [&] (int v) { return b.getX() + b.getWidth() * (float) (v - lo) / (float) juce::jmax (1, hi - lo); };
    const float x = (float) e.x;
    dragging = std::abs (x - xOf (r.min)) <= std::abs (x - xOf (r.max)) ? 1 : 2;
    if (r.min == r.max) dragging = x < xOf (r.min) ? 1 : 2;
    mouseDrag (e);
}

void RangeBar::mouseDrag (const juce::MouseEvent& e)
{
    auto r = get();
    const int v = valueAt ((float) e.x);
    if (dragging == 1) r.min = juce::jmin (v, r.max);
    else if (dragging == 2) r.max = juce::jmax (v, r.min);
    set (r);
}

void RangeBar::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    int lo = 0, hi = 0;
    axis (lo, hi);
    const auto r = get();
    auto area = getLocalBounds();
    g.setColour (t.text2);
    g.setFont (Fonts::monoSemi (11.0f));
    g.drawText (laneName (lane), area.removeFromLeft (38), juce::Justification::centredLeft);
    area.removeFromLeft (10);
    g.setColour (t.text);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (valueText (juce::jlimit (lo, hi, r.min)), area.removeFromLeft (32), juce::Justification::centredRight);
    area.removeFromLeft (10);
    g.drawText (valueText (juce::jlimit (lo, hi, r.max)), area.removeFromRight (32), juce::Justification::centredLeft);

    const auto b = barArea();
    auto track = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
    g.setColour (t.border);
    g.fillRoundedRectangle (track, 2.0f);
    auto xOf = [&] (int v) { return b.getX() + b.getWidth() * (float) (juce::jlimit (lo, hi, v) - lo) / (float) juce::jmax (1, hi - lo); };
    const float x0 = xOf (r.min), x1 = xOf (r.max);
    g.setColour (laneColour (lane));
    g.fillRoundedRectangle (track.withLeft (x0).withRight (juce::jmax (x0 + 1.0f, x1)), 2.0f);
    g.setColour (t.text);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x0, track.getCentreY() }));
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x1, track.getCentreY() }));
}

// ---------------------------------------------------------------------------------------
ShiftKnob::ShiftKnob (Context& c) : ctx (c)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle ("Shift all");
}

void ShiftKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const auto lane = ctx.ui.ccMode ? Lane::cc : ctx.ui.shiftLane;
    const auto sc = ctx.scope();
    ctx.beginGesture();   // the whole drag is one undo entry
    gesture = ctx.proc.read ([&] (const Project& p) { return beginShift (p, sc, lane); });
    startY = e.getScreenY();
    active = true;
    ctx.ui.shiftAmount = 0;
    repaint();
}

void ShiftKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! active) return;
    const int k = juce::roundToInt ((float) (startY - e.getScreenY()) / (e.mods.isShiftDown() ? 18.0f : 9.0f));
    if (k == ctx.ui.shiftAmount) return;
    ctx.ui.shiftAmount = k;
    ctx.edit (false, [this, k] (Project& p) { applyShift (p, gesture, k); });   // always from the snapshot
}

void ShiftKnob::mouseUp (const juce::MouseEvent&)
{
    active = false;
    ctx.ui.shiftAmount = 0;
    ctx.changed();
}

void ShiftKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    // one notch = shift by one (each notch is its own undo step)
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (juce::exactlyEqual (d, 0.0f) || active) return;
    const int k = (d > 0.0f) != w.isReversed ? 1 : -1;
    const auto lane = ctx.ui.ccMode ? Lane::cc : ctx.ui.shiftLane;
    const auto sc = ctx.scope();
    ctx.beginGesture();
    const auto g = ctx.proc.read ([&] (const Project& p) { return beginShift (p, sc, lane); });
    ctx.edit (false, [&g, k] (Project& p) { applyShift (p, g, k); });
}

void ShiftKnob::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 5.0f;
    g.setColour (active ? ctx.modeAccent().withAlpha (0.35f) : t.border);
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 4.0f);
    const float ang = ctx.ui.shiftAmount * 20.0f * juce::MathConstants<float>::pi / 180.0f;
    g.setColour (ctx.modeAccent());
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ c.x + r * std::sin (ang), c.y - r * std::cos (ang) }));
    const int k = ctx.ui.shiftAmount;
    g.setColour (k != 0 ? ctx.modeAccent() : t.text2);
    g.setFont (Fonts::monoSemi (13.0f));
    g.drawText (k > 0 ? "+" + juce::String (k) : juce::String (k), getLocalBounds(), juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------
OpsPanel::OpsPanel (Context& c) : ctx (c), shiftKnob (c)
{
    auto smallFont = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    auto plain = [] { const auto& t = Theme::current(); return Pill::outline (t.border2, t.text2); };

    // COPY -> lit "COPIED" (clipboard full). Clicking COPIED empties the clipboard.
    copyBtn.text = [this] { return juce::String (ctx.ui.clip.empty() ? "COPY" : "COPIED"); };
    copyBtn.font = smallFont;
    copyBtn.look = [this, plain]
    {
        if (ctx.ui.clip.empty()) return plain();
        return Pill::filled (ctx.modeAccent(), Theme::current().onAccent);
    };
    copyBtn.onClick = [this]
    {
        if (! ctx.ui.clip.empty())
        {
            ctx.ui.clip.clear();
            ctx.ui.pasteStart = -1;
            ctx.changed();
            return;
        }
        const auto lanes = activeLanes (ctx.globalsCopy(), ctx.ui.ccMode);
        if (lanes.empty()) return;   // no lane armed: nothing to copy
        const auto sc = ctx.scope();
        ctx.ui.clip = ctx.proc.read ([&] (const Project& p) { return copySteps (p, sc); });
        ctx.ui.clipLanes = lanes;
        ctx.ui.clipSpan = ctx.ui.clip.empty() ? 0 : ctx.ui.clip.back().offset + 1;
        ctx.ui.selection = 0;        // no selection while COPIED
        ctx.ui.pasteStart = -1;
        ctx.changed();
    };
    addAndMakeVisible (copyBtn);

    // PASTE writes the copied lanes at the landing step (the ghost preview). COPIED stays lit.
    pasteBtn.text = [] { return juce::String ("PASTE"); };
    pasteBtn.font = smallFont;
    pasteBtn.look = [this]
    {
        const auto& t = Theme::current();
        const bool ready = ! ctx.ui.clip.empty() && ctx.ui.pasteStart >= 0;
        return Pill::outline (ready ? ctx.modeAccent() : t.border2, t.text2);
    };
    pasteBtn.alpha = [this] { return (! ctx.ui.clip.empty() && ctx.ui.pasteStart >= 0) ? 1.0f : 0.4f; };
    pasteBtn.onClick = [this]
    {
        if (ctx.ui.clip.empty() || ctx.ui.pasteStart < 0) return;
        const int t = ctx.ui.track, start = ctx.ui.pasteStart;
        const auto clip = ctx.ui.clip;
        const auto lanes = ctx.ui.clipLanes;
        ctx.edit (true, [&] (Project& p) { pasteSteps (p, p.currentPattern, t, start, clip, lanes); });
    };
    addAndMakeVisible (pasteBtn);

    clearBtn.text = [] { return juce::String ("CLEAR"); };
    clearBtn.font = smallFont;
    clearBtn.look = plain;
    // CLEAR: deselect; when COPIED, also empty the clipboard (and the ghost preview)
    clearBtn.alpha = [this] { return (ctx.hasSelection() || ! ctx.ui.clip.empty()) ? 1.0f : 0.4f; };
    clearBtn.onClick = [this]
    {
        ctx.ui.selection = 0;
        ctx.ui.clip.clear();
        ctx.ui.pasteStart = -1;
        ctx.changed();
    };
    addAndMakeVisible (clearBtn);

    // Lane chips (ALL first). Built for note mode; CC mode shows ALL, ON, CC.
    std::vector<Lane> all { Lane::on, Lane::note, Lane::octave, Lane::velocity, Lane::gate, Lane::probability, Lane::cc };
    {
        auto* p = laneChips.add (new Pill());
        p->text = [] { return juce::String ("ALL"); };
        p->getProperties().set ("lane", -1);
        p->look = [this]
        {
            const auto g = ctx.globalsCopy();
            auto gg = g;
            bool allOn = true;
            for (auto l : ctx.ui.ccMode ? ccLanes() : noteLanes()) allOn = allOn && laneFlag (gg, l);
            return chipLook (allOn, ctx.modeAccent());
        };
        p->onClick = [this]
        {
            const bool cc = ctx.ui.ccMode;
            ctx.edit (false, [cc] (Project& pr)
            {
                bool allOn = true;
                for (auto l : cc ? ccLanes() : noteLanes()) allOn = allOn && laneFlag (pr.globals, l);
                for (auto l : cc ? ccLanes() : noteLanes()) laneFlag (pr.globals, l) = ! allOn;
            });
        };
        addAndMakeVisible (p);
    }
    for (auto l : all)
    {
        auto* p = laneChips.add (new Pill());
        p->getProperties().set ("lane", (int) l);
        p->text = [l] { return juce::String (laneName (l)); };
        p->look = [this, l] { auto g = ctx.globalsCopy(); return chipLook (laneFlag (g, l), ctx.modeAccent()); };
        p->onClick = [this, l] { ctx.edit (false, [l] (Project& pr) { auto& f = laneFlag (pr.globals, l); f = ! f; }); };
        addAndMakeVisible (p);
    }
    for (auto* p : laneChips) { p->font = [] { return Fonts::monoSemi (11.0f); }; p->radius = 5.0f; }

    for (auto l : { Lane::note, Lane::octave, Lane::velocity, Lane::gate, Lane::probability, Lane::cc })
        addAndMakeVisible (ranges.add (new RangeBar (ctx, l)));

    randomBtn.text = [] { return juce::String ("RANDOMIZE"); };
    randomBtn.font = [] { return Fonts::sansBold (12.0f).withExtraKerningFactor (0.12f); };
    randomBtn.radius = 6.0f;
    randomBtn.look = [this] { return Pill::filled (ctx.modeAccent(), Theme::current().onAccent); };
    randomBtn.onClick = [this]
    {
        const auto sc = ctx.scope();
        const bool cc = ctx.ui.ccMode;
        juce::Random r;
        ctx.edit (true, [&] (Project& p) { randomize (p, sc, cc, r); });
    };
    addAndMakeVisible (randomBtn);

    smoothBtn.text = [] { return juce::String ("SMOOTH"); };
    smoothBtn.font = [] { return Fonts::sansBold (12.0f).withExtraKerningFactor (0.12f); };
    smoothBtn.radius = 6.0f;
    smoothBtn.look = [this] { return Pill::outline (ctx.modeAccent(), ctx.modeAccent()); };
    smoothBtn.onPress = [this]
    {
        ctx.beginGesture();   // one press or one hold = one undo entry
        smoothOnce();
        repeating = false;
        startTimer (400);
    };
    smoothBtn.onRelease = [this] { stopTimer(); };
    addAndMakeVisible (smoothBtn);

    addAndMakeVisible (shiftKnob);
    for (auto l : { Lane::note, Lane::octave, Lane::velocity, Lane::gate, Lane::probability, Lane::cc })
    {
        auto* p = shiftChips.add (new Pill());
        p->getProperties().set ("lane", (int) l);
        p->radius = 4.0f;
        p->font = [] { return Fonts::monoSemi (10.0f); };
        p->text = [l] { return juce::String (laneName (l)); };
        p->look = [this, l] { return chipLook (ctx.ui.ccMode ? l == Lane::cc : ctx.ui.shiftLane == l, ctx.modeAccent()); };
        p->onClick = [this, l] { if (! ctx.ui.ccMode) { ctx.ui.shiftLane = l; ctx.changed(); } };
        addAndMakeVisible (p);
    }
}

void OpsPanel::smoothOnce()
{
    const auto sc = ctx.scope();
    const bool cc = ctx.ui.ccMode;
    ctx.edit (false, [&] (Project& p) { smooth (p, sc, cc); });
}

void OpsPanel::timerCallback()
{
    if (! repeating)
    {
        repeating = true;
        startTimer (70);
    }
    smoothOnce();
}

void OpsPanel::refreshState()
{
    const bool cc = ctx.ui.ccMode;
    for (auto* p : laneChips)
    {
        const int l = p->getProperties()["lane"];
        p->setVisible (l < 0 || (cc ? (l == (int) Lane::on || l == (int) Lane::cc) : l != (int) Lane::cc));
    }
    for (auto* r : ranges)
    {
        juce::ignoreUnused (r);
    }
    for (int i = 0; i < ranges.size(); ++i)
        ranges[i]->setVisible (cc ? i == 5 : i < 5);
    for (auto* p : shiftChips)
    {
        const int l = p->getProperties()["lane"];
        p->setVisible (cc ? l == (int) Lane::cc : l != (int) Lane::cc);
    }
    resized();
    repaint();
}

void OpsPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    const float total = (float) r.getWidth() - 4 * 16.0f;
    const float fr = total / (1.05f + 0.95f + 1.25f + 0.62f + 0.95f);
    auto take = [&] (float f)
    {
        auto c = r.removeFromLeft (juce::roundToInt (f * fr));
        r.removeFromLeft (16);
        return c;
    };
    selCol = take (1.05f);
    lanesCol = take (0.95f);
    rangesCol = take (1.25f);
    buttonsCol = take (0.62f);
    shiftCol = r;

    {
        auto c = selCol.withTrimmedTop (12 + 10 + 18 + 10).withHeight (28);
        copyBtn.setBounds (c.removeFromLeft (64)); c.removeFromLeft (6);
        pasteBtn.setBounds (c.removeFromLeft (84)); c.removeFromLeft (6);
        clearBtn.setBounds (c.removeFromLeft (64));
    }
    {
        auto c = lanesCol.withTrimmedTop (20);
        int x = c.getX(), y = c.getY();
        for (auto* p : laneChips)
        {
            if (! p->isVisible()) continue;
            const int w = juce::GlyphArrangement::getStringWidthInt (Fonts::monoSemi (11.0f), p->text ? p->text() : juce::String()) + 22;
            if (x + w > c.getRight()) { x = c.getX(); y += 34; }
            p->setBounds (x, y, w, 28);
            x += w + 6;
        }
    }
    {
        auto c = rangesCol.withTrimmedTop (20);
        for (auto* rb : ranges)
        {
            if (! rb->isVisible()) continue;
            rb->setBounds (c.removeFromTop (14));
            c.removeFromTop (6);
        }
    }
    {
        auto c = buttonsCol;
        const int h = 36 * 2 + 8;
        auto b = c.withSizeKeepingCentre (c.getWidth(), h);
        randomBtn.setBounds (b.removeFromTop (36));
        b.removeFromTop (8);
        smoothBtn.setBounds (b.removeFromTop (36));
    }
    {
        auto c = shiftCol.withTrimmedLeft (16).withTrimmedTop (20);
        auto row = c.removeFromTop (58);
        shiftKnob.setBounds (row.removeFromLeft (58));
        row.removeFromLeft (12);
        const int cols = 3;
        const int w = (row.getWidth() - 4 * (cols - 1)) / cols;
        int i = 0;
        const int visibleCount = ctx.ui.ccMode ? 1 : 5;
        const int rows = (visibleCount + cols - 1) / cols;
        int y = row.getCentreY() - (rows * 22 + (rows - 1) * 4) / 2;
        for (auto* p : shiftChips)
        {
            if (! p->isVisible()) continue;
            p->setBounds (row.getX() + (i % cols) * (w + 4), y + (i / cols) * 26, w, 22);
            ++i;
        }
    }
}

void OpsPanel::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    draw::tile (g, getLocalBounds().toFloat(), t.panel, t.border, 8.0f);

    draw::label (g, "SELECTION", selCol.withHeight (12), t.muted);
    {
        // "LENGTH: 7, STEPS: 4": the clipboard while COPIED, else the selection (or all steps)
        const bool copied = ! ctx.ui.clip.empty();
        int span = 0, count = 0;
        if (copied)
        {
            span = ctx.ui.clipSpan;
            count = (int) ctx.ui.clip.size();
        }
        else
        {
            const auto sc = ctx.scope();
            int last = -1;
            for (int i = 0; i < maxSteps; ++i) if (sc.contains (i)) last = i;
            span = last >= 0 ? last - sc.first() + 1 : 0;
            count = sc.count();
        }
        const auto text = "LENGTH: " + juce::String (span) + ", STEPS: " + juce::String (count);
        g.setColour (copied || ctx.hasSelection() ? t.text : t.muted);
        g.setFont (Fonts::monoSemi (13.0f));
        g.drawText (text, selCol.withTrimmedTop (22).withHeight (18), juce::Justification::centredLeft);
    }
    draw::label (g, "LANES", lanesCol.withHeight (12), t.muted);
    draw::label (g, "RANGES", rangesCol.withHeight (12), t.muted);
    g.setColour (t.border);
    g.fillRect (shiftCol.getX(), shiftCol.getY(), 1, shiftCol.getHeight());
    draw::label (g, "SHIFT ALL", shiftCol.withTrimmedLeft (16).withHeight (12), t.muted);
}
} // namespace vima
