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
        const bool on = stepCopy().on;
        const bool isPlay = ctx.proc.engine.isPlaying && ctx.proc.engine.playStep[(size_t) ctx.ui.track].load() == index;
        const auto accent = ctx.modeAccent();
        if (on) return Pill::Look { accent, accent, t.onAccent, 1.5f, false };
        return Pill::Look { juce::Colours::transparentBlack, isPlay ? accent : t.dim, isPlay ? accent : t.muted, 1.5f, false };
    };
    toggle.onClick = [this]
    {
        ctx.edit (true, [this] (Project& p) { auto& s = ctx.liveTrack (p).steps[(size_t) index]; s.on = ! s.on; });
    };
    addAndMakeVisible (toggle);

    auto gestureStart = [this] { ctx.beginGesture(); };

    // Pitch: moves through in-scale notes when the scale is on; shows the note that will sound
    pitch.thickness = 4.0f;
    pitch.fontSize = 12.0f;
    pitch.colour = [this] { return ctx.modeAccent(); };
    pitch.getValue = [this] { return ((soundingPitch (ctx, index) % 12) + 12) % 12; };
    pitch.fraction = [this] { return (float) (((soundingPitch (ctx, index) % 12) + 12) % 12) / 11.0f; };
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
    pitch.minValue = 0; pitch.maxValue = 11; pitch.defaultValue = 0;
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
        return (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2 + (g.middleCIsC4 ? 1 : 0);
    };
    octave.fraction = [this]
    {
        const auto g = ctx.globalsCopy();
        const int o = (int) std::floor (soundingPitch (ctx, index) / 12.0) - 2;
        return g.octaveHigh > g.octaveLow ? (float) (o - g.octaveLow) / (float) (g.octaveHigh - g.octaveLow) : 1.0f;
    };
    octave.nudge = [this] (int d)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, d, this] (Project& p) { nudgeOctave (p, p.currentPattern, t, index, d); });
    };
    octave.minValue = minOctave; octave.maxValue = maxOctave + 1; octave.defaultValue = 3;
    octave.setValue = [this] (int shown)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, shown, this] (Project& p)
        {
            const int o = shown - (p.globals.middleCIsC4 ? 1 : 0);
            p.patterns[(size_t) p.currentPattern].tracks[(size_t) t].steps[(size_t) index].octave
                = (std::int8_t) juce::jlimit (p.globals.octaveLow, p.globals.octaveHigh, o);
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
            else if (lane == SliderLane::gate) s.gate = (std::uint8_t) juce::jlimit (1, 100, v);
            else s.probability = (std::uint8_t) juce::jlimit (0, 100, v);
        });
    };
    slider.barColour = [this]
    {
        const auto& t = Theme::current();
        return ctx.ui.slider == SliderLane::velocity ? t.green : ctx.ui.slider == SliderLane::gate ? t.purple : t.amber;
    };
    slider.onGestureStart = gestureStart;
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

    refreshState();
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
    slider.maxValue = lane == SliderLane::velocity ? 127 : 100;
    slider.defaultValue = lane == SliderLane::velocity ? 100 : lane == SliderLane::gate ? 50 : 100;
    octave.textColour = Theme::current().text2;

    const auto tp = ctx.trackCopy();
    const bool beyond = index >= tp.length;
    const bool on = tp.steps[(size_t) index].on && ! beyond;
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

    if (ctx.ui.pasteArmed)
    {
        const int t = ctx.ui.track;
        const auto clip = ctx.ui.clip;
        int last = -1;
        ctx.edit (true, [&] (Project& p) { last = pasteSteps (p, p.currentPattern, t, i, clip); });
        ctx.ui.pasteArmed = false;
        ctx.ui.pasteHover = -1;
        ctx.ui.selFirst = i;
        ctx.ui.selLast = juce::jmax (i, last);
        ctx.changed();
        return;
    }
    dragFrom = i;
    ctx.ui.selFirst = ctx.ui.selLast = i;
    ctx.changed();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    const int len = ctx.trackCopy().length;
    const int i = juce::jlimit (0, len - 1, ledIndexAt (e.getPosition(), true));
    ctx.ui.selFirst = juce::jmin (dragFrom, i);
    ctx.ui.selLast = juce::jmax (dragFrom, i);
    repaintLeds();
    if (ctx.refresh) ctx.refresh();
}

void StepGrid::mouseUp (const juce::MouseEvent&) { dragFrom = -1; }

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const int i = ledIndexAt (e.getPosition(), false);
    setMouseCursor (i >= 0 ? (ctx.ui.pasteArmed ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::PointingHandCursor)
                           : juce::MouseCursor::NormalCursor);
    if (ctx.ui.pasteArmed && i != ctx.ui.pasteHover)
    {
        ctx.ui.pasteHover = i;
        repaintLeds();
    }
}

void StepGrid::mouseExit (const juce::MouseEvent&)
{
    if (ctx.ui.pasteHover >= 0)
    {
        ctx.ui.pasteHover = -1;
        repaintLeds();
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const int len = ctx.trackCopy().length;
    const auto accent = ctx.modeAccent();
    const bool playing = ctx.proc.engine.isPlaying;
    const int play = playing ? ctx.proc.engine.playStep[(size_t) ctx.ui.track].load() : -1;
    const int clipN = (int) ctx.ui.clip.size();
    const bool hasSel = ctx.ui.selFirst >= 0 && ctx.ui.selFirst < len;

    for (int i = 0; i < maxSteps; ++i)
    {
        auto b = ledBounds (i).toFloat();
        auto bar = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
        const bool beyond = i >= len;
        const bool ghost = ctx.ui.pasteArmed && ctx.ui.pasteHover >= 0 && i >= ctx.ui.pasteHover && i < ctx.ui.pasteHover + clipN && ! beyond;
        const bool sel = hasSel && i >= ctx.ui.selFirst && i <= ctx.ui.selLast && ! beyond;
        juce::Colour c = beyond ? t.beyond : t.border3;
        if (sel) c = t.selection;
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
