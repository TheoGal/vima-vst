#include "Header.h"
#include "BinaryData.h"
#include "Popovers.h"

namespace vima
{
namespace
{
    // Small inline icons drawn as paths
    void drawPlay (juce::Graphics& g, juce::Point<float> c, juce::Colour col)
    {
        juce::Path p;
        p.addTriangle (c.x - 3.5f, c.y - 4.5f, c.x - 3.5f, c.y + 4.5f, c.x + 4.5f, c.y);
        g.setColour (col);
        g.fillPath (p);
    }
    void drawStop (juce::Graphics& g, juce::Point<float> c, juce::Colour col)
    {
        g.setColour (col);
        g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c), 1.0f);
    }

}

static void undoIcon (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
{
    const float x = b.getX() + 12.0f, y = b.getCentreY() - 7.0f;
    juce::Path p;
    p.startNewSubPath (x + 3.0f, y + 2.5f);
    p.lineTo (x, y + 5.5f);
    p.lineTo (x + 3.0f, y + 8.5f);
    p.startNewSubPath (x, y + 5.5f);
    p.lineTo (x + 7.0f, y + 5.5f);
    p.addCentredArc (x + 7.0f, y + 9.0f, 3.5f, 3.5f, 0.0f, 0.0f, juce::MathConstants<float>::pi, false);
    p.lineTo (x + 4.5f, y + 12.5f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void settingsIcon (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
{
    const float x = b.getX() + 12.0f, y = b.getCentreY() - 7.0f;
    g.setColour (c);
    for (int i = 0; i < 3; ++i)
        g.drawLine (x, y + 3.0f + i * 4.0f, x + 12.0f, y + 3.0f + i * 4.0f, 1.3f);
    const float knobX[] = { 8.0f, 3.5f, 9.0f };
    for (int i = 0; i < 3; ++i)
    {
        auto r = juce::Rectangle<float> (3.6f, 3.6f).withCentre ({ x + knobX[i], y + 3.0f + i * 4.0f });
        g.setColour (Theme::current().panel);
        g.fillEllipse (r);
        g.setColour (c);
        g.drawEllipse (r, 1.3f);
    }
}

static void fileIcon (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
{
    // a folder outline
    const auto ctr = b.getCentre();
    juce::Path p;
    const float w = 15.0f, h = 11.0f, x = ctr.x - w / 2, y = ctr.y - h / 2 + 0.5f;
    p.startNewSubPath (x, y + h);
    p.lineTo (x, y);
    p.lineTo (x + 5.0f, y);
    p.lineTo (x + 6.5f, y + 2.0f);
    p.lineTo (x + w, y + 2.0f);
    p.lineTo (x + w, y + h);
    p.closeSubPath();
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void chevron (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, bool left)
{
    const auto ctr = b.getCentre();
    juce::Path p;
    const float d = left ? 1.0f : -1.0f;
    p.startNewSubPath (ctr.x + 1.5f * d, ctr.y - 3.5f);
    p.lineTo (ctr.x - 2.0f * d, ctr.y);
    p.lineTo (ctr.x + 1.5f * d, ctr.y + 3.5f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

Header::Header (Context& c) : ctx (c)
{
    const auto plainOutline = [] { const auto& t = Theme::current(); return Pill::outline (t.border, t.text2); };

    // TRACK dropdown
    trackBox.look = [] { return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack, 0.0f, false }; };
    trackBox.radius = 6.0f;
    trackBox.onClick = [this] { showTrackMenu(); };
    addAndMakeVisible (trackBox);

    // NOTES / CC switch with output LEDs
    auto modeLook = [this] (bool cc)
    {
        const auto& t = Theme::current();
        const bool sel = ctx.ui.ccMode == cc;
        const auto accent = cc ? t.cyan : t.green;
        return sel ? Pill::Look { accent.withAlpha (0.16f), accent, accent, 1.0f, false }
                   : Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.text2, 0.0f, false };
    };
    notesBtn.look = [modeLook] { return modeLook (false); };
    ccBtn.look = [modeLook] { return modeLook (true); };
    notesBtn.text = [] { return juce::String ("NOTES"); };
    ccBtn.text = [] { return juce::String ("CC"); };
    notesBtn.font = ccBtn.font = [] { return Fonts::monoSemi (12.0f).withExtraKerningFactor (0.08f); };
    notesBtn.ledColour = [this] { return ctx.routingCopy().noteOut ? Theme::current().green : Theme::current().ledOff; };
    ccBtn.ledColour = [this] { return ctx.routingCopy().ccOut ? Theme::current().cyan : Theme::current().ledOff; };
    notesBtn.ledGlow = ccBtn.ledGlow = [] { return false; };   // plain dot, no glow ring
    notesBtn.justification = ccBtn.justification = juce::Justification::centredLeft;
    notesBtn.textInset = ccBtn.textInset = 2;
    notesBtn.onClick = [this] { ctx.ui.ccMode = false; Popover::closeAll (*ctx.root); ctx.changed(); };
    ccBtn.onClick = [this] { ctx.ui.ccMode = true; Popover::closeAll (*ctx.root); ctx.changed(); };
    addAndMakeVisible (notesBtn);
    addAndMakeVisible (ccBtn);

    // Back to the step view (patterns view only)
    // STEPS (back to the step view) stands out: filled with the note green
    stepsBtn.look = [] { const auto& t = Theme::current(); return Pill::filled (t.green, t.onAccent); };
    stepsBtn.text = [] { return juce::String ("STEPS"); };
    stepsBtn.justification = juce::Justification::centredLeft;
    stepsBtn.textInset = 28;
    stepsBtn.font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.12f); };
    stepsBtn.radius = 6.0f;
    stepsBtn.onClick = [this]
    {
        if (ctx.ui.macrosView && ctx.showMacros) ctx.showMacros (false);
        else if (ctx.showPatterns) ctx.showPatterns (false);
    };
    addChildComponent (stepsBtn);

    // Pattern quick picker
    auto arrowLook = [] { const auto& t = Theme::current(); return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.text2, 0.0f, false }; };
    prevBtn.look = nextBtn.look = arrowLook;
    prevBtn.radius = nextBtn.radius = 4.0f;
    prevBtn.onClick = [this]
    {
        int i = ctx.currentPattern() - 1;
        if (i < 0) i = numPatterns - 1;
        ctx.proc.queuePattern (i);
        ctx.changed();
    };
    nextBtn.onClick = [this]
    {
        int i = ctx.currentPattern() + 1;
        if (i >= numPatterns) i = 0;
        ctx.proc.queuePattern (i);
        ctx.changed();
    };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    patternBtn.look = arrowLook;
    patternBtn.onClick = [this]
    {
        if (Popover::isOpen (*ctx.root, "patternPicker")) { Popover::closeAll (*ctx.root); return; }
        ctx.ui.pickerPage = ctx.currentPattern() / patternsPerPage;
        const auto sz = PatternPickerPopover::size();
        auto area = patternArea.translated (getX(), getY());
        Popover::show (*ctx.root, std::make_unique<PatternPickerPopover> (ctx),
                       { area.getRight() - sz.x, area.getBottom() + 8, sz.x, sz.y }, [this] { repaint(); });
        repaint();
    };
    addAndMakeVisible (patternBtn);

    // Transport: play / stop button inside the HOST box
    transportBtn.look = [] { return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack, 0.0f, false }; };
    transportBtn.radius = 4.0f;
    transportBtn.setTitle ("Start / stop");
    transportBtn.onClick = [this] { ctx.proc.toggleTransport(); repaint(); };
    addAndMakeVisible (transportBtn);

    // FILE menu: presets (SPEC.md 13.3)
    fileBtn.look = plainOutline;
    fileBtn.radius = 6.0f;
    fileBtn.setTitle ("Presets");
    fileBtn.onClick = [this] { Popover::closeAll (*ctx.root); showFileMenu(); };
    addAndMakeVisible (fileBtn);

    // UNDO and SETTINGS
    undoBtn.look = [this]
    {
        const auto& t = Theme::current();
        auto l = Pill::outline (t.border, t.text2);
        if (! ctx.proc.canUndo()) l.text = t.dim;
        return l;
    };
    undoBtn.text = [] { return juce::String ("UNDO"); };
    undoBtn.justification = juce::Justification::centredLeft;
    undoBtn.textInset = 32;
    undoBtn.font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.12f); };
    undoBtn.radius = 6.0f;
    undoBtn.onClick = [this] { if (ctx.proc.performUndo()) ctx.changed(); };
    addAndMakeVisible (undoBtn);

    // MACROS page; shows LEARN and the macro being learned while LEARN is on
    macrosBtn.look = [this]
    {
        const auto& t = Theme::current();
        if (ctx.ui.macroLearn) return Pill::Look { t.amber.withAlpha (0.14f), t.amber, t.amber, 1.5f, false };
        return Pill::outline (ctx.ui.macrosView ? t.neutral : t.border, t.text2);
    };
    macrosBtn.text = [this] { return ctx.ui.macroLearn ? "LEARN " + macroId (ctx.ui.selectedMacro) : juce::String ("MACROS"); };
    macrosBtn.font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.12f); };
    macrosBtn.radius = 6.0f;
    macrosBtn.onClick = [this] { Popover::closeAll (*ctx.root); if (ctx.showMacros) ctx.showMacros (true); };
    addAndMakeVisible (macrosBtn);

    settingsBtn.look = plainOutline;
    settingsBtn.text = [] { return juce::String ("SETTINGS"); };
    settingsBtn.justification = juce::Justification::centredLeft;
    settingsBtn.textInset = 34;
    settingsBtn.font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.12f); };
    settingsBtn.radius = 6.0f;
    settingsBtn.onClick = [this] { Popover::closeAll (*ctx.root); if (ctx.openSettings) ctx.openSettings(); };
    addAndMakeVisible (settingsBtn);
}

