#include "StepGrid.h"

namespace vima
{
// ---------------------------------------------------------------------------------------
void LaneSlider::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    auto b = getLocalBounds().toFloat();
    g.setColour (t.sliderTrack);
    g.fillRoundedRectangle (b, 4.0f);
    const int v = getValue ? getValue() : 0;
    const float f = maxValue > minValue ? (float) (v - minValue) / (float) (maxValue - minValue) : 0.0f;
    auto fill = b.withTrimmedTop (b.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, f)));
    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle (b, 4.0f);
    g.reduceClipRegion (clip);
    g.setColour (barColour ? barColour() : t.green);
    g.fillRect (fill);
    g.restoreState();
}

void LaneSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isAltDown())
    {
        Knob::mouseDown (e);   // reset to default
        return;
    }
    drawing = true;
    if (onGestureStart) onGestureStart();   // the whole draw is one undo step
    if (onDraw) onDraw (e.getScreenPosition(), true);
}

void LaneSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (drawing && onDraw) onDraw (e.getScreenPosition(), false);
}

void LaneSlider::mouseUp (const juce::MouseEvent&)
{
    if (drawing && onGestureEnd) onGestureEnd();
    drawing = false;
}

int LaneSlider::valueAtScreenY (int screenY) const
{
    const auto b = getScreenBounds();
    const float f = 1.0f - (float) (screenY - b.getY()) / (float) juce::jmax (1, b.getHeight());
    return juce::jlimit (minValue, maxValue, juce::roundToInt ((float) minValue + juce::jlimit (0.0f, 1.0f, f) * (float) (maxValue - minValue)));
}

// ---------------------------------------------------------------------------------------
static int soundingPitch (Context& ctx, int index)
{
    const int t = ctx.ui.track;
    return ctx.proc.read ([t, index] (const Project& p) { return soundingNote (p, p.currentPattern, t, index); });
}

