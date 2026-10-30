#include "StepGrid.h"

namespace vima
{
// ---------------------------------------------------------------------------------------
void StepSlider::paint (juce::Graphics& g)
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

void StepSlider::mouseDown (const juce::MouseEvent& e)
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

void StepSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (drawing && onDraw) onDraw (e.getScreenPosition(), false);
}

void StepSlider::mouseUp (const juce::MouseEvent&)
{
    if (drawing && onGestureEnd) onGestureEnd();
    drawing = false;
}

int StepSlider::valueAtScreenY (int screenY) const
{
    const auto b = getScreenBounds();
    const float f = 1.0f - (float) (screenY - b.getY()) / (float) juce::jmax (1, b.getHeight());
    return juce::jlimit (minValue, maxValue, juce::roundToInt ((float) minValue + juce::jlimit (0.0f, 1.0f, f) * (float) (maxValue - minValue)));
}

// ---------------------------------------------------------------------------------------
static int soundingPitch (Context& ctx, int index)
{
    const int t = ctx.ui.track;
    return ctx.proc.read ([&ctx, t, index] (const Project& p) { return soundingNote (p, ctx.shownIn (p), t, index); });
}

StepCell::StepCell (Context& c, int i) : ctx (c), index (i)
{
    // On/off button with the step number
    toggle.radius = 4.0f;
    toggle.hoverHighlight = false;   // step buttons keep their own look (SPEC.md 12.5)
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
    // Press: the step flips; dragging paints the same state over the other step buttons of the row
    toggle.onPress = [this]
    {
        const auto st = stepCopy();
        const bool newValue = ! (ctx.ui.ccMode ? st.ccOn : st.on);
        ctx.beginGesture ("STEP " + juce::String (index + 1).paddedLeft ('0', 2) + (ctx.ui.ccMode ? " CC " : " NOTE ")
                          + (newValue ? "ON" : "OFF") + ctx.where());   // the whole stroke is one undo step
        if (auto* grid = dynamic_cast<StepGrid*> (getParentComponent()))
            grid->drawToggles (toggle.getScreenBounds().getCentre(), true, index / 16, newValue);
    };
    toggle.onDrag = [this] (juce::Point<int> p)
    {
        if (auto* grid = dynamic_cast<StepGrid*> (getParentComponent()))
            grid->drawToggles (p, false, index / 16, false);
    };
    addAndMakeVisible (toggle);

    // undo label per control: "STEP 05 NOTE · T03 · P23"
    auto stepLabel = [this] (const juce::String& what) { return "STEP " + juce::String (index + 1).paddedLeft ('0', 2) + " " + what + ctx.where(); };
    auto sliderName = [this]
    {
        if (ctx.ui.ccMode) return juce::String ("CC");
        return juce::String (ctx.ui.slider == SliderParam::velocity ? "VEL" : ctx.ui.slider == SliderParam::gate ? "GATE" : "PROB");
    };

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
        ctx.edit (false, [t, d, this] (Project& p) { nudgePitchClass (p, ctx.shownIn (p), t, index, d); });
    };
    pitch.parse = [] (const juce::String& s)
    {
        const auto u = s.toUpperCase().trim();
        for (int pc = 11; pc >= 0; --pc)
            if (u.startsWith (pitchClassName (pc)) && (juce::String (pitchClassName (pc)).length() == 2 || ! u.contains ("#")))
                return pc;
        return 0;
    };
    pitch.minValue = 0; pitch.maxValue = 11; pitch.defaultValue = 0;   // C (init value: middle C)
    pitch.setValue = [this] (int pc)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, pc, this] (Project& p) { setPitchClass (p, ctx.shownIn (p), t, index, pc); });
    };
    pitch.onGestureStart = [this, stepLabel] { ctx.beginGesture (stepLabel ("PITCH")); };
    pitch.pixelsPerStep = 8.0f;
    addAndMakeVisible (pitch);

    // Octave: shows the octave that will sound (middle C setting applied)
    octave.thickness = 3.0f;
    octave.fontSize = 11.0f;
    octave.textColour = Theme::current().text2;
    octave.colour = [] { return Theme::current().green; };   // a note param: green (SPEC.md 12.1)
    octave.getValue = [this]
    {
        const auto g = ctx.globalsCopy();
        return (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2 + g.octaveOffset();
    };
    octave.fraction = [this]
    {
        const int o = (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2;
        return (float) (o - minOctave) / (float) (maxOctave - minOctave);
    };
    octave.nudge = [this] (int d)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, d, this] (Project& p) { nudgeOctave (p, ctx.shownIn (p), t, index, d); });
    };
    octave.minValue = minOctave; octave.maxValue = maxOctave + 2; octave.defaultValue = 3;
    octave.setValue = [this] (int shown)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, shown, this] (Project& p)
        {
            const int o = shown - p.globals.octaveOffset();
            p.patterns[(size_t) ctx.shownIn (p)].tracks[(size_t) t].steps[(size_t) index].octave
                = (std::int8_t) juce::jlimit (minOctave, maxOctave, o);
        });
    };
    octave.onGestureStart = [this, stepLabel] { ctx.beginGesture (stepLabel ("OCT")); };
    octave.pixelsPerStep = 10.0f;
    addAndMakeVisible (octave);

    // Step slider: velocity / gate / probability in note mode, the CC value in CC mode
    slider.getValue = [this]
    {
        const auto s = stepCopy();
        if (ctx.ui.ccMode) return (int) s.cc;
        switch (ctx.ui.slider)
        {
            case SliderParam::velocity:    return (int) s.velocity;
            case SliderParam::gate:        return (int) s.gate;
            case SliderParam::probability: return (int) s.probability;
        }
        return 0;
    };
    slider.setValue = [this] (int v)
    {
        const auto param = ctx.ui.slider;
        const bool ccMode = ctx.ui.ccMode;
        ctx.edit (false, [this, param, ccMode, v] (Project& p)
        {
            auto& s = ctx.liveTrack (p).steps[(size_t) index];
            if (ccMode) s.cc = (std::uint8_t) juce::jlimit (0, 127, v);
            else if (param == SliderParam::velocity) s.velocity = (std::uint8_t) juce::jlimit (1, 127, v);
            else if (param == SliderParam::gate) s.gate = (std::uint16_t) juce::jlimit (1, maxGate, v);
            else s.probability = (std::uint8_t) juce::jlimit (1, 100, v);   // never 0
        });
    };
    slider.barColour = [this]
    {
        const auto& t = Theme::current();
        if (ctx.ui.ccMode) return t.cyan;
        return ctx.ui.slider == SliderParam::velocity ? t.green : ctx.ui.slider == SliderParam::gate ? t.purple : t.amber;
    };
    slider.onGestureStart = [this, stepLabel, sliderName] { ctx.beginGesture (stepLabel (sliderName())); };
    slider.onDraw = [this] (juce::Point<int> p, bool start) { if (auto* grid = dynamic_cast<StepGrid*> (getParentComponent())) grid->drawSliders (p, start, index / 16); };
    slider.pixelsPerStep = 1.5f;
    slider.format = [] (int v) { return juce::String (v); };
    addAndMakeVisible (slider);

    // LEARN targets (SPEC.md 13.1)
    getProperties().set ("learnStep", index);
    setLearnTarget (toggle, MacroTarget::stepOn);
    setLearnTarget (pitch, MacroTarget::pitch);
    setLearnTarget (octave, MacroTarget::octave);
    setLearnTarget (slider, -1);

    refreshState();
}