void Header::resized()
{
    const bool pv = ctx.ui.patternsView || ctx.ui.macrosView;
    auto r = getLocalBounds().withTrimmedBottom (1).reduced (20, 0);

    nameArea = r.removeFromLeft (60);
    r.removeFromLeft (14 + 1 + 14);   // separator

    trackBox.setVisible (! pv);
    notesBtn.setVisible (! pv);
    ccBtn.setVisible (! pv);
    stepsBtn.setVisible (pv);
    if (pv)
    {
        stepsBtn.setBounds (r.removeFromLeft (92).withSizeKeepingCentre (92, 32));
        modeArea = {};
    }
    else
    {
        r = r.withLeft (128);   // over the track tiles; the wordmark column holds SCALE below
        trackBox.setBounds (r.removeFromLeft (100).withSizeKeepingCentre (100, 32));
        r.removeFromLeft (14);
        modeArea = r.removeFromLeft (172).withSizeKeepingCentre (172, 34);
        auto m = modeArea.reduced (4);
        notesBtn.setBounds (m.removeFromLeft (92));
        m.removeFromLeft (3);
        ccBtn.setBounds (m);
    }

    auto right = r;
    settingsBtn.setBounds (right.removeFromRight (108).withSizeKeepingCentre (108, 32));
    right.removeFromRight (8);
    macrosBtn.setBounds (right.removeFromRight (104).withSizeKeepingCentre (104, 32));
    right.removeFromRight (8);
    undoBtn.setBounds (right.removeFromRight (84).withSizeKeepingCentre (84, 32));
    right.removeFromRight (8);
    fileBtn.setBounds (right.removeFromRight (40).withSizeKeepingCentre (40, 32));
    right.removeFromRight (8);
    hostArea = right.removeFromRight (116).withSizeKeepingCentre (116, 32);
    transportBtn.setBounds (hostArea.withWidth (30).reduced (4, 4).translated (1, 0));
    right.removeFromRight (10);
    patternArea = right.removeFromRight (162).withSizeKeepingCentre (162, 32);   // sized for two-digit numbers
    if (! pv)
        patternArea.setX (496);   // over the pattern tiles
    auto pa = patternArea.reduced (3);
    prevBtn.setBounds (pa.removeFromLeft (24));
    nextBtn.setBounds (pa.removeFromRight (24));
    patternBtn.setBounds (pa);
}