StepCell::StepCell (Context& c, int i) : ctx (c), index (i)
{
    // On/off button with the step number
    toggle.radius = 4.0f;
    toggle.font = [] { return Fonts::monoSemi (11.0f).withExtraKerningFactor (0.04f); };
    toggle.text = [i] { return juce::String (i + 1).paddedLeft ('0', 2); };
    toggle.look = [this]
    {
        const auto& t = Theme::current();
        // on = accent fill; off (within the length) = grey fill; beyond the length = outline only.
        // The playhead is shown on the LEDs only.
        const auto st = stepCopy();
        const bool on = ctx.ui.ccMode ? st.ccOn : st.on;   // each part has its own on/off
        const bool beyond = index >= ctx.trackCopy().length;
        const auto accent = ctx.modeAccent();
        if (on && ! beyond) return Pill::Look { accent, accent, t.onAccent, 1.5f, false };
        if (! beyond) return Pill::Look { t.ledOff, t.ledOff, t.text2, 1.5f, false };
        return Pill::Look { juce::Colours::transparentBlack, on ? accent : t.dim, on ? accent : t.muted, 1.5f, false };
    };
    toggle.onClick = [this]
    {
        const bool cc = ctx.ui.ccMode;
        ctx.edit (true, [this, cc] (Project& p)
        {
            auto& s = ctx.liveTrack (p).steps[(size_t) index];
            if (cc) s.ccOn = ! s.ccOn; else s.on = ! s.on;
        });
    };
    addAndMakeVisible (toggle);

    auto gestureStart = [this] { ctx.beginGesture(); };

    // Pitch: moves through in-scale notes when the scale is on; shows the note that will sound
    pitch.thickness = 4.0f;
    pitch.fontSize = 12.0f;
    pitch.colour = [this] { return ctx.modeAccent(); };
    pitch.getValue = [this] { return ((soundingPitch (ctx, index) % 12) + 12) % 12; };
    pitch.fraction = [this]
    {
        // With the track's scale on, the in-scale notes are spread over the whole arc
        const int pc = ((soundingPitch (ctx, index) % 12) + 12) % 12;
        const auto g = ctx.globalsCopy();
        if (! ctx.trackCopy().scaleOn)
            return (float) pc / 11.0f;
        const auto mask = g.scaleMask();
        int count = 0, pos = -1;
        for (int n = 0; n < 12; ++n)
            if (inScale (mask, g.root, n)) { if (n == pc) pos = count; ++count; }
        if (pos < 0 || count < 2)
            return count < 2 ? 0.0f : (float) pc / 11.0f;
        return (float) pos / (float) (count - 1);
    };
    pitch.format = [] (int pc) { return juce::String (pitchClassName (pc)); };
    pitch.nudge = [this] (int d)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, d, this] (Project& p) { nudgePitchClass (p, p.currentPattern, t, index, d); });
    };
    pitch.parse = [] (const juce::String& s)
    {
        const auto u = s.toUpperCase().trim();
        for (int pc = 11; pc >= 0; --pc)
            if (u.startsWith (pitchClassName (pc)) && (juce::String (pitchClassName (pc)).length() == 2 || ! u.contains ("#")))
                return pc;
        return 0;
    };
    pitch.minValue = 0; pitch.maxValue = 11; pitch.defaultValue = 5;   // F, the middle of the knob (init value)
    pitch.setValue = [this] (int pc)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, pc, this] (Project& p) { setPitchClass (p, p.currentPattern, t, index, pc); });
    };
    pitch.onGestureStart = gestureStart;
    pitch.pixelsPerStep = 8.0f;
    addAndMakeVisible (pitch);

    // Octave: shows the octave that will sound (middle C setting applied)
    octave.thickness = 3.0f;
    octave.fontSize = 11.0f;
    octave.textColour = Theme::current().text2;
    octave.colour = [] { return Theme::current().text2; };
    octave.getValue = [this]
    {
        const auto g = ctx.globalsCopy();
        return (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2 + g.octaveOffset();
    };
    octave.fraction = [this]
    {
        const auto g = ctx.globalsCopy();
        const int o = (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2;
        juce::ignoreUnused (g);
        return (float) (o - minOctave) / (float) (maxOctave - minOctave);
    };
    octave.nudge = [this] (int d)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, d, this] (Project& p) { nudgeOctave (p, p.currentPattern, t, index, d); });
    };
    octave.minValue = minOctave; octave.maxValue = maxOctave + 2; octave.defaultValue = 3;
    octave.setValue = [this] (int shown)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, shown, this] (Project& p)
        {
            const int o = shown - p.globals.octaveOffset();
            p.patterns[(size_t) p.currentPattern].tracks[(size_t) t].steps[(size_t) index].octave
                = (std::int8_t) juce::jlimit (minOctave, maxOctave, o);
        });
    };
    octave.onGestureStart = gestureStart;
    octave.pixelsPerStep = 10.0f;
    addAndMakeVisible (octave);

    // Velocity / gate / probability slider
    slider.getValue = [this]
    {
        const auto s = stepCopy();
        switch (ctx.ui.slider)
        {
            case SliderLane::velocity:    return (int) s.velocity;
            case SliderLane::gate:        return (int) s.gate;
            case SliderLane::probability: return (int) s.probability;
        }
        return 0;
    };
    slider.setValue = [this] (int v)
    {
        const auto lane = ctx.ui.slider;
        ctx.edit (false, [this, lane, v] (Project& p)
        {
            auto& s = ctx.liveTrack (p).steps[(size_t) index];
            if (lane == SliderLane::velocity) s.velocity = (std::uint8_t) juce::jlimit (1, 127, v);
            else if (lane == SliderLane::gate) s.gate = (std::uint16_t) juce::jlimit (1, maxGate, v);
            else s.probability = (std::uint8_t) juce::jlimit (0, 100, v);
        });
    };
    slider.barColour = [this]
    {
        const auto& t = Theme::current();
        return ctx.ui.slider == SliderLane::velocity ? t.green : ctx.ui.slider == SliderLane::gate ? t.purple : t.amber;
    };
    slider.onGestureStart = gestureStart;
    slider.onDraw = [this] (juce::Point<int> p, bool start) { if (auto* grid = dynamic_cast<StepGrid*> (getParentComponent())) grid->drawSliders (p, start, index / 16); };
    slider.pixelsPerStep = 1.5f;
    slider.format = [] (int v) { return juce::String (v); };
    addAndMakeVisible (slider);

    // CC value (CC mode)
    cc.thickness = 5.0f;
    cc.fontSize = 14.0f;
    cc.minValue = 0; cc.maxValue = 127; cc.defaultValue = 64;
    cc.colour = [] { return Theme::current().cyan; };
    cc.getValue = [this] { return (int) stepCopy().cc; };
    cc.setValue = [this] (int v) { ctx.edit (false, [this, v] (Project& p) { ctx.liveTrack (p).steps[(size_t) index].cc = (std::uint8_t) v; }); };
    cc.onGestureStart = gestureStart;
    cc.pixelsPerStep = 3.0f;
    addAndMakeVisible (cc);

    // LEARN targets (SPEC.md 13.1)
    getProperties().set ("learnStep", index);
    setLearnTarget (toggle, MacroTarget::stepOn);
    setLearnTarget (pitch, MacroTarget::note);
    setLearnTarget (octave, MacroTarget::octave);
    setLearnTarget (slider, -1);
    setLearnTarget (cc, MacroTarget::cc);

    refreshState();
}

void StepCell::drawAt (juce::Point<int> screenPos)
{
    if (index >= ctx.trackCopy().length || ! slider.setValue)
        return;
    slider.setValue (slider.valueAtScreenY (screenPos.y));
}

