#include "ParamRow.h"
#include "Popovers.h"

namespace vima
{
KnobTile::KnobTile (juce::String t, juce::String u) : title (std::move (t)), unit (std::move (u))
{
    knob.thickness = 3.0f;
    knob.fontSize = 10.0f;
    addAndMakeVisible (knob);
}

void KnobTile::resized()
{
    knob.setBounds (4, (getHeight() - 40) / 2, 40, 40);
}

void KnobTile::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    draw::tile (g, getLocalBounds().toFloat(), t.tile, t.border);
    auto text = getLocalBounds().withTrimmedLeft (52);
    if (unit.isEmpty())
    {
        draw::label (g, title, text, t.muted);
        return;
    }
    draw::label (g, title, text.withHeight (getHeight() / 2 + 2).withTrimmedTop (getHeight() / 2 - 12), t.muted,
                 10.0f, juce::Justification::bottomLeft);
    g.setColour (t.text2);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (unit, text.withTrimmedTop (getHeight() / 2 + 3).withHeight (14), juce::Justification::topLeft);
}

void LabelledTile::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    draw::tile (g, getLocalBounds().toFloat(), t.tile, borderColour ? borderColour() : t.border);
    auto top = getLocalBounds().reduced (8, 0).withTrimmedTop (6).withHeight (13);
    draw::label (g, title, top, t.muted);
    if (rightText)
    {
        g.setColour (t.text2);
        g.setFont (Fonts::mono (10.0f));
        g.drawText (rightText(), top, juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------------------------------
namespace
{
    struct ArrowPill : public Pill
    {
        int kind = 0;   // 0 forward, 1 backward, 2 ping-pong
        void paint (juce::Graphics& g) override
        {
            Pill::paint (g);
            const auto c = getLocalBounds().toFloat().getCentre();
            juce::Path p;
            if (kind == 0)
            {
                p.startNewSubPath (c.x - 7, c.y); p.lineTo (c.x + 6, c.y);
                p.startNewSubPath (c.x + 2, c.y - 3.5f); p.lineTo (c.x + 6, c.y); p.lineTo (c.x + 2, c.y + 3.5f);
            }
            else if (kind == 1)
            {
                p.startNewSubPath (c.x + 7, c.y); p.lineTo (c.x - 6, c.y);
                p.startNewSubPath (c.x - 2, c.y - 3.5f); p.lineTo (c.x - 6, c.y); p.lineTo (c.x - 2, c.y + 3.5f);
            }
            else
            {
                p.startNewSubPath (c.x - 6, c.y); p.lineTo (c.x + 6, c.y);
                p.startNewSubPath (c.x - 3, c.y - 3.5f); p.lineTo (c.x - 6.5f, c.y); p.lineTo (c.x - 3, c.y + 3.5f);
                p.startNewSubPath (c.x + 3, c.y - 3.5f); p.lineTo (c.x + 6.5f, c.y); p.lineTo (c.x + 3, c.y + 3.5f);
            }
            g.setColour (look ? look().text : Theme::current().text2);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    Pill::Look segmentLook (bool on, juce::Colour accent)
    {
        const auto& t = Theme::current();
        return on ? Pill::Look { accent, accent, accent == t.neutral ? t.onNeutral : t.onAccent, 0.0f, false }
                  : Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.text2, 0.0f, false };
    }
}

ParamRow::ParamRow (Context& c) : ctx (c)
{
    auto neutral = [] { return Theme::current().neutral; };

    // LENGTH
    auto& lk = lengthTile.knob;
    lk.minValue = minLength; lk.maxValue = maxSteps; lk.defaultValue = 16;
    lk.colour = neutral;
    lk.getValue = [this] { return (int) ctx.trackCopy().length; };
    lk.setValue = [this] (int v) { ctx.edit (false, [this, v] (Project& p) { ctx.liveTrack (p).length = (std::uint8_t) v; }); };
    lk.onGestureStart = [this] { ctx.beginGesture(); };
    addAndMakeVisible (lengthTile);

    // SWING
    auto& sk = swingTile.knob;
    sk.minValue = 0; sk.maxValue = 100; sk.defaultValue = 0;
    sk.colour = neutral;
    sk.format = [] (int v) { return juce::String (v) + "%"; };
    sk.parse = [] (const juce::String& s) { return s.retainCharacters ("0123456789").getIntValue(); };
    sk.getValue = [this] { return (int) ctx.trackCopy().swing; };
    sk.setValue = [this] (int v) { ctx.edit (false, [this, v] (Project& p) { ctx.liveTrack (p).swing = (std::uint8_t) v; }); };
    sk.onGestureStart = [this] { ctx.beginGesture(); };
    addAndMakeVisible (swingTile);

    // MIDI CH (routing: global per track, not undoable)
    auto& ck = channelTile.knob;
    ck.minValue = 1; ck.maxValue = 16;
    ck.colour = neutral;
    ck.getValue = [this] { return (int) ctx.routingCopy().midiChannel; };
    ck.setValue = [this] (int v)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, v] (Project& p) { p.routing[(size_t) t].midiChannel = (std::uint8_t) v; });
    };
    ck.onGestureStart = [this] { channelTile.knob.defaultValue = ctx.ui.track + 1; };
    addAndMakeVisible (channelTile);

    // DIVISION
    divisionTile.borderColour = [this] { return Popover::isOpen (*ctx.root, "division") ? Theme::current().neutral : Theme::current().border; };
    divisionBtn.look = [] { return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, Theme::current().text, 0.0f, false }; };
    divisionBtn.text = [this] { return juce::String (divisionInfo (ctx.trackCopy().division).name); };
    divisionBtn.font = [] { return Fonts::monoSemi (14.0f); };
    divisionBtn.justification = juce::Justification::centredLeft;
    divisionBtn.textInset = 8;
    divisionBtn.onClick = [this] { openDivision(); };
    divisionBtn.onWheel = [this] (int d)
    {
        ctx.edit (true, [this, d] (Project& p)
        {
            auto& tp = ctx.liveTrack (p);
            tp.division = (std::uint8_t) juce::jlimit (0, numDivisions - 1, tp.division - d);   // wheel up = shorter steps
        });
    };
    divisionTile.addAndMakeVisible (divisionBtn);
    addAndMakeVisible (divisionTile);

    // DIRECTION
    for (int i = 0; i < 3; ++i)
    {
        auto* p = new ArrowPill();
        p->kind = i;
        p->radius = 4.0f;
        p->look = [this, i] { return segmentLook ((int) ctx.trackCopy().direction == i, Theme::current().neutral); };
        p->onClick = [this, i] { ctx.edit (true, [this, i] (Project& pr) { ctx.liveTrack (pr).direction = (Direction) i; }); };
        p->setTitle (i == 0 ? "Forward" : i == 1 ? "Backward" : "Ping-pong");
        directionBtns.add (p);
        directionTile.addAndMakeVisible (p);
    }
    addAndMakeVisible (directionTile);

    // SCALE (note mode)
    scaleNameBtn.look = [] { const auto& t = Theme::current(); return Pill::Look { t.panel, t.border2, t.text, 1.0f, false }; };
    scaleNameBtn.radius = 4.0f;
    scaleNameBtn.justification = juce::Justification::centredLeft;
    scaleNameBtn.textInset = 6;
    scaleNameBtn.font = [] { return Fonts::monoSemi (10.0f); };
    scaleNameBtn.text = [this]
    {
        const auto g = ctx.globalsCopy();
        return juce::String (pitchClassName (g.root)) + " " + scaleInfo (g.scale).shortName;
    };
    scaleNameBtn.onClick = [this] { openScale(); };
    scaleNameBtn.onWheel = [this] (int d)
    {
        ctx.edit (false, [d] (Project& p) { p.globals.scale = juce::jlimit (0, numScales - 1, p.globals.scale + d); });
    };
    scaleTile.addAndMakeVisible (scaleNameBtn);
    scaleOnBtn.radius = 4.0f;
    scaleOnBtn.font = [] { return Fonts::monoSemi (10.0f); };
    scaleOnBtn.look = [this] { return Pill::toggle (ctx.trackCopy().scaleOn, Theme::current().green); };
    scaleOnBtn.text = [this] { return juce::String (ctx.trackCopy().scaleOn ? "ON" : "OFF"); };
    scaleOnBtn.onClick = [this] { ctx.edit (true, [this] (Project& p) { auto& tp = ctx.liveTrack (p); tp.scaleOn = ! tp.scaleOn; }); };
    scaleTile.addAndMakeVisible (scaleOnBtn);
    scaleTile.borderColour = [this] { return Popover::isOpen (*ctx.root, "scale") ? Theme::current().green : Theme::current().border; };
    addAndMakeVisible (scaleTile);

    // NOTE OUT (note mode)
    noteOutBtn.radius = 4.0f;
    noteOutBtn.font = [] { return Fonts::monoSemi (10.0f); };
    noteOutBtn.look = [this] { return Pill::toggle (ctx.routingCopy().noteOut, Theme::current().green); };
    noteOutBtn.text = [this] { return juce::String (ctx.routingCopy().noteOut ? "ON" : "OFF"); };
    noteOutBtn.onClick = [this]
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t] (Project& p) { p.routing[(size_t) t].noteOut = ! p.routing[(size_t) t].noteOut; });
    };
    noteOutTile.addAndMakeVisible (noteOutBtn);
    addAndMakeVisible (noteOutTile);

    // CC OUT + CC number (CC mode)
    ccOutBtn.radius = 4.0f;
    ccOutBtn.font = [] { return Fonts::monoSemi (10.0f); };
    ccOutBtn.look = [this] { return Pill::toggle (ctx.routingCopy().ccOut, Theme::current().cyan); };
    ccOutBtn.text = [this] { return juce::String (ctx.routingCopy().ccOut ? "ON" : "OFF"); };
    ccOutBtn.onClick = [this]
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t] (Project& p) { p.routing[(size_t) t].ccOut = ! p.routing[(size_t) t].ccOut; });
    };
    ccOutTile.addAndMakeVisible (ccOutBtn);
    ccNumberKnob.minValue = 0; ccNumberKnob.maxValue = 127; ccNumberKnob.defaultValue = 102;
    ccNumberKnob.fontSize = 10.0f;
    ccNumberKnob.colour = [] { return Theme::current().cyan; };
    ccNumberKnob.getValue = [this] { return (int) ctx.routingCopy().ccNumber; };
    ccNumberKnob.setValue = [this] (int v)
    {
        const int t = ctx.ui.track;
        ctx.edit (false, [t, v] (Project& p) { p.routing[(size_t) t].ccNumber = (std::uint8_t) v; });
    };
    ccNumberKnob.onGestureStart = [this] { ccNumberKnob.defaultValue = 102 + ctx.ui.track; };   // init: 102-109
    ccOutTile.addAndMakeVisible (ccNumberKnob);
    addAndMakeVisible (ccOutTile);

    // SLIDER lane switch (note mode)
    const std::array<SliderLane, 3> lanes { SliderLane::velocity, SliderLane::gate, SliderLane::probability };
    const char* names[] = { "VEL", "GATE", "PROB" };
    for (int i = 0; i < 3; ++i)
    {
        auto* p = new Pill();
        const auto lane = lanes[(size_t) i];
        p->radius = 4.0f;
        p->font = [] { return Fonts::monoSemi (10.0f); };
        p->text = [n = juce::String (names[i])] { return n; };
        p->look = [this, lane, i]
        {
            const auto& t = Theme::current();
            const juce::Colour accents[] = { t.green, t.purple, t.amber };
            return segmentLook (ctx.ui.slider == lane, accents[i]);
        };
        p->onClick = [this, lane] { ctx.ui.slider = lane; ctx.changed(); };
        sliderBtns.add (p);
        sliderTile.addAndMakeVisible (p);
    }
    addAndMakeVisible (sliderTile);

    // LEARN targets (SPEC.md 13.1)
    setLearnTarget (lengthTile, MacroTarget::length);
    setLearnTarget (swingTile, MacroTarget::swing);
    setLearnTarget (divisionTile, MacroTarget::division);
    setLearnTarget (directionTile, MacroTarget::direction);
    setLearnTarget (scaleTile, MacroTarget::scaleOn);
    setLearnTarget (noteOutTile, MacroTarget::noteOut);
    setLearnTarget (ccOutTile, MacroTarget::ccOut);

    updateVisibility();
}