void Header::showTrackMenu()
{
    juce::PopupMenu m;
    for (int t = 0; t < numTracks; ++t)
        m.addItem (t + 1, "TRACK " + juce::String (t + 1).paddedLeft ('0', 2), true, t == ctx.ui.track);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&trackBox).withMinimumWidth (trackBox.getWidth()),
                     [this] (int result)
                     {
                         if (result <= 0) return;
                         ctx.ui.track = result - 1;
                         ctx.ui.selection = 0;   // switching tracks clears the selection
                         ctx.changed();
                     });
}

void Header::showFileMenu()
{
    // PATTERN acts on the selected pattern: the patterns view's selected slot, else the one shown
    const int pattern = ctx.ui.patternsView ? ctx.ui.selectedPattern : ctx.currentPattern();
    juce::PopupMenu m;
    fileActions.addMenu (m, pattern);
    juce::Component::SafePointer<Header> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn).withMinimumWidth (200),
                     [safe, pattern] (int result)
                     {
                         if (safe != nullptr && result > 0)
                             safe->fileActions.perform (result, pattern);
                     });
}

void Header::paintOverChildren (juce::Graphics& g)
{
    // the STEPS chevron sits on the button's green fill
    if (stepsBtn.isVisible())
        chevron (g, stepsBtn.getBounds().toFloat().withWidth (28.0f), Theme::current().onAccent, true);
}

void Header::mouseDown (const juce::MouseEvent&) {}

void Header::drawWordmark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = Theme::current();
    static std::unique_ptr<juce::Drawable> logo;
    static juce::Colour logoColour;
    if (logo == nullptr || logoColour != t.text)
    {
        const auto svgText = juce::String::fromUTF8 (BinaryData::vimawordmarkui_svg, BinaryData::vimawordmarkui_svgSize)
                                 .replace ("#EEF1F5", "#" + t.text.toDisplayString (false));
        if (auto xml = juce::XmlDocument::parse (svgText))
            logo = juce::Drawable::createFromSVG (*xml);
        logoColour = t.text;
    }
    if (logo != nullptr)
        logo->drawWithin (g, area, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid, 1.0f);
}