Step StepCell::stepCopy() const
{
    const int t = ctx.ui.track;
    return ctx.proc.read ([t, this] (const Project& p) { return p.patterns[(size_t) p.currentPattern].tracks[(size_t) t].steps[(size_t) index]; });
}

void StepCell::refreshState()
{
    const bool cc_ = ctx.ui.ccMode;
    pitch.setVisible (! cc_);
    octave.setVisible (! cc_);
    slider.setVisible (! cc_);
    cc.setVisible (cc_);

    const auto lane = ctx.ui.slider;
    slider.minValue = lane == SliderLane::probability ? 0 : 1;
    slider.maxValue = lane == SliderLane::velocity ? 127 : lane == SliderLane::gate ? maxGate : 100;
    slider.defaultValue = 100;   // init values: velocity 100, gate 100 %, probability 100 %
    octave.defaultValue = 3 + ctx.globalsCopy().octaveOffset();   // the octave of middle C, as shown
    octave.textColour = Theme::current().text2;

    const auto tp = ctx.trackCopy();
    const bool beyond = index >= tp.length;
    const auto& stp = tp.steps[(size_t) index];
    const bool on = (cc_ ? stp.ccOn : stp.on) && ! beyond;
    setLearnTarget (toggle, cc_ ? MacroTarget::ccStepOn : MacroTarget::stepOn);
    setAlpha (beyond ? 0.35f : 1.0f);
    const float a = on ? 1.0f : 0.32f;
    pitch.setAlpha (a);
    octave.setAlpha (a);
    slider.setAlpha (a);
    cc.setAlpha (a);
    repaint();
}

void StepCell::resized()
{
    auto b = getLocalBounds().reduced (5, 0).withTrimmedTop (5).withTrimmedBottom (6);
    toggle.setBounds (b.removeFromTop (22));
    b.removeFromTop (4);
    auto rest = b;
    pitch.setBounds (b.removeFromTop (46).withSizeKeepingCentre (46, 46));
    b.removeFromTop (4);
    octave.setBounds (b.removeFromTop (34).withSizeKeepingCentre (34, 34));
    b.removeFromTop (4);
    labelArea = b.removeFromBottom (13);
    b.removeFromBottom (4);
    slider.setBounds (b.withTrimmedTop (2).reduced (9, 0));
    cc.setBounds (rest.withSizeKeepingCentre (56, 56));
}

