#include "SettingsDialog.h"

namespace vima
{
namespace
{
    Pill::Look choice (bool on, juce::Colour accent)
    {
        const auto& t = Theme::current();
        return on ? Pill::filled (accent, accent == t.neutral ? t.onNeutral : t.onAccent)
                  : Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.text2, 0.0f, false };
    }
}

Pill& SettingsDialog::pill()
{
    auto* p = pills.add (new Pill());
    p->radius = 5.0f;
    p->font = [] { return Fonts::monoSemi (11.0f); };
    addAndMakeVisible (p);
    return *p;
}

Knob& SettingsDialog::knob()
{
    auto* k = knobs.add (new Knob());
    k->thickness = 3.0f;
    k->fontSize = 11.0f;
    addAndMakeVisible (k);
    return *k;
}

SettingsDialog::SettingsDialog (Context& c) : ctx (c)
{
    setComponentID ("settings");
    auto G = [] { return Theme::current().green; };
    auto C = [] { return Theme::current().cyan; };
    auto N = [] { return Theme::current().neutral; };

    // Footer buttons. There is no close (x) button: DONE or a click outside closes the dialog,
    // and every setting applies the moment it is clicked (SPEC.md 11).
    resetBtn = &pill();
    resetBtn->radius = 6.0f;
    resetBtn->look = [] { const auto& t = Theme::current(); return Pill::outline (t.border2, t.text2); };
    resetBtn->text = [] { return juce::String ("RESET DEFAULTS"); };
    resetBtn->font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    resetBtn->onClick = [this]
    {
        ctx.edit (false, [] (Project& p)
        {
            const Globals d;
            auto& g = p.globals;
            g.root = d.root; g.userScale = d.userScale; g.ccSmoothing = d.ccSmoothing; g.ccCurve = d.ccCurve;
            g.patternCC = d.patternCC; g.patternCCChannel = d.patternCCChannel; g.patternCCNumber = d.patternCCNumber; g.quantize = d.quantize;
            g.midiThru = d.midiThru; g.noteTrigger = d.noteTrigger; g.triggerTracks = d.triggerTracks; g.triggerQuantize = d.triggerQuantize; g.basePitchClass = d.basePitchClass; g.baseOctave = d.baseOctave;
        });
        Theme::setLight (false);       // the theme default: dark (all instances)
        userSettings::setMiddleC (3);  // middle C = C3 (all instances)
        ctx.changed();
    };

    // GITHUB: a small link to the repository (a generic "open link" icon, not GitHub's logo)
    githubBtn = &pill();
    githubBtn->radius = 6.0f;
    githubBtn->look = [] { const auto& t = Theme::current(); return Pill::Look { juce::Colours::transparentBlack, juce::Colours::transparentBlack, t.muted, 0.0f, false }; };
    githubBtn->text = [] { return juce::String ("VIMA ON GITHUB"); };
    githubBtn->font = [] { return Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f); };
    githubBtn->justification = juce::Justification::centredLeft;
    githubBtn->textInset = 30;
    githubBtn->icon = [] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
    {
        // open-link icon: a box with an arrow pointing out of its top right corner
        const auto box = juce::Rectangle<float> (b.getX() + 10.0f, b.getCentreY() - 6.0f, 12.0f, 12.0f);
        juce::Path p;
        p.startNewSubPath (box.getX() + 6.0f, box.getY());
        p.lineTo (box.getX(), box.getY());
        p.lineTo (box.getX(), box.getBottom());
        p.lineTo (box.getRight(), box.getBottom());
        p.lineTo (box.getRight(), box.getY() + 6.0f);
        p.startNewSubPath (box.getX() + 5.0f, box.getY() + 7.0f);
        p.lineTo (box.getRight() + 1.0f, box.getY() - 1.0f);
        p.startNewSubPath (box.getRight() - 4.0f, box.getY() - 1.0f);
        p.lineTo (box.getRight() + 1.0f, box.getY() - 1.0f);
        p.lineTo (box.getRight() + 1.0f, box.getY() + 4.0f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        g.setFont (Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f));
        g.drawText ("VIMA ON GITHUB", b.withTrimmedLeft (30.0f), juce::Justification::centredLeft, false);
    };
    githubBtn->onClick = [] { juce::URL ("https://github.com/TheoGal/vima-vst").launchInDefaultBrowser(); };
    githubBtn->setTitle ("Vima on GitHub");

    // MANUAL: opens the PDF manual from the plugin's bundle (Contents/Resources/Vima Manual.pdf);
    // without it, the online manual on GitHub (SPEC.md 14.8)
    manualBtn = &pill();
    manualBtn->radius = 6.0f;
    manualBtn->look = githubBtn->look;
    manualBtn->text = [] { return juce::String ("MANUAL"); };
    manualBtn->font = githubBtn->font;
    manualBtn->justification = juce::Justification::centredLeft;
    manualBtn->textInset = 30;
    manualBtn->icon = [] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
    {
        // document icon: a page with a folded corner and three text lines
        const auto page = juce::Rectangle<float> (b.getX() + 11.0f, b.getCentreY() - 7.0f, 11.0f, 14.0f);
        juce::Path p;
        p.startNewSubPath (page.getRight() - 4.0f, page.getY());
        p.lineTo (page.getX(), page.getY());
        p.lineTo (page.getX(), page.getBottom());
        p.lineTo (page.getRight(), page.getBottom());
        p.lineTo (page.getRight(), page.getY() + 4.0f);
        p.lineTo (page.getRight() - 4.0f, page.getY());
        p.lineTo (page.getRight() - 4.0f, page.getY() + 4.0f);
        p.lineTo (page.getRight(), page.getY() + 4.0f);
        for (int i = 0; i < 3; ++i)
        {
            const float y = page.getY() + 6.5f + (float) i * 2.5f;
            p.startNewSubPath (page.getX() + 2.5f, y);
            p.lineTo (page.getRight() - 2.5f, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        g.setFont (Fonts::sansSemi (11.0f).withExtraKerningFactor (0.1f));
        g.drawText ("MANUAL", b.withTrimmedLeft (30.0f), juce::Justification::centredLeft, false);
    };
    manualBtn->onClick = []
    {
        // the plugin binary sits in <bundle>/Contents/<platform folder>/; the PDF in <bundle>/Contents/Resources
        const auto bin = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto pdf = bin.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile ("Vima Manual.pdf");
        if (pdf.existsAsFile()) pdf.startAsProcess();
        else juce::URL ("https://github.com/TheoGal/vima-vst/blob/main/MANUAL.md").launchInDefaultBrowser();
    };
    manualBtn->setTitle ("Vima manual");

    doneBtn = &pill();
    doneBtn->radius = 6.0f;
    doneBtn->look = [] { const auto& t = Theme::current(); return Pill::filled (t.neutral, t.onNeutral); };
    doneBtn->text = [] { return juce::String ("DONE"); };
    doneBtn->font = [] { return Fonts::sansBold (12.0f).withExtraKerningFactor (0.12f); };
    doneBtn->onClick = [this] { if (onClose) onClose(); };

    // Appearance
    for (int i = 0; i < 2; ++i)
    {
        auto& p = pill();
        p.text = [i] { return juce::String (i == 0 ? "DARK" : "LIGHT"); };
        p.look = [i, N] { return choice (Theme::isLight() == (i == 1), N()); };
        p.onClick = [this, i] { Theme::setLight (i == 1); ctx.changed(); };   // user-wide, all instances
        themeBtns.add (&p);
    }

    // Pitch
    // Middle C: C3 (Live, Logic, Cubase...), C4 (Reaper, scientific), C5 (FL Studio, Cakewalk, Renoise)
    for (int i = 0; i < 3; ++i)
    {
        auto& p = pill();
        p.text = [i] { return juce::StringArray { "C3 (LOWEST C-2)", "C4 (LOWEST C-1)", "C5 (LOWEST C0)" }[i]; };
        p.font = [] { return Fonts::monoSemi (10.0f); };
        p.look = [i, G] { return choice (userSettings::middleC() == 3 + i, G()); };
        p.onClick = [this, i] { userSettings::setMiddleC (3 + i); ctx.changed(); };   // user-wide
        midCBtns.add (&p);
    }

    // User scale: root row and 12 note toggles (relative to the root)
    for (int i = 0; i < 12; ++i)
    {
        auto& r = pill();
        r.radius = 4.0f;
        r.font = [] { return Fonts::monoSemi (10.0f); };
        r.text = [i] { return juce::String (pitchClassName (i)); };
        r.look = [this, i]
        {
            const auto& t = Theme::current();
            return ctx.globalsCopy().root == i ? Pill::filled (t.green, t.onAccent) : Pill::outline (t.border2, t.text2);
        };
        r.onClick = [this, i] { ctx.edit (false, [i] (Project& p) { p.globals.root = i; }); };
        rootBtns.add (&r);
    }
    for (int i = 0; i < 12; ++i)
    {
        auto& n = pill();
        n.text = [this, i] { return juce::String (pitchClassName (ctx.globalsCopy().root + i)); };
        n.look = [this, i]
        {
            const auto& t = Theme::current();
            const bool on = i == 0 || ((ctx.globalsCopy().userScale >> i) & 1);
            return on ? Pill::tinted (t.green) : Pill::outline (t.border2, t.muted);
        };
        n.onClick = [this, i]
        {
            if (i == 0) return;   // the root is always included
            ctx.edit (false, [i] (Project& p) { p.globals.userScale = (std::uint16_t) ((p.globals.userScale ^ (1u << i)) | 1u); });
        };
        userBtns.add (&n);
    }

    // CC output
    smoothBtn = &pill();
    smoothBtn->radius = 4.0f;
    smoothBtn->font = [] { return Fonts::monoSemi (10.0f); };
    smoothBtn->look = [this, C] { return Pill::toggle (ctx.globalsCopy().ccSmoothing, C()); };
    smoothBtn->text = [this] { return juce::String (ctx.globalsCopy().ccSmoothing ? "ON" : "OFF"); };
    smoothBtn->onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.ccSmoothing = ! p.globals.ccSmoothing; }); };

    // the glide's curve: LINEAR / EXP / LOG, each with a small drawing of its shape; dimmed while
    // smoothing is off (it only applies to the glide)
    const char* curveNames[] = { "LINEAR", "EXP", "LOG" };
    for (int i = 0; i < 3; ++i)
    {
        auto& p = pill();
        const juce::String name (curveNames[i]);
        p.text = [name] { return name; };
        p.font = [] { return Fonts::monoSemi (10.0f); };
        p.look = [this, i, C] { return choice ((int) ctx.globalsCopy().ccCurve == i, C()); };
        p.alpha = [this] { return ctx.globalsCopy().ccSmoothing ? 1.0f : 0.4f; };
        p.icon = [i, name] (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c)
        {
            auto box = juce::Rectangle<float> (b.getX() + 10.0f, b.getCentreY() - 5.0f, 16.0f, 10.0f);
            juce::Path path;
            for (int k = 0; k <= 16; ++k)
            {
                const double f = k / 16.0;
                const double v = i == 1 ? f * f * f : i == 2 ? 1.0 - (1.0 - f) * (1.0 - f) * (1.0 - f) : f;
                const float x = box.getX() + box.getWidth() * (float) f, y = box.getBottom() - box.getHeight() * (float) v;
                if (k == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setFont (Fonts::monoSemi (10.0f));
            g.drawText (name, b.withTrimmedLeft (32.0f), juce::Justification::centredLeft, false);
        };
        p.setTitle (name.toLowerCase() + " curve");
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.ccCurve = (CCCurve) i; }); };
        curveBtns.add (&p);
    }

    // Pattern change
    patternCCBtn = &pill();
    patternCCBtn->radius = 4.0f;
    patternCCBtn->font = [] { return Fonts::monoSemi (10.0f); };
    patternCCBtn->look = [this, N]
    {
        const auto& t = Theme::current();
        const bool on = ctx.globalsCopy().patternCC;
        return on ? Pill::Look { N(), N(), t.onNeutral, 1.5f, false } : Pill::Look { juce::Colours::transparentBlack, t.dim, t.muted, 1.5f, false };
    };
    patternCCBtn->text = [this] { return juce::String (ctx.globalsCopy().patternCC ? "ON" : "OFF"); };
    patternCCBtn->onClick = [this] { ctx.edit (false, [] (Project& p) { p.globals.patternCC = ! p.globals.patternCC; }); };
    pcChannel = &knob();
    pcChannel->colour = N;
    pcChannel->minValue = 1; pcChannel->maxValue = 16; pcChannel->defaultValue = 16;
    pcChannel->getValue = [this] { return ctx.globalsCopy().patternCCChannel; };
    pcChannel->setValue = [this] (int v) { ctx.edit (false, [v] (Project& p) { p.globals.patternCCChannel = v; }); };
    pcNumber = &knob();
    pcNumber->colour = N;
    pcNumber->minValue = 0; pcNumber->maxValue = 127; pcNumber->defaultValue = 20;
    pcNumber->getValue = [this] { return ctx.globalsCopy().patternCCNumber; };
    pcNumber->setValue = [this] (int v) { ctx.edit (false, [v] (Project& p) { p.globals.patternCCNumber = v; }); };

    const char* qNames[] = { "OFF", "1 BEAT", "1 BAR", "PATTERN END" };
    for (int i = 0; i < 4; ++i)
    {
        auto& p = pill();
        p.text = [n = juce::String (qNames[i])] { return n; };
        p.look = [this, i, N] { return choice ((int) ctx.globalsCopy().quantize == i, N()); };
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.quantize = (Quantize) i; }); };
        quantBtns.add (&p);
    }

    // the tracks the note trigger drives (SPEC.md 3.1)
    for (int i = 0; i < numTracks; ++i)
    {
        auto& p = pill();
        p.text = [i] { return juce::String (i + 1).paddedLeft ('0', 2); };
        p.font = [] { return Fonts::monoSemi (10.0f); };
        p.look = [this, i, N]
        {
            const auto& t = Theme::current();
            return ((ctx.globalsCopy().triggerTracks >> i) & 1) ? Pill::Look { N(), N(), t.onNeutral, 1.5f, false } : Pill::outline (t.border2, t.muted);
        };
        p.alpha = [this] { return ctx.globalsCopy().noteTrigger == NoteTrigger::off ? 0.4f : 1.0f; };
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.triggerTracks = (std::uint8_t) (pr.globals.triggerTracks ^ (1 << i)); }); };
        p.setTitle ("Trigger track " + juce::String (i + 1));
        trigTrackBtns.add (&p);
    }

    // the note trigger's own quantize (its starts and stops)
    const char* tqNames[] = { "OFF", "1/32", "1/16", "1/8", "1 BEAT", "1 BAR" };
    for (int i = 0; i < 6; ++i)
    {
        auto& p = pill();
        p.text = [n = juce::String (tqNames[i])] { return n; };
        p.look = [this, i, N] { return choice ((int) ctx.globalsCopy().triggerQuantize == i, N()); };
        p.alpha = [this] { return ctx.globalsCopy().noteTrigger == NoteTrigger::off ? 0.4f : 1.0f; };
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.triggerQuantize = (TriggerQuantize) i; }); };
        p.setTitle ("Trigger quantize " + juce::String (tqNames[i]));
        trigQuantBtns.add (&p);
    }

    // MIDI input (SPEC.md 3.1): MIDI thru, note trigger, base note
    thruBtn = &pill();
    thruBtn->radius = 4.0f;
    thruBtn->font = [] { return Fonts::monoSemi (10.0f); };
    thruBtn->look = [this, N]
    {
        const auto& t = Theme::current();
        const auto g = ctx.globalsCopy();
        const bool on = g.midiThru && g.noteTrigger == NoteTrigger::off;
        return on ? Pill::Look { N(), N(), t.onNeutral, 1.5f, false } : Pill::Look { juce::Colours::transparentBlack, t.dim, t.muted, 1.5f, false };
    };
    // NOTE INPUT uses every incoming note: THRU shows OFF and is greyed out (its own value is kept)
    thruBtn->alpha = [this] { return ctx.globalsCopy().noteTrigger != NoteTrigger::off ? 0.4f : 1.0f; };
    thruBtn->text = [this] { const auto g = ctx.globalsCopy(); return juce::String (g.midiThru && g.noteTrigger == NoteTrigger::off ? "ON" : "OFF"); };
    thruBtn->onClick = [this]
    {
        if (ctx.globalsCopy().noteTrigger != NoteTrigger::off) return;
        ctx.edit (false, [] (Project& p) { p.globals.midiThru = ! p.globals.midiThru; });
    };
    thruBtn->setTitle ("MIDI thru");
    const char* tNames[] = { "OFF", "TRANSPOSE", "TRIGGER" };   // NOTE INPUT (SPEC.md 3.1)
    for (int i = 0; i < 3; ++i)
    {
        auto& p = pill();
        const juce::String name (tNames[i]);
        p.text = [name] { return name; };
        p.font = [] { return Fonts::monoSemi (10.0f); };
        p.look = [this, i, N] { return choice ((int) ctx.globalsCopy().noteTrigger == i, N()); };
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.noteTrigger = (NoteTrigger) i; }); };
        p.setTitle ("Note input " + name.toLowerCase());
        trigBtns.add (&p);
    }
    // base note: a note knob (all 12 notes) and an octave knob, as on a step
    basePitch = &knob();
    basePitch->thickness = 4.0f;
    basePitch->fontSize = 12.0f;
    basePitch->colour = [] { return Theme::current().green; };
    basePitch->minValue = 0; basePitch->maxValue = 11; basePitch->defaultValue = 0;
    basePitch->getValue = [this] { return (int) ctx.globalsCopy().basePitchClass; };
    basePitch->format = [] (int pc) { return juce::String (pitchClassName (pc)); };
    basePitch->setValue = [this] (int pc) { ctx.edit (false, [pc] (Project& p) { p.globals.basePitchClass = (std::int8_t) pc; }); };   // all 12 notes
    basePitch->parse = [] (const juce::String& txt)
    {
        const auto u = txt.toUpperCase().trim();
        for (int pc = 11; pc >= 0; --pc)
            if (u.startsWith (pitchClassName (pc)) && (juce::String (pitchClassName (pc)).length() == 2 || ! u.contains ("#")))
                return pc;
        return 0;
    };
    basePitch->pixelsPerStep = 8.0f;
    baseOctave = &knob();
    baseOctave->thickness = 3.0f;
    baseOctave->fontSize = 11.0f;
    baseOctave->textColour = Theme::current().text2;
    baseOctave->colour = [] { return Theme::current().green; };
    baseOctave->minValue = minOctave; baseOctave->maxValue = maxOctave + 2; baseOctave->defaultValue = 3;
    baseOctave->getValue = [this] { const auto g = ctx.globalsCopy(); return g.baseOctave + g.octaveOffset(); };
    baseOctave->fraction = [this] { return (float) (ctx.globalsCopy().baseOctave - minOctave) / (float) (maxOctave - minOctave); };
    baseOctave->setValue = [this] (int shown)
    {
        ctx.edit (false, [shown] (Project& p)
        {
            p.globals.baseOctave = (std::int8_t) juce::jlimit (minOctave, maxOctave, shown - p.globals.octaveOffset());
        });
    };
    baseOctave->pixelsPerStep = 10.0f;
}

