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
            case Lane::ccOn:        return g.laneCcOn;
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
            case Lane::on: case Lane::ccOn: break;
        }
        return g.rangeVel;
    }

    const char* laneName (Lane l)
    {
        switch (l)
        {
            case Lane::on:
            case Lane::ccOn:        return "ON";
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
            case Lane::note: case Lane::octave: case Lane::velocity: return t.green;
            case Lane::gate:        return t.purple;
            case Lane::probability: return t.amber;
            case Lane::cc:          return t.cyan;
            case Lane::on: case Lane::ccOn: break;
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
        static const std::vector<Lane> l { Lane::ccOn, Lane::cc };
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

// NOTE: the notes the bar shows. With the current track's scale on, only the in-scale notes,
// spread over the whole bar (like the note knob arc, SPEC.md 5); otherwise C to B.
std::vector<int> RangeBar::notePositions() const
{
    std::vector<int> pcs;
    const auto g = ctx.globalsCopy();
    const bool scaleOn = ctx.trackCopy().scaleOn;
    const auto mask = g.scaleMask();
    for (int pc = 0; pc < 12; ++pc)
        if (! scaleOn || inScale (mask, g.root, pc)) pcs.push_back (pc);
    if (pcs.empty()) pcs.push_back (g.root);
    return pcs;
}

// Ranges in bar units: for NOTE, positions in notePositions(); for the other params, values
LaneRange RangeBar::get() const
{
    auto g = ctx.globalsCopy();
    const auto r = laneRange (g, lane);
    if (lane != Lane::note)
        return r;
    const auto pcs = notePositions();
    const int n = (int) pcs.size();
    const int lo = juce::jmin (r.min, r.max), hi = juce::jmax (r.min, r.max);
    int a = n - 1, b = 0;
    for (int i = 0; i < n; ++i) if (pcs[(size_t) i] >= lo) { a = i; break; }
    for (int i = n - 1; i >= 0; --i) if (pcs[(size_t) i] <= hi) { b = i; break; }
    if (b < a) b = a;
    return { a, b };
}

void RangeBar::set (LaneRange r)
{
    if (lane == Lane::note)
    {
        const auto pcs = notePositions();
        const int n = (int) pcs.size();
        r = { pcs[(size_t) juce::jlimit (0, n - 1, r.min)], pcs[(size_t) juce::jlimit (0, n - 1, r.max)] };
    }
    ctx.edit (false, [this, r] (Project& p) { laneRange (p.globals, lane) = r; });
}

bool RangeBar::armed() const
{
    const auto lanes = activeLanes (ctx.globalsCopy(), ctx.ui.ccMode);
    return std::find (lanes.begin(), lanes.end(), lane) != lanes.end();
}

void RangeBar::axis (int& lo, int& hi) const
{
    switch (lane)
    {
        case Lane::note:        lo = 0; hi = (int) notePositions().size() - 1; return;
        case Lane::octave:      lo = minOctave; hi = maxOctave; return;
        case Lane::velocity:    lo = 1; hi = 127; return;
        case Lane::gate:        lo = 1; hi = maxGate; return;
        case Lane::probability: lo = 1; hi = 100; return;
        case Lane::cc:          lo = 0; hi = 127; return;
        case Lane::on: case Lane::ccOn: lo = 0; hi = 1; return;
    }
}

juce::String RangeBar::valueText (int v) const
{
    switch (lane)
    {
        case Lane::note:
        {
            const auto pcs = notePositions();
            return pitchClassName (pcs[(size_t) juce::jlimit (0, (int) pcs.size() - 1, v)]);
        }
        case Lane::octave: return juce::String (v + ctx.globalsCopy().octaveOffset());
        case Lane::gate: case Lane::probability: return juce::String (v) + "%";
        case Lane::on: case Lane::ccOn: case Lane::velocity: case Lane::cc: break;
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
    // ranges of unarmed params are dimmed (SPEC.md 12.6)
    g.beginTransparencyLayer (armed() ? 1.0f : 0.35f);
    paintBar (g);
    g.endTransparencyLayer();
}

void RangeBar::paintBar (juce::Graphics& g)
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
    pasteBtn.pressedLook = [] { const auto& t = Theme::current(); return Pill::filled (t.green, t.onAccent); };   // green while held (SPEC.md 9.1)
    addAndMakeVisible (pasteBtn);

    clearBtn.text = [] { return juce::String ("CLEAR"); };
    clearBtn.font = smallFont;
    clearBtn.look = plain;
    // CLEAR: deselect; when COPIED, also empty the clipboard (and the ghost preview)
    clearBtn.alpha = [this] { return (ctx.hasSelection() || ! ctx.ui.clip.empty()) ? 1.0f : 0.4f; };
    clearBtn.pressedLook = [] { const auto& t = Theme::current(); return Pill::filled (t.green, t.onAccent); };
    clearBtn.onClick = [this]
    {
        ctx.ui.selection = 0;
        ctx.ui.clip.clear();
        ctx.ui.pasteStart = -1;
        ctx.changed();
    };
    addAndMakeVisible (clearBtn);

    // PARAMS buttons (ALL first). Built for note mode; CC mode shows ALL, ON, CC.
    std::vector<Lane> all { Lane::on, Lane::ccOn, Lane::note, Lane::octave, Lane::velocity, Lane::gate, Lane::probability, Lane::cc };
    {
        auto* p = laneChips.add (new Pill());
        p->text = [] { return juce::String ("ALL"); };
        p->getProperties().set ("lane", -1);
        // ALL is its own state: only ALL lights up. Clicking the lit ALL does nothing
        // (something is always armed, SPEC.md 9.2).
        p->look = [this]
        {
            const auto g = ctx.globalsCopy();
            return chipLook (ctx.ui.ccMode ? g.laneAllCc : g.laneAllNote, ctx.modeAccent());
        };
        p->onClick = [this]
        {
            const bool cc = ctx.ui.ccMode;
            ctx.edit (false, [cc] (Project& pr)
            {
                auto& all = cc ? pr.globals.laneAllCc : pr.globals.laneAllNote;
                if (all) return;
                all = true;
                for (auto l : cc ? ccLanes() : noteLanes()) laneFlag (pr.globals, l) = false;
            });
        };
        addAndMakeVisible (p);
    }
    for (auto l : all)
    {
        auto* p = laneChips.add (new Pill());
        p->getProperties().set ("lane", (int) l);
        p->text = [l] { return juce::String (laneName (l)); };
        p->look = [this, l]
        {
            auto g = ctx.globalsCopy();
            const bool all = ctx.ui.ccMode ? g.laneAllCc : g.laneAllNote;
            return chipLook (! all && laneFlag (g, l), ctx.modeAccent());
        };
        p->onClick = [this, l]
        {
            const bool cc = ctx.ui.ccMode;
            ctx.edit (false, [l, cc] (Project& pr)
            {
                auto& g = pr.globals;
                auto& all = cc ? g.laneAllCc : g.laneAllNote;
                const auto& modeLanes = cc ? ccLanes() : noteLanes();
                if (all)
                {
                    // leaving ALL: only the clicked param is armed
                    all = false;
                    for (auto m : modeLanes) laneFlag (g, m) = false;
                    laneFlag (g, l) = true;
                    return;
                }
                auto& f = laneFlag (g, l);
                if (f)
                {
                    int armed = 0;
                    for (auto m : modeLanes) armed += laneFlag (g, m) ? 1 : 0;
                    if (armed <= 1) return;   // the last armed param stays armed
                }
                f = ! f;
            });
        };
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

    // Curves, rotate, invert (SPEC.md 9.5): act on the scope and the armed params, one undo step each
    {
        auto stroke = [] (juce::Graphics& g, const juce::Path& p, juce::Colour c)
        {
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        auto box = [] (juce::Rectangle<float> b) { return b.withSizeKeepingCentre (juce::jmin (b.getWidth() - 8.0f, 16.0f), 10.0f); };
        std::vector<std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>> icons;
        icons.push_back ([=] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
        {
            const auto r = box (b); juce::Path p;
            for (int i = 0; i <= 24; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / 24.0f;
                const float y = r.getCentreY() - std::sin ((float) i / 24.0f * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            stroke (g, p, c);
        });
        icons.push_back ([=] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
        {
            const auto r = box (b); juce::Path p;
            p.startNewSubPath (r.getX(), r.getBottom()); p.lineTo (r.getCentreX(), r.getY()); p.lineTo (r.getRight(), r.getBottom());
            stroke (g, p, c);
        });
        icons.push_back ([=] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
        {
            const auto r = box (b); juce::Path p;
            p.startNewSubPath (r.getX(), r.getBottom()); p.lineTo (r.getRight(), r.getY()); p.lineTo (r.getRight(), r.getBottom());
            stroke (g, p, c);
        });
        icons.push_back ([=] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
        {
            const auto r = box (b); juce::Path p;
            p.startNewSubPath (r.getX(), r.getBottom()); p.lineTo (r.getX(), r.getY()); p.lineTo (r.getCentreX(), r.getY());
            p.lineTo (r.getCentreX(), r.getBottom()); p.lineTo (r.getRight(), r.getBottom()); p.lineTo (r.getRight(), r.getY());
            stroke (g, p, c);
        });
        for (int i = 0; i < 7; ++i)
        {
            auto* p = shapeBtns.add (new Pill());
            p->radius = 5.0f;
            p->look = [this] { return Pill::outline (ctx.modeAccent().withAlpha (0.6f), ctx.modeAccent()); };
            p->pressedLook = [] { const auto& t = Theme::current(); return Pill::filled (t.green, t.onAccent); };
            p->font = [] { return Fonts::monoSemi (11.0f); };
            if (i < 4) p->icon = icons[(size_t) i];
            else p->text = [i] { return juce::String (i == 4 ? "<" : i == 5 ? ">" : "INV"); };
            static const char* titles[] = { "Sine", "Triangle", "Ramp", "Square", "Rotate left", "Rotate right", "Invert" };
            p->setTitle (titles[i]);
            p->onClick = [this, i]
            {
                const auto sc = ctx.scope();
                const bool cc = ctx.ui.ccMode;
                ctx.edit (true, [&] (Project& pr)
                {
                    if (i < 4) applyCurve (pr, sc, cc, (CurveShape) i);
                    else if (i < 6) rotateSteps (pr, sc, cc, i == 5 ? 1 : -1);
                    else invertSteps (pr, sc, cc);
                });
            };
            addAndMakeVisible (p);
        }
    }

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
        p->setVisible (l < 0 || (cc ? (l == (int) Lane::ccOn || l == (int) Lane::cc) : (l != (int) Lane::cc && l != (int) Lane::ccOn)));
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
        const int h = 30 * 2 + 6 + 8 + 24 * 2 + 4;
        auto b = c.withSizeKeepingCentre (c.getWidth(), h);
        randomBtn.setBounds (b.removeFromTop (30));
        b.removeFromTop (6);
        smoothBtn.setBounds (b.removeFromTop (30));
        b.removeFromTop (8);
        auto row1 = b.removeFromTop (24);
        b.removeFromTop (4);
        auto row2 = b.removeFromTop (24);
        const int w1 = (row1.getWidth() - 3 * 4) / 4;
        for (int i = 0; i < 4; ++i) shapeBtns[i]->setBounds (row1.getX() + i * (w1 + 4), row1.getY(), w1, 24);
        const int w2 = (row2.getWidth() - 2 * 4) / 3;
        for (int i = 0; i < 3; ++i) shapeBtns[4 + i]->setBounds (row2.getX() + i * (w2 + 4), row2.getY(), w2, 24);
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
    draw::label (g, "PARAMS", lanesCol.withHeight (12), t.muted);
    draw::label (g, "RANGES", rangesCol.withHeight (12), t.muted);
    g.setColour (t.border);
    g.fillRect (shiftCol.getX(), shiftCol.getY(), 1, shiftCol.getHeight());
    draw::label (g, "SHIFT ALL", shiftCol.withTrimmedLeft (16).withHeight (12), t.muted);
}
} // namespace vima