void StepCell::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const bool beyond = index >= ctx.trackCopy().length;
    const bool alt = (index / 4) % 2 == 1;
    draw::tile (g, getLocalBounds().toFloat(), beyond ? t.beyond : (alt ? t.tileAlt : t.tile), t.border3);

    if (! ctx.ui.ccMode)
    {
        const auto s = stepCopy();
        juce::String text;
        switch (ctx.ui.slider)
        {
            case SliderLane::velocity:    text = juce::String ((int) s.velocity); break;
            case SliderLane::gate:        text = juce::String ((int) s.gate) + "%"; break;
            case SliderLane::probability: text = juce::String ((int) s.probability) + "%"; break;
        }
        g.setColour (t.muted.withMultipliedAlpha (s.on ? 1.0f : 0.32f));
        g.setFont (Fonts::mono (10.0f));
        g.drawText (text, labelArea, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------
StepGrid::StepGrid (Context& c) : ctx (c)
{
    for (int i = 0; i < maxSteps; ++i)
        addAndMakeVisible (cells.add (new StepCell (ctx, i)));
}

static int columnWidth (int totalWidth) { return (totalWidth - 15 * 6); }

juce::Rectangle<int> StepGrid::ledBounds (int step) const
{
    const int row = step / 16, col = step % 16;
    const int total = columnWidth (getWidth());
    const int x0 = (int) std::round (col * (total / 16.0 + 6.0));
    const int x1 = (int) std::round ((col + 1) * (total / 16.0 + 6.0) - 6.0);
    const int y = row * (ledHeight + ledGap + cellHeight + rowGap);
    return { x0, y, x1 - x0, ledHeight };
}

juce::Rectangle<int> StepGrid::cellBounds (int step) const
{
    auto l = ledBounds (step);
    return { l.getX(), l.getBottom() + ledGap, l.getWidth(), cellHeight };
}

void StepGrid::resized()
{
    for (int i = 0; i < maxSteps; ++i)
        cells[i]->setBounds (cellBounds (i));
}

void StepGrid::refreshCells()
{
    for (auto* c : cells) c->refreshState();
    repaint();
}

void StepGrid::drawSliders (juce::Point<int> screenPos, bool start, int row)
{
    // follow the whole mouse path, so a fast drag does not skip steps
    if (start)
        drawRow = row;
    else
    {
        const auto from = lastDraw.toFloat(), to = screenPos.toFloat();
        const int n = juce::jmax (1, (int) (from.getDistanceFrom (to) / 4.0f));
        for (int k = 1; k < n; ++k)
            drawOne ((from + (to - from) * ((float) k / (float) n)).roundToInt());
    }
    drawOne (screenPos);
    lastDraw = screenPos;
}

void StepGrid::drawOne (juce::Point<int> screenPos)
{
    const auto p = getLocalPoint (nullptr, screenPos);
    const int row = drawRow;
    for (int col = 0; col < 16; ++col)
    {
        const auto b = cellBounds (row * 16 + col);
        if (p.x >= b.getX() - 3 && p.x < b.getRight() + 3)
        {
            cells[row * 16 + col]->drawAt (screenPos);
            return;
        }
    }
}

void StepGrid::repaintLeds()
{
    repaint (0, 0, getWidth(), ledHeight + 1);
    repaint (0, ledHeight + ledGap + cellHeight + rowGap, getWidth(), ledHeight + 1);
    for (auto* c : cells) c->repaint (0, 0, c->getWidth(), 30);   // step number outline follows the playhead
}

int StepGrid::ledIndexAt (juce::Point<int> p, bool clampToRow) const
{
    const int rowPitch = ledHeight + ledGap + cellHeight + rowGap;
    int row = p.y >= rowPitch - rowGap / 2 ? 1 : 0;
    if (! clampToRow)
    {
        const int yInRow = p.y - row * rowPitch;
        if (yInRow < 0 || yInRow >= ledHeight) return -1;
    }
    const double colW = columnWidth (getWidth()) / 16.0 + 6.0;
    const int col = juce::jlimit (0, 15, (int) std::floor ((p.x + 3) / colW));
    return row * 16 + col;
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = ledIndexAt (e.getPosition(), false);
    if (i < 0) return;
    const int len = ctx.trackCopy().length;
    if (i >= len) return;

    if (! ctx.ui.clip.empty())
    {
        // COPIED: no selection, the click picks where the paste lands
        ctx.ui.pasteStart = i;
        dragFrom = -1;
        ctx.changed();
        return;
    }

    dragFrom = i;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        // Ctrl-click (Cmd-click on Mac): add or remove this step; Ctrl-drag adds a range
        if ((ctx.ui.selection >> i) & 1u)
        {
            ctx.ui.selection &= ~(1u << i);   // already selected: remove it
            dragFrom = -1;
        }
        else
        {
            dragBase = ctx.ui.selection;      // add it; a drag from here adds a range
            ctx.ui.selection |= (1u << i);
        }
    }
    else
    {
        dragBase = 0;
        ctx.ui.selection = 1u << i;
    }
    ctx.changed();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    const int len = ctx.trackCopy().length;
    const int i = juce::jlimit (0, len - 1, ledIndexAt (e.getPosition(), true));
    ctx.ui.selection = dragBase | rangeMask (dragFrom, i);
    repaintLeds();
    if (ctx.refresh) ctx.refresh();
}

void StepGrid::mouseUp (const juce::MouseEvent&) { dragFrom = -1; }

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const int i = ledIndexAt (e.getPosition(), false);
    setMouseCursor (i >= 0 ? (! ctx.ui.clip.empty() ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::PointingHandCursor)
                           : juce::MouseCursor::NormalCursor);
}

void StepGrid::mouseExit (const juce::MouseEvent&) {}

void StepGrid::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const int len = ctx.trackCopy().length;
    const auto accent = ctx.modeAccent();
    const bool playing = ctx.proc.engine.isPlaying;
    const int play = playing ? ctx.proc.engine.playStep[(size_t) ctx.ui.track].load() : -1;
    const bool copied = ! ctx.ui.clip.empty();
    const auto sel = copied ? 0u : ctx.ui.selection & rangeMask (0, len - 1);
    std::uint32_t ghostMask = 0;
    if (copied && ctx.ui.pasteStart >= 0)
        for (const auto& c : ctx.ui.clip)
            if (ctx.ui.pasteStart + c.offset < len) ghostMask |= (1u << (ctx.ui.pasteStart + c.offset));

    for (int i = 0; i < maxSteps; ++i)
    {
        auto b = ledBounds (i).toFloat();
        auto bar = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
        const bool beyond = i >= len;
        const bool ghost = ((ghostMask >> i) & 1u) != 0;
        const bool selected = ((sel >> i) & 1u) != 0;
        juce::Colour c = beyond ? t.beyond : t.border3;
        if (selected) c = t.selection;
        if (i == play && ! beyond) c = accent;
        if (ghost) c = accent.withAlpha (0.55f);
        if (i == play && ! beyond && ! ghost)
        {
            g.setColour (accent.withAlpha (0.3f));
            g.fillRoundedRectangle (bar.expanded (2.0f, 3.0f), 5.0f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (bar, 3.0f);
    }
}
} // namespace vima