// The Settings view fills the area below the header (SPEC.md 11)
juce::Rectangle<int> SettingsDialog::panelBounds() const
{
    return getLocalBounds();
}

void SettingsDialog::mouseDown (const juce::MouseEvent&) {}

void SettingsDialog::resized()
{
    rows.clear();
    dividers.clear();
    const auto panel = panelBounds();
    // footer: RESET DEFAULTS on the left, the MANUAL and VIMA ON GITHUB links on the right
    auto footer = panel.withTop (panel.getBottom() - 60).reduced (20, 13);
    resetBtn->setBounds (footer.removeFromLeft (140));
    doneBtn->setVisible (false);   // a view, not a dialog: SETTINGS or another view's button leaves it
    githubBtn->setBounds (footer.removeFromRight (150));
    footer.removeFromRight (4);
    manualBtn->setBounds (footer.removeFromRight (100));

    auto inner = panel.withTrimmedBottom (60).reduced (40, 24);
    const int colGap = 40, colW = (inner.getWidth() - colGap) / 2;
    auto leftCol = inner.withWidth (colW), rightCol = inner.withTrimmedLeft (colW + colGap);
    juce::Rectangle<int> body;
    const int labelW = 140;
    auto section = [&] (const juce::String& name)
    {
        rows.add ({ name, body.removeFromTop (12), 1.0f, true });
        body.removeFromTop (10);
    };
    auto rowArea = [&] (const juce::String& name, int h)
    {
        auto r = body.removeFromTop (h);
        rows.add ({ name, r.removeFromLeft (labelW), 1.0f, false });
        r.removeFromLeft (16);
        body.removeFromTop (10);
        return r;
    };
    auto divider = [&]
    {
        body.removeFromTop (2);
        dividers.add (body.withHeight (1));
        body.removeFromTop (12);
    };
    auto segment = [] (juce::Rectangle<int> r, juce::Array<Pill*>& btns, int w)
    {
        auto box = r.withWidth (btns.size() * w + (btns.size() - 1) * 3 + 6).withSizeKeepingCentre (btns.size() * w + (btns.size() - 1) * 3 + 6, 32);
        for (int i = 0; i < btns.size(); ++i)
            btns[i]->setBounds (box.getX() + 3 + i * (w + 3), box.getY() + 3, w, 26);
        return box;
    };

    // left column: how Vima looks and sounds
    body = leftCol;
    section ("APPEARANCE");
    segment (rowArea ("Theme", 32), themeBtns, 64);
    divider();

    section ("MIDDLE C");   // not "PITCH": that is the name of the step knob
    segment (rowArea ("Middle C is", 32), midCBtns, 108);
    divider();

    section ("USER SCALE");
    {
        auto r = body.removeFromTop (26);
        rows.add ({ "ROOT", r.removeFromLeft (44), 1.0f, true });
        const int w = (r.getWidth() - 4 * 12) / 12;
        for (int i = 0; i < 12; ++i) rootBtns[i]->setBounds (r.getX() + 4 + i * (w + 4), r.getY(), w, 26);
        body.removeFromTop (6);
        auto n = body.removeFromTop (36);
        rows.add ({ "NOTES", n.removeFromLeft (44), 1.0f, true });
        for (int i = 0; i < 12; ++i) userBtns[i]->setBounds (n.getX() + 4 + i * (w + 4), n.getY(), w, 36);
        body.removeFromTop (6);
    }
    divider();

    section ("CC OUTPUT");
    smoothBtn->setBounds (rowArea ("Smooth between steps", 24).withWidth (60));
    segment (rowArea ("Curve", 32), curveBtns, 86);

    // right column: incoming MIDI and timing
    body = rightCol;
    section ("MIDI INPUT");
    thruBtn->setBounds (rowArea ("MIDI thru", 24).withWidth (60));
    segment (rowArea ("Note input", 32), trigBtns, 86);
    {
        auto r = rowArea ("Tracks", 26);
        for (int i = 0; i < numTracks; ++i) trigTrackBtns[i]->setBounds (r.getX() + i * 40, r.getY(), 36, 26);
    }
    {
        auto r = rowArea ("Quantize", 32);
        const int widths[] = { 46, 50, 50, 44, 66, 58 };
        int total = 6 + 5 * 3;
        for (int w : widths) total += w;
        auto box = r.withWidth (total).withSizeKeepingCentre (total, 32);
        int x = box.getX() + 3;
        for (int i = 0; i < 6; ++i) { trigQuantBtns[i]->setBounds (x, box.getY() + 3, widths[i], 26); x += widths[i] + 3; }
    }
    {
        auto r = rowArea ("Base note", 46);
        basePitch->setBounds (r.removeFromLeft (46));
        r.removeFromLeft (8);
        rows.add ({ "NOTE", r.removeFromLeft (40), 1.0f, true });
        r.removeFromLeft (14);
        baseOctave->setBounds (r.removeFromLeft (34).withSizeKeepingCentre (34, 34));
        r.removeFromLeft (8);
        rows.add ({ "OCT", r.removeFromLeft (30), 1.0f, true });
    }
    divider();

    section ("PATTERN CHANGE");
    {
        auto r = rowArea ("From incoming CC", 40);
        patternCCBtn->setBounds (r.removeFromLeft (60).withSizeKeepingCentre (60, 24));
        r.removeFromLeft (14);
        pcChannel->setBounds (r.removeFromLeft (40));
        r.removeFromLeft (8);
        rows.add ({ "MIDI CH", r.removeFromLeft (52), 1.0f, true });
        r.removeFromLeft (10);
        pcNumber->setBounds (r.removeFromLeft (40));
        r.removeFromLeft (8);
        rows.add ({ "CC", r.removeFromLeft (30), 1.0f, true });
    }
    {
        auto r = rowArea ("Quantize changes to", 32);
        auto box = r.withWidth (6 + 52 + 3 + 64 + 3 + 58 + 3 + 104).withSizeKeepingCentre (6 + 52 + 3 + 64 + 3 + 58 + 3 + 104, 32);
        const int widths[] = { 52, 64, 58, 104 };
        int x = box.getX() + 3;
        for (int i = 0; i < 4; ++i) { quantBtns[i]->setBounds (x, box.getY() + 3, widths[i], 26); x += widths[i] + 3; }
    }
}

