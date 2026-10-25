#include "MacrosView.h"

namespace vima
{
namespace
{
    Pill::Look boxLook (bool on, juce::Colour accent)
    {
        const auto& t = Theme::current();
        return on ? Pill::Look { accent, accent, accent == t.neutral ? t.onNeutral : t.onAccent, 1.0f, false }
                  : Pill::Look { juce::Colours::transparentBlack, t.border2, t.text2, 1.0f, false };
    }

    const std::vector<MacroTarget>& targetsOf (MacroKind k)
    {
        static const std::vector<MacroTarget> global { MacroTarget::pattern, MacroTarget::chainOn };
        static const std::vector<MacroTarget> track { MacroTarget::noteOut, MacroTarget::ccOut, MacroTarget::length, MacroTarget::division,
                                                       MacroTarget::direction, MacroTarget::swing, MacroTarget::scaleOn };
        static const std::vector<MacroTarget> step { MacroTarget::stepOn, MacroTarget::note, MacroTarget::octave, MacroTarget::velocity,
                                                      MacroTarget::gate, MacroTarget::probability, MacroTarget::ccStepOn, MacroTarget::cc };
        static const std::vector<MacroTarget> none;
        return k == MacroKind::global ? global : k == MacroKind::track ? track : k == MacroKind::step ? step : none;
    }

    constexpr int maxTargetsPerKind = 8;
}

juce::Colour MacrosView::targetColour (MacroTarget target)
{
    const auto& t = Theme::current();
    switch (target)
    {
        case MacroTarget::none:        return t.dim;
        case MacroTarget::stepOn:
        case MacroTarget::note:
        case MacroTarget::octave:
        case MacroTarget::velocity:    return t.green;
        case MacroTarget::gate:        return t.purple;
        case MacroTarget::probability: return t.amber;
        case MacroTarget::cc:
        case MacroTarget::ccStepOn:    return t.cyan;
        case MacroTarget::pattern:
        case MacroTarget::chainOn:
        case MacroTarget::noteOut:
        case MacroTarget::ccOut:
        case MacroTarget::length:
        case MacroTarget::division:
        case MacroTarget::direction:
        case MacroTarget::swing:
        case MacroTarget::scaleOn:     return t.neutral;
    }
    return t.neutral;
}

// ---------------------------------------------------------------------------------------
MacroCell::MacroCell (Context& c, int s) : ctx (c), slot (s)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

int MacroCell::macroIndex() const { return ctx.ui.macroPage * macrosPerPage + slot; }

void MacroCell::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const int i = macroIndex();
    struct Info { MacroMap map; std::optional<int> value; juce::String text; MacroNotes notes; };
    const auto info = ctx.proc.read ([i] (const Project& p)
    {
        Info r;
        r.map = p.macros[(size_t) i];
        r.value = macroTargetValue (p, r.map);
        r.notes = macroNotes (p, r.map);
        if (r.value) r.text = macroValueText (p, r.map, *r.value);
        return r;
    });
    const bool sel = i == ctx.ui.selectedMacro;
    const bool listening = sel && ctx.ui.macroLearn;
    const auto colour = MacrosView::targetColour (info.map.target);

    auto b = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (sel ? t.selected : t.panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (listening ? t.amber : sel ? t.neutral : t.border);
    g.drawRoundedRectangle (b, 8.0f, (listening || sel) ? 1.5f : 1.0f);

    auto r = getLocalBounds().reduced (8, 6);
    auto top = r.removeFromTop (12);
    g.setColour (sel ? t.text : t.muted);
    g.setFont (Fonts::monoSemi (10.0f));
    g.drawText (macroId (i), top, juce::Justification::centredLeft);
    if (info.map.mapped())
    {
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) top.getRight() - 3.0f, (float) top.getCentreY() }));
    }

    auto lines = r.removeFromBottom (26);
    const auto centre = r.getCentre().toFloat();
    const float radius = juce::jmin (18.0f, (float) r.getHeight() * 0.5f - 2.0f);
    draw::arc (g, centre, radius, 3.0f, 1.0f, t.knobTrack);
    if (info.value)
    {
        draw::arc (g, centre, radius, 3.0f, macroNormalized (info.map, *info.value, info.notes), colour);
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (10.0f));
        g.drawText (info.text, juce::Rectangle<float> (radius * 2.0f, 14.0f).withCentre (centre), juce::Justification::centred);
    }

    g.setColour (info.map.mapped() ? colour : t.dim);
    g.setFont (Fonts::sansSemi (10.0f).withExtraKerningFactor (0.04f));
    g.drawFittedText (macroTargetName (info.map, true), lines.removeFromTop (13), juce::Justification::centred, 1, 0.8f);
    g.setColour (t.muted);
    g.setFont (Fonts::mono (9.0f));
    g.drawText (macroWhereText (info.map, true), lines, juce::Justification::centred, true);
}