void Header::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const bool stepView = ! ctx.ui.patternsView && ! ctx.ui.macrosView;
    if (! stepView)   // the step view's top band is drawn by the editor, with the group frames
    {
        g.fillAll (t.panel);
        g.setColour (t.border);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    // The header shows the Greek wordmark "βήμα" (SPEC.md 12.1); in the step view the editor draws
    // it larger, beside the TRACK frame
    if (! stepView)
        drawWordmark (g, nameArea.toFloat().withSizeKeepingCentre ((float) nameArea.getWidth(), 25.0f));
    if (! stepView)
    {
        g.setColour (t.border);
        g.fillRect (nameArea.getRight() + 14, getHeight() / 2 - 11, 1, 22);
    }

    if (! ctx.ui.patternsView && ! ctx.ui.macrosView)
    {
        // TRACK dropdown content
        auto tb = trackBox.getBounds();
        draw::tile (g, tb.toFloat(), t.tile, t.border, 6.0f);
        const int lw = juce::GlyphArrangement::getStringWidthInt (Fonts::label (10.0f), "TRACK") + 4;
        draw::label (g, "TRACK", tb.withTrimmedLeft (12).withWidth (lw), t.muted);
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (14.0f));
        g.drawText (juce::String (ctx.ui.track + 1).paddedLeft ('0', 2), tb.withTrimmedLeft (12 + lw + 6).withWidth (24), juce::Justification::centredLeft);
        draw::chevronDown (g, { (float) tb.getRight() - 14.0f, (float) tb.getCentreY() }, t.text2, 10.0f);

        // mode switch frame
        draw::tile (g, modeArea.toFloat(), t.tile, t.border, 7.0f);
    }
    else
    {
        if (ctx.ui.macrosView)
        {
            g.setColour (t.text);
            g.setFont (Fonts::sansSemi (13.0f).withExtraKerningFactor (0.14f));
            g.drawText ("MACROS", stepsBtn.getBounds().withX (stepsBtn.getRight() + 14).withWidth (120), juce::Justification::centredLeft);
        }
    }

    // pattern box
    const bool pickerOpen = Popover::isOpen (*ctx.root, "patternPicker");
    draw::tile (g, patternArea.toFloat(), t.tile, (pickerOpen || ctx.ui.patternsView) ? t.neutral : t.border, 6.0f);
    chevron (g, prevBtn.getBounds().toFloat(), t.text2, true);
    chevron (g, nextBtn.getBounds().toFloat(), t.text2, false);
    {
        auto pb = patternBtn.getBounds();
        const int lw = juce::GlyphArrangement::getStringWidthInt (Fonts::label (10.0f), "PATTERN") + 4;
        draw::label (g, "PATTERN", pb.withTrimmedLeft (8).withWidth (lw), t.muted);
        const bool queued = ctx.proc.engine.queuedPattern.load() >= 0;
        g.setColour (t.text.withAlpha (queued && ctx.ui.blink ? 0.2f : 1.0f));
        g.setFont (Fonts::monoSemi (13.0f));
        const auto num = patternLabel (ctx.currentPattern());
        const int nx = pb.getX() + 8 + lw + 6;
        const int nw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), num);
        g.drawText (num, pb.withX (nx).withWidth (nw + 2), juce::Justification::centredLeft);
        // chevron after the number: the number opens the pattern picker
        draw::chevronDown (g, { (float) (nx + nw + 11), (float) pb.getCentreY() }, t.text2, 10.0f);
    }

    // host status (display only)
    {
        draw::tile (g, hostArea.toFloat(), juce::Colours::transparentBlack, t.border, 6.0f);
        draw::tile (g, transportBtn.getBounds().toFloat(), t.tile, t.border2, 4.0f);
        const auto c = transportBtn.getBounds().toFloat().getCentre();
        if (ctx.proc.engine.isPlaying) drawPlay (g, c, ctx.modeAccent());
        else drawStop (g, c, ctx.proc.userStopped.load() ? t.amber : t.muted);   // amber: held, does not follow the host
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (12.0f));
        g.drawText (juce::String (ctx.proc.hostBpm.load(), 2) + " BPM", hostArea.withTrimmedLeft (36), juce::Justification::centredLeft);
    }

    undoIcon (g, undoBtn.getBounds().toFloat(), ctx.proc.canUndo() ? t.text2 : t.dim);
    fileIcon (g, fileBtn.getBounds().toFloat(), t.text2);
    settingsIcon (g, settingsBtn.getBounds().toFloat(), t.text2);
}
} // namespace vima
