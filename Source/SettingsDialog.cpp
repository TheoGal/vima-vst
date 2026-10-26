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

    // Header close and footer buttons
    closeBtn = &pill();
    closeBtn->radius = 6.0f;
    closeBtn->look = [] { const auto& t = Theme::current(); return Pill::outline (t.border, t.text2); };
    closeBtn->text = [] { return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")); };
    closeBtn->font = [] { return Fonts::sans (16.0f); };
    closeBtn->onClick = [this] { if (onClose) onClose(); };

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
            g.root = d.root; g.userScale = d.userScale; g.ccSmoothing = d.ccSmoothing;
            g.patternCC = d.patternCC; g.patternCCChannel = d.patternCCChannel; g.patternCCNumber = d.patternCCNumber; g.quantize = d.quantize;
        });
        Theme::setLight (false);       // the theme default: dark (all instances)
        userSettings::setMiddleC (3);  // middle C = C3 (all instances)
        ctx.changed();
    };

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
    return getLocalBounds().withSizeKeepingCentre (600, 604);   // sized to its content
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
    closeBtn->setBounds (panel.getRight() - 20 - 32, panel.getY() + 11, 32, 32);
    auto footer = panel.withTop (panel.getBottom() - 60).reduced (20, 13);
    doneBtn->setBounds (footer.removeFromRight (90));
    resetBtn->setBounds (footer.removeFromLeft (140));

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

    section ("PITCH");
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
        if (r.section) draw::label (g, r.text, r.area, t.muted.withMultipliedAlpha (a));
        else
        {
            g.setColour (t.text.withMultipliedAlpha (a));
            g.setFont (Fonts::sans (13.0f));
            g.drawText (r.text, r.area, juce::Justification::centredLeft);
        }
    }

    // segment frames
    for (auto* group : { &themeBtns, &midCBtns, &quantBtns })
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