void MacroCell::setNormalised (float v)
{
    ctx.proc.macros.setFromUi (macroIndex(), juce::jlimit (0.0f, 1.0f, v));
}

void MacroCell::mouseDown (const juce::MouseEvent& e)
{
    const int i = macroIndex();
    if (ctx.ui.selectedMacro != i)
    {
        ctx.ui.selectedMacro = i;
        ctx.changed();
    }
    dragging = false;
    const auto m = ctx.macroCopy (i);
    if (! m.mapped() || e.mods.isPopupMenu())
        return;
    dragStart = ctx.proc.read ([&m] (const Project& p)
    {
        const auto v = macroTargetValue (p, m);
        return v ? macroNormalized (m, *v, macroNotes (p, m)) : 0.0f;
    });
    dragStartY = e.getScreenY();
    dragging = true;
    ctx.proc.macros.beginGesture (i);
}

void MacroCell::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float px = e.mods.isShiftDown() ? 600.0f : 150.0f;
    setNormalised (dragStart + (float) (dragStartY - e.getScreenY()) / px);
}

void MacroCell::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    ctx.proc.macros.endGesture (macroIndex());
}

void MacroCell::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int i = macroIndex();
    const auto m = ctx.macroCopy (i);
    if (! m.mapped() || juce::exactlyEqual (w.deltaY, 0.0f)) return;
    // one wheel step = the next value the macro can reach (the next in-scale note for notes)
    const auto pos = ctx.proc.read ([&m] (const Project& p) -> std::optional<std::pair<float, int>>
    {
        const auto v = macroTargetValue (p, m);
        if (! v) return std::nullopt;
        const auto n = macroNotes (p, m);
        return std::make_pair (macroNormalized (m, *v, n), macroPositions (m, n));
    });
    if (! pos || pos->second < 2) return;
    ctx.proc.macros.beginGesture (i);
    setNormalised (pos->first + (w.deltaY > 0 ? 1.0f : -1.0f) / (float) (pos->second - 1));
    ctx.proc.macros.endGesture (i);
}