void StepCell::drawAt (juce::Point<int> screenPos)
{
    if (index >= ctx.trackCopy().length || ! slider.setValue)
        return;
    slider.setValue (slider.valueAtScreenY (screenPos.y));
}

void StepCell::setShownOn (bool on)
{
    const bool cc = ctx.ui.ccMode;
    const auto st = stepCopy();
    if ((cc ? st.ccOn : st.on) == on)
        return;   // already in that state: unchanged
    ctx.edit (false, [this, cc, on] (Project& p)
    {
        auto& s = ctx.liveTrack (p).steps[(size_t) index];
        if (cc) s.ccOn = on; else s.on = on;
    });
}

Step StepCell::stepCopy() const
{
    const int t = ctx.ui.track;
    return ctx.proc.read ([t, this] (const Project& p) { return p.patterns[(size_t) ctx.shownIn (p)].tracks[(size_t) t].steps[(size_t) index]; });
}

void StepCell::refreshState()
{
    const bool cc_ = ctx.ui.ccMode;
    pitch.setVisible (! cc_);
    octave.setVisible (! cc_);

    const auto param = ctx.ui.slider;
    slider.minValue = cc_ ? 0 : 1;   // velocity and probability never 0
    slider.maxValue = cc_ ? 127 : param == SliderParam::velocity ? 127 : param == SliderParam::gate ? maxGate : 100;
    slider.defaultValue = cc_ ? 64 : 100;   // init values: CC 64; velocity 100, gate 100 %, probability 100 %
    setLearnTarget (slider, cc_ ? (int) MacroTarget::cc : -1);
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
    resized();
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
    if (ctx.ui.ccMode)   // CC mode: one tall CC slider in place of the note controls
        slider.setBounds (rest.withTrimmedBottom (13 + 4).withTrimmedTop (2).reduced (9, 0));
    else
        slider.setBounds (b.withTrimmedTop (2).reduced (9, 0));
}

