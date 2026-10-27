#include "Popovers.h"

namespace vima
{
Pill& PopoverPanel::addPill()
{
    auto* p = pills.add (new Pill());
    addAndMakeVisible (p);
    return *p;
}

void PopoverPanel::paintOverChildren (juce::Graphics& g)
{
    for (const auto& l : labels)
        draw::label (g, l.text, l.area, Theme::current().muted);
}

static juce::Rectangle<int> cell (juce::Rectangle<int> area, int cols, int index, int h, int gap)
{
    const int w = (area.getWidth() - gap * (cols - 1)) / cols;
    const int col = index % cols, row = index / cols;
    return { area.getX() + col * (w + gap), area.getY() + row * (h + gap), w, h };
}

static Pill::Look choiceLook (bool on, juce::Colour accent)
{
    const auto& t = Theme::current();
    return on ? Pill::Look { accent, accent, accent == t.neutral ? t.onNeutral : t.onAccent, 1.0f, false }
              : Pill::outline (t.border2, t.text2);
}

// ---------------------------------------------------------------------------------------
DivisionPopover::DivisionPopover (Context& c) : ctx (c)
{
    setComponentID ("division");
    for (int i = 0; i < numDivisions; ++i)
    {
        auto& p = addPill();
        p.radius = 4.0f;
        p.text = [i] { return juce::String (divisionInfo (i).name); };
        p.look = [this, i] { return choiceLook (ctx.trackCopy().division == i, Theme::current().neutral); };
        p.onClick = [this, i]
        {
            ctx.edit (true, [this, i] (Project& pr) { ctx.liveTrack (pr).division = (std::uint8_t) i; }, "DIVISION" + ctx.where());
            Popover::closeAll (*ctx.root);
        };
    }
}

void DivisionPopover::resized()
{
    auto area = getLocalBounds().reduced (10);
    labels.clear();
    labels.add ({ "STRAIGHT", area.removeFromTop (12) });
    area.removeFromTop (8);
    auto row1 = area.removeFromTop (30);
    for (int i = 0; i < 6; ++i) pills[i]->setBounds (cell (row1, 6, i, 30, 4));
    area.removeFromTop (8);
    labels.add ({ "TRIPLET", area.removeFromTop (12) });
    area.removeFromTop (8);
    auto row2 = area.removeFromTop (30);
    for (int i = 6; i < numDivisions; ++i) pills[i]->setBounds (cell (row2, 6, i - 6, 30, 4));
}

// ---------------------------------------------------------------------------------------
ScalePopover::ScalePopover (Context& c) : ctx (c)
{
    setComponentID ("scale");
    for (int i = 0; i < 12; ++i)
    {
        auto& p = addPill();
        p.radius = 4.0f;
        p.text = [i] { return juce::String (pitchClassName (i)); };
        p.look = [this, i] { return choiceLook (ctx.globalsCopy().root == i, Theme::current().green); };
        p.onClick = [this, i] { ctx.edit (false, [i] (Project& pr) { pr.globals.root = i; }); };
    }
    for (int i = 0; i < numScales; ++i)
    {
        auto& p = addPill();
        p.radius = 4.0f;
        p.justification = juce::Justification::centredLeft;
        p.textInset = 7;
        p.font = [] { return Fonts::monoSemi (10.0f); };
        p.text = [i] { return juce::String (scaleInfo (i).longName); };
        p.look = [this, i] { return choiceLook (ctx.globalsCopy().scale == i, Theme::current().green); };
        p.onClick = [this, i]
        {
            ctx.edit (false, [i] (Project& pr) { pr.globals.scale = i; });
            Popover::closeAll (*ctx.root);
        };
    }
}

void ScalePopover::resized()
{
    auto area = getLocalBounds().reduced (10);
    labels.clear();
    labels.add ({ "ROOT", area.removeFromTop (14) });
    area.removeFromTop (8);
    auto roots = area.removeFromTop (56);
    for (int i = 0; i < 12; ++i) pills[i]->setBounds (cell (roots, 6, i, 26, 4));
    area.removeFromTop (8);
    labels.add ({ "SCALE", area.removeFromTop (12) });
    area.removeFromTop (8);
    for (int i = 0; i < numScales; ++i) pills[12 + i]->setBounds (cell (area, 3, i, 26, 4));
}

void ScalePopover::paintOverChildren (juce::Graphics& g)
{
    PopoverPanel::paintOverChildren (g);
    const auto& t = Theme::current();
    auto tag = juce::Rectangle<int> (getWidth() - 10 - 52, 10, 52, 16);
    g.setColour (t.border2);
    g.drawRoundedRectangle (tag.toFloat().reduced (0.5f), 3.0f, 1.0f);
    g.setColour (t.muted);
    g.setFont (Fonts::label (9.0f));
    g.drawText ("GLOBAL", tag, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------
PatternPickerPopover::PatternPickerPopover (Context& c) : ctx (c)
{
    setComponentID ("patternPicker");
    for (int b = 0; b < numPages; ++b)
    {
        auto& p = addPill();
        p.radius = 4.0f;
        p.text = [b] { return pageRangeText (b); };
        p.look = [this, b] { return choiceLook (ctx.ui.pickerPage == b, Theme::current().neutral); };
        p.onClick = [this, b] { ctx.ui.pickerPage = b; repaint(); };
        p.cornerDot = [this, b]
        {
            const int q = ctx.proc.engine.queuedPattern.load();
            return ctx.currentPattern() / patternsPerPage == b || (q >= 0 && q / patternsPerPage == b);
        };
    }
    for (int s = 0; s < patternsPerPage; ++s)
    {
        auto& p = addPill();
        p.radius = 5.0f;
        p.justification = juce::Justification::centredLeft;
        p.textInset = 9;
        p.font = [] { return Fonts::monoSemi (12.0f); };
        p.text = [this, s] { return patternLabel (ctx.ui.pickerPage * patternsPerPage + s); };
        p.look = [this, s]
        {
            const auto& t = Theme::current();
            const int idx = ctx.ui.pickerPage * patternsPerPage + s;
            const int cur = ctx.currentPattern();
            const int q = ctx.proc.engine.queuedPattern.load();
            const bool used = ! ctx.proc.read ([idx] (const Project& pr) { return pr.patterns[(size_t) idx].isEmpty(); });
            Pill::Look l { used ? t.tile : t.panel, t.border2, used ? t.text : t.muted, 1.0f, false };
            if (idx == cur) { l.border = t.neutral; l.borderWidth = 2.0f; }
            if (idx == q && idx != cur) { l.border = t.neutral; l.borderWidth = 2.0f; l.dashed = true; }
            return l;
        };
        p.ledOnRight = true;
        p.ledColour = [this, s]
        {
            const int idx = ctx.ui.pickerPage * patternsPerPage + s;
            return idx == ctx.currentPattern() ? Theme::current().neutral : juce::Colours::transparentBlack;
        };
        p.ledGlow = [] { return true; };
        p.onClick = [this, s]
        {
            const int idx = ctx.ui.pickerPage * patternsPerPage + s;
            if (idx != ctx.currentPattern())
                ctx.proc.queuePattern (idx);
            Popover::closeAll (*ctx.root);
            ctx.changed();
        };
    }
    auto& open = addPill();
    open.text = [] { return juce::String ("OPEN PATTERNS VIEW"); };
    open.font = [] { return Fonts::sansBold (11.0f).withExtraKerningFactor (0.12f); };
    open.look = [] { const auto& t = Theme::current(); return Pill::outline (t.neutral, t.text); };
    open.onClick = [this]
    {
        Popover::closeAll (*ctx.root);
        if (ctx.showPatterns) ctx.showPatterns (true);
    };
    startTimerHz (8);
}

void PatternPickerPopover::paintOverChildren (juce::Graphics& g)
{
    labels.getReference (1).text = "PATTERNS " + pageRangeText (ctx.ui.pickerPage);
    PopoverPanel::paintOverChildren (g);
}

void PatternPickerPopover::resized()
{
    auto area = getLocalBounds().reduced (10);
    labels.clear();
    labels.add ({ "PAGE", area.removeFromTop (12) });
    area.removeFromTop (8);
    auto banks = area.removeFromTop (26);
    for (int i = 0; i < numPages; ++i) pills[i]->setBounds (cell (banks, numPages, i, 26, 4));
    area.removeFromTop (8);
    labels.add ({ "PATTERNS", area.removeFromTop (12) });
    area.removeFromTop (8);
    auto slots = area.removeFromTop (172);
    for (int i = 0; i < patternsPerPage; ++i) pills[numPages + i]->setBounds (cell (slots, 4, i, 40, 4));
    area.removeFromTop (10);
    pills[numPages + patternsPerPage]->setBounds (area.removeFromTop (30));
}
} // namespace vima