// ---------------------------------------------------------------------------------------
MacrosView::MacrosView (Context& c) : ctx (c)
{
    for (int b = 0; b < numMacros / macrosPerPage; ++b)
    {
        auto* p = new Pill();
        p->radius = 5.0f;
        p->font = [] { return Fonts::monoSemi (11.0f); };
        p->text = [b] { return juce::String (b * macrosPerPage + 1) + " - " + juce::String (b * macrosPerPage + macrosPerPage); };
        p->look = [this, b]   // like the patterns view page buttons: filled when chosen, outlined otherwise
        {
            const auto& t = Theme::current();
            return ctx.ui.macroPage == b ? Pill::filled (t.neutral, t.onNeutral) : Pill::outline (t.border2, t.text2);
        };
        p->onClick = [this, b]
        {
            ctx.ui.macroPage = b;
            ctx.ui.selectedMacro = b * macrosPerPage + ctx.ui.selectedMacro % macrosPerPage;
            ctx.changed();
        };
        pageBtns.add (p);
        addAndMakeVisible (p);
    }

    learnBtn.radius = 6.0f;
    learnBtn.font = [] { return Fonts::monoSemi (11.0f).withExtraKerningFactor (0.08f); };
    learnBtn.text = [] { return juce::String ("LEARN"); };
    learnBtn.look = [this]
    {
        const auto& t = Theme::current();
        return ctx.ui.macroLearn ? Pill::Look { t.amber.withAlpha (0.14f), t.amber, t.amber, 1.5f, false }
                                 : Pill::Look { juce::Colours::transparentBlack, t.dim2, t.text2, 1.5f, false };
    };
    learnBtn.ledColour = [this] { return ctx.ui.macroLearn ? Theme::current().amber : Theme::current().dim2; };
    learnBtn.ledGlow = [this] { return ctx.ui.macroLearn; };
    learnBtn.onClick = [this]
    {
        if (! ctx.ui.macroLearn)
        {
            // listen with the selected macro, or the next free one
            const int i = ctx.macroCopy (ctx.ui.selectedMacro).mapped() ? ctx.nextFreeMacro (ctx.ui.selectedMacro) : ctx.ui.selectedMacro;
            if (i < 0) return;   // all 128 macros are mapped
            ctx.selectMacro (i);
        }
        ctx.ui.macroLearn = ! ctx.ui.macroLearn;
        ctx.changed();
    };
    addAndMakeVisible (learnBtn);

    // SAVE / LOAD all 128 mappings as a set (SPEC.md 13.3)
    for (auto* b : { &saveSetBtn, &loadSetBtn })
    {
        b->radius = 6.0f;
        b->font = [] { return Fonts::monoSemi (11.0f).withExtraKerningFactor (0.08f); };
        b->look = [] { const auto& t = Theme::current(); return Pill::outline (t.border3, t.text2); };
        addAndMakeVisible (*b);
    }
    saveSetBtn.text = [] { return juce::String ("SAVE"); };
    loadSetBtn.text = [] { return juce::String ("LOAD"); };
    saveSetBtn.onClick = [this] { chooseSetFile (true); };
    loadSetBtn.onClick = [this] { chooseSetFile (false); };

    for (int k = 0; k < macrosPerPage; ++k)
    {
        auto* cell = new MacroCell (ctx, k);
        cells.add (cell);
        addAndMakeVisible (cell);
    }

    // TARGET kind
    const std::array<std::pair<MacroKind, const char*>, 3> kinds { { { MacroKind::global, "GLOBAL" }, { MacroKind::track, "TRACK" },
                                                                    { MacroKind::step, "STEP" } } };
    for (const auto& [kind, name] : kinds)
    {
        auto* p = new Pill();
        p->radius = 5.0f;
        p->font = [] { return Fonts::monoSemi (11.0f); };
        p->text = [n = juce::String (name)] { return n; };
        p->look = [this, k = kind] { return boxLook (macroKind (ctx.macroCopy (ctx.ui.selectedMacro).target) == k, Theme::current().neutral); };
        p->onClick = [this, k = kind] { pickKind (k); };
        kindBtns.add (p);
        addAndMakeVisible (p);
    }

    // PARAMETER (rebuilt texts per kind)
    for (int k = 0; k < maxTargetsPerKind; ++k)
    {
        auto* p = new Pill();
        p->radius = 5.0f;
        p->font = [] { return Fonts::monoSemi (10.0f); };
        p->text = [this, k] { return k < (int) shownTargets.size() ? juce::String (macroTargetInfo (shownTargets[(size_t) k]).name) : juce::String(); };
        p->look = [this, k]
        {
            const auto target = k < (int) shownTargets.size() ? shownTargets[(size_t) k] : MacroTarget::none;
            return boxLook (ctx.macroCopy (ctx.ui.selectedMacro).target == target, targetColour (target));
        };
        p->onClick = [this, k] { if (k < (int) shownTargets.size()) pickTarget (shownTargets[(size_t) k]); };
        targetBtns.add (p);
        addChildComponent (p);
    }

    // TRACK: 1..8
    for (int k = 0; k < numTracks; ++k)
    {
        auto* p = new Pill();
        p->radius = 5.0f;
        p->font = [] { return Fonts::monoSemi (10.0f); };
        p->text = [k] { return juce::String (k + 1); };
        p->look = [this, k] { return boxLook (ctx.macroCopy (ctx.ui.selectedMacro).track == k, Theme::current().neutral); };
        p->onClick = [this, k] { editMap ([k] (MacroMap& m, const Globals&) { m.track = (std::int8_t) k; }); };
        trackBtns.add (p);
        addChildComponent (p);
    }

    // PATTERN: a knob (targets stored per pattern)
    patternKnob.minValue = 0; patternKnob.maxValue = numPatterns - 1;
    patternKnob.colour = [] { return Theme::current().neutral; };
    patternKnob.format = [] (int) { return juce::String(); };
    patternKnob.getValue = [this] { return (int) ctx.macroCopy (ctx.ui.selectedMacro).pattern; };
    patternKnob.setValue = [this] (int v) { editMap ([v] (MacroMap& m, const Globals&) { m.pattern = (std::int8_t) v; }); };
    patternKnob.pixelsPerStep = 4.0f;
    addChildComponent (patternKnob);

    // STEP 1..32
    stepKnob.minValue = 1; stepKnob.maxValue = maxSteps; stepKnob.defaultValue = 1;
    stepKnob.colour = [] { return Theme::current().neutral; };
    stepKnob.getValue = [this] { return (int) ctx.macroCopy (ctx.ui.selectedMacro).step + 1; };
    stepKnob.setValue = [this] (int v) { editMap ([v] (MacroMap& m, const Globals&) { m.step = (std::uint8_t) (v - 1); }); };
    stepKnob.pixelsPerStep = 6.0f;
    addChildComponent (stepKnob);

    clearBtn.radius = 6.0f;
    clearBtn.font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    clearBtn.text = [] { return juce::String ("CLEAR MAPPING"); };
    clearBtn.look = [] { const auto& t = Theme::current(); return Pill::outline (t.border3, t.text2); };
    clearBtn.onClick = [this] { editMap ([] (MacroMap& m, const Globals&) { m = MacroMap(); }); };
    addAndMakeVisible (clearBtn);

    refreshState();
}