void ParamRow::updateVisibility()
{
    const bool cc = ctx.ui.ccMode;
    scaleTile.setVisible (! cc);
    noteOutTile.setVisible (! cc);
    sliderTile.setVisible (! cc);
    ccOutTile.setVisible (cc);
}

void ParamRow::resized()
{
    updateVisibility();
    auto r = getLocalBounds();
    auto place = [&r] (juce::Component& c, int w)
    {
        c.setBounds (r.removeFromLeft (w));
        r.removeFromLeft (8);
    };
    place (lengthTile, 106);
    place (divisionTile, 112);
    place (directionTile, 136);
    place (swingTile, 106);
    place (channelTile, 106);
    if (ctx.ui.ccMode)
    {
        place (ccOutTile, 150);
    }
    else
    {
        place (scaleTile, 128);
        place (noteOutTile, 88);
        sliderTile.setBounds (getLocalBounds().removeFromRight (150));
    }

    divisionBtn.setBounds (divisionTile.getLocalBounds().withTrimmedTop (18).withTrimmedBottom (4));
    {
        auto b = directionTile.getLocalBounds().reduced (8, 0).withTrimmedTop (22).withHeight (20);
        const int w = (b.getWidth() - 6) / 3;
        for (int i = 0; i < 3; ++i) directionBtns[i]->setBounds (b.getX() + i * (w + 3), b.getY(), w, 20);
    }
    {
        auto b = scaleTile.getLocalBounds().reduced (6, 0).withTrimmedLeft (2).withTrimmedTop (22).withHeight (20);
        scaleOnBtn.setBounds (b.removeFromRight (34));
        b.removeFromRight (4);
        scaleNameBtn.setBounds (b);
    }
    noteOutBtn.setBounds (noteOutTile.getLocalBounds().reduced (8, 0).withTrimmedTop (22).withHeight (20));
    {
        auto b = ccOutTile.getLocalBounds().reduced (6, 0).withTrimmedLeft (2);
        ccNumberKnob.setBounds (b.removeFromRight (40).withSizeKeepingCentre (40, 40));
        b.removeFromRight (8);
        ccOutBtn.setBounds (b.withTrimmedTop (22).withHeight (20));
    }
    {
        auto b = sliderTile.getLocalBounds().reduced (8, 0).withTrimmedTop (22).withHeight (20);
        const int w = (b.getWidth() - 6) / 3;
        for (int i = 0; i < 3; ++i) sliderBtns[i]->setBounds (b.getX() + i * (w + 3), b.getY(), w, 20);
    }
}

void ParamRow::openDivision()
{
    if (Popover::isOpen (*ctx.root, "division")) { Popover::closeAll (*ctx.root); return; }
    const auto sz = DivisionPopover::size();
    auto b = ctx.root->getLocalArea (this, divisionTile.getBounds());
    Popover::show (*ctx.root, std::make_unique<DivisionPopover> (ctx), { b.getX(), b.getBottom() + 6, sz.x, sz.y },
                   [this] { repaint(); divisionTile.repaint(); });
    divisionTile.repaint();
}

void ParamRow::openScale()
{
    if (Popover::isOpen (*ctx.root, "scale")) { Popover::closeAll (*ctx.root); return; }
    const auto sz = ScalePopover::size();
    auto b = ctx.root->getLocalArea (this, scaleTile.getBounds());
    Popover::show (*ctx.root, std::make_unique<ScalePopover> (ctx), { b.getX(), b.getBottom() + 6, sz.x, sz.y },
                   [this] { scaleTile.repaint(); });
    scaleTile.repaint();
}
} // namespace vima