void StepCell::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const bool beyond = index >= ctx.trackCopy().length;
    const bool alt = (index / 4) % 2 == 1;
    draw::tile (g, getLocalBounds().toFloat(), beyond ? t.beyond : (alt ? t.tileAlt : t.tile), t.border3);

    {
        const auto s = stepCopy();
        juce::String text;
        if (ctx.ui.ccMode) text = juce::String ((int) s.cc);
        else switch (ctx.ui.slider)
        {
            case SliderParam::velocity:    text = juce::String ((int) s.velocity); break;
            case SliderParam::gate:        text = juce::String ((int) s.gate) + "%"; break;
            case SliderParam::probability: text = juce::String ((int) s.probability) + "%"; break;
        }
        // the value in its param's color (velocity green, gate purple, probability amber, CC cyan)
        const auto colour = slider.barColour ? slider.barColour() : t.muted;
        g.setColour (colour.withMultipliedAlpha ((ctx.ui.ccMode ? s.ccOn : s.on) ? 1.0f : 0.32f));
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

void StepGrid::drawToggles (juce::Point<int> screenPos, bool start, int row, bool on)
{
    if (start)
    {
        drawingToggles = true;
        toggleValue = on;
    }
    stroke (screenPos, start, row);
}

void StepGrid::drawSliders (juce::Point<int> screenPos, bool start, int row)
{
    if (start)
        drawingToggles = false;
    stroke (screenPos, start, row);
}

void StepGrid::stroke (juce::Point<int> screenPos, bool start, int row)
{
    // follow the whole mouse path, so a fast drag does not skip steps
    if (start)
    {
        drawRow = row;
        strokeMin = strokeMax = -1;
    }
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
            const int step = row * 16 + col;
            if (drawingToggles) cells[step]->setShownOn (toggleValue);
            else                cells[step]->drawAt (screenPos);
            const bool grew = strokeMin >= 0 && (step < strokeMin || step > strokeMax);
            strokeMin = strokeMin < 0 ? step : juce::jmin (strokeMin, step);
            strokeMax = juce::jmax (strokeMax, step);
            if (grew) relabelStroke();
            return;
        }
    }
}

void StepGrid::relabelStroke()
{
    // a stroke over several steps: "STEPS 3-9 NOTE ON", "DRAW VEL · STEPS 3-9"
    const auto steps = "STEPS " + juce::String (strokeMin + 1) + "-" + juce::String (strokeMax + 1);
    juce::String label;
    if (drawingToggles)
        label = steps + (ctx.ui.ccMode ? " CC " : " NOTE ") + (toggleValue ? "ON" : "OFF");
    else
        label = "DRAW " + juce::String (ctx.ui.ccMode ? "CC" : ctx.ui.slider == SliderParam::velocity ? "VEL"
                                                         : ctx.ui.slider == SliderParam::gate ? "GATE" : "PROB")
              + Context::dot() + steps;
    ctx.proc.relabelUndo (label + ctx.where());
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
    // the running step light only on the playing pattern (edit mode may show another, SPEC.md 7.5)
    const bool playing = ctx.proc.engine.isPlaying && ctx.showsPlaying();
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
        juce::Colour c = beyond ? t.ledBeyond : t.ledIdle;
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