void MacrosView::chooseSetFile (bool save)
{
    const auto start = VimaProcessor::macroFolder().getChildFile (juce::String ("Mappings") + VimaProcessor::macroExtension);
    chooser = std::make_unique<juce::FileChooser> (save ? "Save macro mappings" : "Load macro mappings", start,
                                                   juce::String ("*") + VimaProcessor::macroExtension);
    const int flags = juce::FileBrowserComponent::canSelectFiles
                      | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode);
    juce::Component::SafePointer<MacrosView> safe (this);
    chooser->launchAsync (flags, [safe, save] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto file = fc.getResult();
        if (file == juce::File()) return;
        if (save)
        {
            if (! file.hasFileExtension (VimaProcessor::macroExtension))
                file = file.withFileExtension (VimaProcessor::macroExtension);
            safe->ctx.proc.saveMacros (file);
        }
        else if (safe->ctx.proc.loadMacros (file))
            safe->ctx.changed();
    });
}

void MacrosView::editMap (std::function<void (MacroMap&, const Globals&)> fn)
{
    // No two macros may point at the same control: a change that would do that is refused
    const int i = ctx.ui.selectedMacro;
    const auto result = ctx.proc.read ([i, &fn] (const Project& p)
    {
        auto m = p.macros[(size_t) i];
        fn (m, p.globals);
        int clash = -1;
        for (int k = 0; k < numMacros && clash < 0; ++k)
            if (k != i && sameControl (p.macros[(size_t) k], m)) clash = k;
        return std::make_pair (m, clash);
    });
    if (result.second >= 0)
    {
        conflict = "ALREADY ON " + macroId (result.second);
        repaint();
        return;
    }
    conflict.clear();
    const auto m = result.first;
    ctx.proc.edit (-1, [i, m] (Project& p) { p.macros[(size_t) i] = m; });
    ctx.changed();
}

void MacrosView::pickKind (MacroKind kind)
{
    const auto cur = ctx.macroCopy (ctx.ui.selectedMacro);
    conflict.clear();
    if (macroKind (cur.target) == kind)
        return;
    pickTarget (targetsOf (kind).front());
}

void MacrosView::pickTarget (MacroTarget target)
{
    // a new mapping starts on the track and pattern on screen; a changed one keeps its place
    const int track = ctx.ui.track, pattern = ctx.currentPattern();
    editMap ([target, track, pattern] (MacroMap& m, const Globals& g)
    {
        const auto keep = m;
        m = defaultMacroMap (target, keep.mapped() ? keep.track : track, keep.mapped() ? keep.pattern : pattern, keep.step, g);
    });
}

