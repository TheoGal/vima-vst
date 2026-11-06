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
}

juce::Rectangle<int> SettingsDialog::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (600, 646);   // sized to its content
}

void SettingsDialog::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()) && onClose)
        onClose();
}

void SettingsDialog::resized()
{
    rows.clear();
    dividers.clear();
    const auto panel = panelBounds();
    // footer: RESET DEFAULTS and DONE on the left, the MANUAL and VIMA ON GITHUB links on the right
    auto footer = panel.withTop (panel.getBottom() - 60).reduced (20, 13);
    resetBtn->setBounds (footer.removeFromLeft (140));
    footer.removeFromLeft (8);
    doneBtn->setBounds (footer.removeFromLeft (90));
    githubBtn->setBounds (footer.removeFromRight (150));
    footer.removeFromRight (4);
    manualBtn->setBounds (footer.removeFromRight (100));

    auto body = panel.withTrimmedTop (54).withTrimmedBottom (60).reduced (20, 14);
    const int labelW = 150;
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
        dividers.add (body.getY());
        body.removeFromTop (12);
    };
    auto segment = [] (juce::Rectangle<int> r, juce::Array<Pill*>& btns, int w)
    {
        auto box = r.withWidth (btns.size() * w + (btns.size() - 1) * 3 + 6).withSizeKeepingCentre (btns.size() * w + (btns.size() - 1) * 3 + 6, 32);
        for (int i = 0; i < btns.size(); ++i)
            btns[i]->setBounds (box.getX() + 3 + i * (w + 3), box.getY() + 3, w, 26);
        return box;
    };

    section ("APPEARANCE");
    segment (rowArea ("Theme", 32), themeBtns, 64);
    divider();

    section ("MIDDLE C");   // not "PITCH": that is the name of the step knob
    segment (rowArea ("Middle C is", 32), midCBtns, 120);
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
    g.fillAll (juce::Colours::black.withAlpha (Theme::isLight() ? 0.25f : 0.55f));
    const auto panel = panelBounds();
    paintPanel (g, panel.toFloat(), 10.0f);

    g.setColour (t.text);
    g.setFont (Fonts::sansSemi (13.0f).withExtraKerningFactor (0.14f));
    g.drawText ("SETTINGS", panel.withHeight (54).withTrimmedLeft (20), juce::Justification::centredLeft);
    g.setColour (t.border);
    g.fillRect (panel.getX(), panel.getY() + 54, panel.getWidth(), 1);
    g.fillRect (panel.getX(), panel.getBottom() - 60, panel.getWidth(), 1);
    for (int y : dividers)
        g.fillRect (panel.getX() + 20, y, panel.getWidth() - 40, 1);

    const auto gl = ctx.globalsCopy();
    for (const auto& r : rows)
    {
        float a = 1.0f;
        if (r.text == "MIDI CH" || r.text == "CC") a = gl.patternCC ? 1.0f : 0.4f;
        if (r.text == "Curve") a = gl.ccSmoothing ? 1.0f : 0.4f;
        if (r.section) draw::label (g, r.text, r.area, t.muted.withMultipliedAlpha (a));
        else
        {
            g.setColour (t.text.withMultipliedAlpha (a));
            g.setFont (Fonts::sans (13.0f));
            g.drawText (r.text, r.area, juce::Justification::centredLeft);
        }
    }

    // segment frames
    for (auto* group : { &themeBtns, &midCBtns, &quantBtns, &curveBtns })
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
            g.drawText (juce::String (count) + " OF 12 NOTES", r.area.withX (panel.getX() + 20).withWidth (panel.getWidth() - 40),
                        juce::Justification::centredRight);

    pcChannel->setAlpha (gl.patternCC ? 1.0f : 0.4f);
    pcNumber->setAlpha (gl.patternCC ? 1.0f : 0.4f);
}
} // namespace vima