void SettingsDialog::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    g.fillAll (t.bg);   // a view, like PATTERNS and MACROS
    const auto panel = panelBounds();
    g.setColour (t.border);
    g.fillRect (panel.getX(), panel.getBottom() - 60, panel.getWidth(), 1);
    for (auto d : dividers)
        g.fillRect (d);
    g.fillRect (panel.getCentreX(), panel.getY() + 24, 1, panel.getHeight() - 60 - 48);   // the column split

    const auto gl = ctx.globalsCopy();
    for (const auto& r : rows)
    {
        float a = 1.0f;
        if (r.text == "MIDI CH" || r.text == "CC") a = gl.patternCC ? 1.0f : 0.4f;
        if (r.text == "Curve") a = gl.ccSmoothing ? 1.0f : 0.4f;
        if (r.text == "MIDI thru") a = gl.noteTrigger != NoteTrigger::off ? 0.4f : 1.0f;
        if (r.text == "Quantize" || r.text == "Tracks") a = gl.noteTrigger == NoteTrigger::off ? 0.4f : 1.0f;
        if (r.text == "Base note" || r.text == "NOTE" || r.text == "OCT") a = gl.noteTrigger == NoteTrigger::off ? 0.4f : 1.0f;
        if (r.section) draw::label (g, r.text, r.area, t.muted.withMultipliedAlpha (a));
        else
        {
            g.setColour (t.text.withMultipliedAlpha (a));
            g.setFont (Fonts::sans (13.0f));
            g.drawText (r.text, r.area, juce::Justification::centredLeft);
        }
    }

    // segment frames
    for (auto* group : { &themeBtns, &midCBtns, &quantBtns, &curveBtns, &trigBtns, &trigQuantBtns })
    {
        auto first = (*group)[0]->getBounds(), last = (*group)[group->size() - 1]->getBounds();
        auto frame = first.getUnion (last).expanded (3);
        draw::tile (g, frame.toFloat(), t.tile, t.border, 7.0f);
    }
    // user scale count
    int count = 0;
    for (int i = 0; i < 12; ++i) count += (i == 0 || ((gl.userScale >> i) & 1)) ? 1 : 0;
    g.setColour (t.muted);
    g.setFont (Fonts::mono (10.0f));
    for (const auto& r : rows)
        if (r.text == "USER SCALE")
            g.drawText (juce::String (count) + " OF 12 NOTES", r.area.withX (panel.getX() + 24).withWidth (panel.getWidth() / 2 - 44),
                        juce::Justification::centredRight);

    pcChannel->setAlpha (gl.patternCC ? 1.0f : 0.4f);
    pcNumber->setAlpha (gl.patternCC ? 1.0f : 0.4f);
    basePitch->setAlpha (gl.noteTrigger == NoteTrigger::off ? 0.4f : 1.0f);
    baseOctave->setAlpha (gl.noteTrigger == NoteTrigger::off ? 0.4f : 1.0f);
}
} // namespace vima