void MacrosView::refreshState()
{
    if (conflictMacro != ctx.ui.selectedMacro) conflict.clear();
    conflictMacro = ctx.ui.selectedMacro;
    const auto m = ctx.macroCopy (ctx.ui.selectedMacro);
    const auto kind = macroKind (m.target);
    shownTargets = targetsOf (kind);
    const auto& info = macroTargetInfo (m.target);

    for (int k = 0; k < targetBtns.size(); ++k) targetBtns[k]->setVisible (k < (int) shownTargets.size());
    const bool showTrack = kind == MacroKind::track || kind == MacroKind::step;
    for (auto* p : trackBtns) p->setVisible (showTrack);
    const bool showPattern = m.mapped() && info.perPattern;
    patternKnob.setVisible (showPattern);
    stepKnob.setVisible (kind == MacroKind::step);
    clearBtn.setVisible (m.mapped());
    resized();
    repaint();
    for (auto* c : cells) c->repaint();
}

void MacrosView::resized()
{
    auto r = getLocalBounds().reduced (20, 0).withTrimmedTop (14).withTrimmedBottom (16);
    aside = r.removeFromRight (360);
    r.removeFromRight (16);
    left = r;

    // page row and LEARN
    auto row = left.removeFromTop (32);
    pagesLabel = row.withWidth (0);
    for (auto* p : pageBtns) { p->setBounds (row.removeFromLeft (84).reduced (0, 1)); row.removeFromLeft (4); }
    learnBtn.setBounds (row.removeFromRight (104).reduced (0, 1));
    row.removeFromRight (16);
    loadSetBtn.setBounds (row.removeFromRight (72).reduced (0, 1));
    row.removeFromRight (6);
    saveSetBtn.setBounds (row.removeFromRight (72).reduced (0, 1));
    left.removeFromTop (10);
    learnBanner = {};
    if (ctx.ui.macroLearn)
    {
        learnBanner = left.removeFromTop (34);
        left.removeFromTop (10);
    }
    gridArea = left;
    const int gap = 8;
    const int cw = (gridArea.getWidth() - 7 * gap) / 8, ch = (gridArea.getHeight() - 3 * gap) / 4;
    for (int k = 0; k < cells.size(); ++k)
        cells[k]->setBounds (gridArea.getX() + (k % 8) * (cw + gap), gridArea.getY() + (k / 8) * (ch + gap), cw, ch);

    // aside: mapping editor
    auto a = aside.reduced (16, 0);
    asideHead = a.removeFromTop (52);
    a.removeFromTop (14);
    auto section = [&a] (juce::Rectangle<int>& label, int height)
    {
        label = a.removeFromTop (12);
        a.removeFromTop (8);
        auto s = a.removeFromTop (height);
        a.removeFromTop (16);
        return s;
    };

    footer = aside.withTrimmedTop (aside.getHeight() - 60).reduced (16, 13);
    clearBtn.setBounds (footer.withWidth (124));

    auto kindsRow = section (kindLabel, 30);
    const int kw = (kindsRow.getWidth() - 2 * 4) / 3;
    for (int k = 0; k < kindBtns.size(); ++k) kindBtns[k]->setBounds (kindsRow.getX() + k * (kw + 4), kindsRow.getY(), kw, 30);

    targetLabel = trackLabel = patternLabel_ = stepLabel = emptyText = patternText = {};
    if (shownTargets.empty())
    {
        emptyText = a.removeFromTop (20);
        return;
    }

    const int rows = ((int) shownTargets.size() + 2) / 3;
    auto grid = section (targetLabel, rows * 28 + (rows - 1) * 4);
    const int tw = (grid.getWidth() - 2 * 4) / 3;
    for (int k = 0; k < (int) shownTargets.size(); ++k)
        targetBtns[k]->setBounds (grid.getX() + (k % 3) * (tw + 4), grid.getY() + (k / 3) * 32, tw, 28);

    if (trackBtns[0]->isVisible())
    {
        auto tr = section (trackLabel, 28);
        const int w = (tr.getWidth() - 7 * 4) / 8;
        for (auto* b : trackBtns) { b->setBounds (tr.removeFromLeft (w)); tr.removeFromLeft (4); }
    }
    if (patternKnob.isVisible())
    {
        auto pr = section (patternLabel_, 40);
        patternKnob.setBounds (pr.removeFromLeft (40));
        pr.removeFromLeft (8);
        patternText = pr;
    }
    if (stepKnob.isVisible())
    {
        auto sr = section (stepLabel, 40);
        stepKnob.setBounds (sr.removeFromLeft (40));
    }
}

void MacrosView::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    g.fillAll (t.bg);

    // mapped macros in total, all pages (SPEC.md 13.2)
    const int mapped = ctx.proc.read ([] (const Project& p)
    {
        int n = 0;
        for (const auto& m : p.macros) n += m.mapped() ? 1 : 0;
        return n;
    });
    {
        auto b = pagesLabel.withX (pageBtns.getLast()->getRight() + 16).withWidth (160);
        g.setColour (t.muted);
        g.setFont (Fonts::mono (10.0f));
        g.drawText (juce::String (mapped) + " OF " + juce::String (numMacros) + " MAPPED", b, juce::Justification::centredLeft);
    }

    if (! learnBanner.isEmpty())
    {
        juce::Path p, dashed;
        p.addRoundedRectangle (learnBanner.toFloat().reduced (0.5f), 6.0f);
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (t.amber);
        g.fillPath (dashed);
        auto b = learnBanner.reduced (14, 0);
        g.setFont (Fonts::monoSemi (12.0f));
        g.drawText (macroId (ctx.ui.selectedMacro), b.removeFromLeft (44), juce::Justification::centredLeft);
        g.setFont (Fonts::sans (12.0f));
        g.drawText ("is listening. In STEPS, click a control to map it. Mapped controls are outlined; right-click one to free it.", b,
                    juce::Justification::centredLeft);
    }

    draw::tile (g, aside.toFloat(), t.panel, t.border, 8.0f);
    const auto m = ctx.macroCopy (ctx.ui.selectedMacro);
    {
        auto h = asideHead.reduced (16, 0);
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (15.0f));
        g.drawText (macroId (ctx.ui.selectedMacro), h.removeFromLeft (56), juce::Justification::centredLeft);
        draw::label (g, "HOST NAME: MACRO " + juce::String (ctx.ui.selectedMacro + 1), h, t.muted);
        g.setColour (t.border);
        g.fillRect (aside.getX(), asideHead.getBottom(), aside.getWidth(), 1);
        g.fillRect (aside.getX(), footer.getY() - 13, aside.getWidth(), 1);
    }
    draw::label (g, "TARGET", kindLabel, t.muted);
    if (! emptyText.isEmpty())
    {
        g.setColour (t.muted);
        g.setFont (Fonts::sans (12.0f));
        g.drawText ("Not mapped. Pick a target type, or use LEARN.", emptyText, juce::Justification::centredLeft);
    }
    draw::label (g, "PARAMETER", targetLabel, t.muted);
    draw::label (g, "TRACK", trackLabel, t.muted);
    draw::label (g, "PATTERN", patternLabel_, t.muted);
    draw::label (g, "STEP", stepLabel, t.muted);
    if (! patternText.isEmpty() && m.mapped() && macroTargetInfo (m.target).perPattern)
    {
        g.setColour (t.text);
        g.setFont (Fonts::monoSemi (13.0f));
        g.drawText (patternLabel (m.pattern), patternText, juce::Justification::centredLeft);
    }
    if (m.mapped())
    {
        const auto now = ctx.proc.read ([&m] (const Project& p)
        {
            const auto v = macroTargetValue (p, m);
            return v ? macroValueText (p, m, *v) : juce::String();
        });
        g.setColour (t.muted);
        g.setFont (Fonts::mono (11.0f));
        if (conflict.isEmpty())
            g.drawText ("NOW " + now, footer, juce::Justification::centredRight);
    }
    if (conflict.isNotEmpty())
    {
        g.setColour (t.amber);
        g.setFont (Fonts::monoSemi (11.0f));
        g.drawText (conflict, footer, juce::Justification::centredRight);
    }
}
} // namespace vima
