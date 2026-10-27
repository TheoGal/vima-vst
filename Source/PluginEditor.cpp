#include "PluginEditor.h"

namespace vima
{
void LookAndFeel::updateColours()
{
    const auto& t = Theme::current();
    setColour (juce::PopupMenu::backgroundColourId, t.panel);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.selected);
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);
    setColour (juce::TextEditor::backgroundColourId, t.tile);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::CaretComponent::caretColourId, t.text);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& t = Theme::current();
    g.fillAll (t.panel);
    g.setColour (t.border3);
    g.drawRect (0, 0, width, height, 1);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::GlyphArrangement::getStringWidthInt (Fonts::monoSemi (11.0f), text) + 24;
    const int h = 26;
    return juce::Rectangle<int> (screenPos.x - w / 2, screenPos.y + 22, w, h).constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& t = Theme::current();
    const auto b = juce::Rectangle<float> ((float) width, (float) height);
    g.fillAll (t.panel);   // the tooltip window is opaque: square corners
    g.setColour (t.border3);
    g.drawRect (b, 1.0f);
    g.setColour (t.text);
    g.setFont (Fonts::monoSemi (11.0f));
    g.drawText (text, b.toNearestInt(), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------------------
LearnOverlay::Hit LearnOverlay::resolve (juce::Component* c)
{
    Hit hit;
    for (; c != nullptr && c != &content; c = c->getParentComponent())
    {
        const auto& props = c->getProperties();
        if (hit.component == nullptr && props.contains ("learnTarget"))
        {
            hit.component = c;
            const int v = props["learnTarget"];
            if (v >= 0) hit.target = (MacroTarget) v;
            else hit.target = ctx.ui.slider == SliderParam::velocity ? MacroTarget::velocity
                            : ctx.ui.slider == SliderParam::gate     ? MacroTarget::gate
                                                                    : MacroTarget::probability;
        }
        if (hit.component != nullptr && props.contains ("learnStep"))
        {
            hit.step = props["learnStep"];
            break;
        }
    }
    return hit;
}

LearnOverlay::Hit LearnOverlay::hitAt (juce::Point<int> local)
{
    setInterceptsMouseClicks (false, false);
    auto* c = content.getComponentAt (content.getLocalPoint (this, local));
    setInterceptsMouseClicks (true, false);
    return resolve (c);
}

void LearnOverlay::collect (juce::Component& parent, std::vector<Hit>& out)
{
    for (auto* c : parent.getChildren())
    {
        if (c == this || ! c->isVisible())
            continue;
        if (c->getProperties().contains ("learnTarget"))
        {
            const auto h = resolve (c);
            if (h.component != nullptr) out.push_back (h);
            continue;   // a learnable control's children belong to it
        }
        collect (*c, out);
    }
}

std::vector<std::pair<LearnOverlay::Hit, int>> LearnOverlay::mappedControls()
{
    std::vector<Hit> all;
    collect (content, all);
    std::vector<std::pair<Hit, int>> mapped;
    for (const auto& h : all)
        if (const int i = ctx.macroFor (h.target, h.step); i >= 0)
            mapped.push_back ({ h, i });
    return mapped;
}

void LearnOverlay::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    g.setColour (t.amber.withAlpha (0.6f));
    g.drawRect (getLocalBounds(), 2);

    // every mapped control keeps its outline, with the macro's number
    for (const auto& [hit, macro] : mappedControls())
    {
        const auto r = getLocalArea (hit.component->getParentComponent(), hit.component->getBounds()).toFloat().expanded (3.0f);
        g.setColour (t.amber.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (t.amber);
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        const auto tagText = macroId (macro);
        g.setFont (Fonts::monoSemi (9.0f));
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), tagText) + 8.0f;
        const auto tag = juce::Rectangle<float> (w, 13.0f).withPosition (r.getRight() - w - 2.0f, r.getY() - 6.0f);
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (t.onAccent);
        g.drawText (tagText, tag, juce::Justification::centred);
    }
    if (! hover.isEmpty())
    {
        g.setColour (t.amber.withAlpha (0.12f));
        g.fillRoundedRectangle (hover.toFloat().expanded (3.0f), 6.0f);
        g.setColour (t.amber);
        g.drawRoundedRectangle (hover.toFloat().expanded (3.0f), 6.0f, 1.5f);
    }
    if (! flash.isEmpty())   // a click on a mapped control: refused
    {
        g.setColour (t.amber.withAlpha (0.45f));
        g.fillRoundedRectangle (flash.toFloat().expanded (3.0f), 6.0f);
    }
}

void LearnOverlay::mouseMove (const juce::MouseEvent& e)
{
    const auto hit = hitAt (e.getPosition());
    const auto r = hit.component != nullptr ? getLocalArea (hit.component->getParentComponent(), hit.component->getBounds()) : juce::Rectangle<int>();
    if (r != hover) { hover = r; repaint(); }
    setMouseCursor (hit.component != nullptr ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void LearnOverlay::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

void LearnOverlay::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = hitAt (e.getPosition());
    if (hit.component == nullptr)
        return;
    if (e.mods.isPopupMenu())
    {
        ctx.unlearn (hit.target, hit.step);   // right-click frees a mapped control
        return;
    }
    if (ctx.macroFor (hit.target, hit.step) >= 0)
    {
        flash = getLocalArea (hit.component->getParentComponent(), hit.component->getBounds());
        repaint();
        startTimer (250);
        return;
    }
    ctx.learn (hit.target, hit.step);
}
}

using namespace vima;

static constexpr int editorWidth = 1200, editorHeight = 720, headerHeight = 52;

VimaEditor::VimaEditor (VimaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ctx (p),
      header (ctx), paramRow (ctx), grid (ctx), ops (ctx), patterns (ctx), macrosView (ctx), learnOverlay (ctx, content)
{
    ctx.root = &content;
    ctx.refresh = [this] { refreshAll(); };
    ctx.showPatterns = [this] (bool b) { setPatternsView (b); };
    ctx.openSettings = [this] { openSettings(); };
    ctx.showMacros = [this] (bool b) { setMacrosView (b); };
    ctx.ui.track = juce::jlimit (0, numTracks - 1, proc.uiTrack);
    ctx.ui.macroPage = 0;
    ctx.ui.selectedPattern = ctx.currentPattern();
    ctx.ui.viewPage = ctx.ui.selectedPattern / patternsPerPage;

    lnf.updateColours();
    lastTheme = Theme::revision();
    setLookAndFeel (&lnf);

    // Everything is laid out at 1200 x 720 inside `content`, which is scaled as a whole
    // when the window is resized (SPEC.md 12.1: resizing)
    content.paintBackground = [this] (juce::Graphics& g)
    {
        const auto& t = Theme::current();
        g.fillAll (t.bg);
        if (ctx.ui.patternsView || ctx.ui.macrosView)
            return;
        // Step view: the header and the parameter row form one band. The TRACK box and the
        // PATTERN box each sit in a rounded frame that flows down around their tiles (SPEC.md 12.2).
        const auto rowPos = paramRow.getPosition();
        auto frame = [&g, &t] (juce::Rectangle<int> top, juce::Rectangle<int> bottom)
        {
            const float pad = 6.0f;
            auto a = top.toFloat().expanded (pad);
            auto b = bottom.toFloat().expanded (pad);
            const float left = juce::jmin (a.getX(), b.getX());
            float ar = a.getRight(), br = b.getRight();
            if (std::abs (ar - br) < 24.0f) ar = br = juce::jmax (ar, br);   // no tiny steps
            const float mid = (a.getBottom() + b.getY()) * 0.5f;
            juce::Path p;
            p.startNewSubPath (left, a.getY());
            p.lineTo (ar, a.getY());
            p.lineTo (ar, mid);
            p.lineTo (br, mid);
            p.lineTo (br, b.getBottom());
            p.lineTo (left, b.getBottom());
            p.closeSubPath();
            p = p.createPathWithRoundedCorners (10.0f);
            g.setColour (t.panel);
            g.fillPath (p);
            g.setColour (t.border);
            g.strokePath (p, juce::PathStrokeType (1.0f));
        };
        // the wordmark, larger, in the column left of the TRACK frame
        const int frameLeft = header.trackGroupTop().getX() - 6;
        vima::Header::drawWordmark (g, juce::Rectangle<float> (20.0f, 0.0f, (float) (frameLeft - 20 - 14), (float) paramRow.getBottom())
                                           .withSizeKeepingCentre ((float) (frameLeft - 20 - 14), 40.0f));
        frame (header.trackGroupTop(), paramRow.trackTiles + rowPos);
        frame (header.patternGroupTop(), paramRow.patternTiles + rowPos);
    };
    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (paramRow);
    content.addAndMakeVisible (grid);
    content.addAndMakeVisible (ops);
    content.addChildComponent (patterns);
    content.addChildComponent (macrosView);
    content.addChildComponent (learnOverlay);
    content.setBounds (0, 0, editorWidth, editorHeight);
    layoutContent();

    // Read the saved size first: setting the limits can resize the window on the way
    const float scale = juce::jlimit (minScale, maxScale, proc.uiScale.load());
    setResizable (true, true);
    setResizeLimits ((int) (editorWidth * minScale), (int) (editorHeight * minScale),
                     (int) (editorWidth * maxScale), (int) (editorHeight * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) editorWidth / (double) editorHeight);
    setSize (juce::roundToInt (editorWidth * scale), juce::roundToInt (editorHeight * scale));
    initialised = true;
    refreshAll();
    startTimerHz (30);
}

VimaEditor::~VimaEditor()
{
    stopTimer();
    for (int i = content.getNumChildComponents(); --i >= 0;)
        if (auto* c = content.getChildComponent (i); c->getComponentID() == "popover")
        {
            content.removeChildComponent (c);
            delete c;
        }
    settings.reset();
    setLookAndFeel (nullptr);
}

void VimaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::current().bg);
}

void VimaEditor::resized()
{
    const float scale = juce::jlimit (minScale, maxScale, (float) getWidth() / (float) editorWidth);
    content.setTransform (juce::AffineTransform::scale (scale));
    if (initialised)
        proc.uiScale = scale;   // remembered with the project
}

void VimaEditor::layoutContent()
{
    auto r = content.getLocalBounds();
    header.setBounds (r.removeFromTop (headerHeight));
    patterns.setBounds (r);
    macrosView.setBounds (r);
    learnOverlay.setBounds (r);
    auto body = r.reduced (20, 0).withTrimmedBottom (12);
    paramRow.setBounds (body.removeFromTop (48 + 2 * vima::ParamRow::tileInset));
    body.removeFromTop (14);
    grid.setBounds (body.removeFromTop (2 * (StepGrid::ledHeight + StepGrid::ledGap + StepGrid::cellHeight) + StepGrid::rowGap));
    ops.setBounds (body.removeFromBottom (146));
    if (settings) settings->setBounds (content.getLocalBounds());
}

void VimaEditor::refreshAll()
{
    if (Theme::revision() != lastTheme)
    {
        lastTheme = Theme::revision();
        lnf.updateColours();
    }
    const bool mv = ctx.ui.macrosView;
    const bool pv = ctx.ui.patternsView && ! mv;
    paramRow.setVisible (! pv && ! mv);
    grid.setVisible (! pv && ! mv);
    ops.setVisible (! pv && ! mv);
    patterns.setVisible (pv);
    macrosView.setVisible (mv);
    learnOverlay.setVisible (ctx.ui.macroLearn && ! mv);
    if (learnOverlay.isVisible()) learnOverlay.toFront (false);
    proc.uiTrack = ctx.ui.track;
    header.resized();
    paramRow.resized();
    grid.refreshCells();
    ops.refreshState();
    patterns.refreshState();
    if (mv) macrosView.refreshState();
    learnOverlay.repaint();
    content.repaint();
    lastRevision = proc.revision.load();
    lastPattern = ctx.currentPattern();
    repaint();
    if (settings) settings->repaint();
}

void VimaEditor::setPatternsView (bool show)
{
    ctx.ui.patternsView = show;
    if (show) ctx.ui.macrosView = false;
    if (show)
    {
        ctx.ui.selectedPattern = ctx.currentPattern();
        ctx.ui.viewPage = ctx.ui.selectedPattern / patternsPerPage;
    }
    Popover::closeAll (content);
    refreshAll();
}

void VimaEditor::setMacrosView (bool show)
{
    ctx.ui.macrosView = show;
    if (show)
    {
        ctx.ui.patternsView = false;
        ctx.ui.macroPage = ctx.ui.selectedMacro / macrosPerPage;
    }
    Popover::closeAll (content);
    refreshAll();
}

void VimaEditor::openSettings()
{
    settings = std::make_unique<SettingsDialog> (ctx);
    settings->onClose = [this] { closeSettings(); };
    content.addAndMakeVisible (*settings);
    settings->setBounds (content.getLocalBounds());
    settings->toFront (true);
}

void VimaEditor::closeSettings()
{
    juce::Component::SafePointer<VimaEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr) { safe->settings.reset(); safe->refreshAll(); }
    });
}

void VimaEditor::timerCallback()
{
    if (++blinkCounter >= 8)
    {
        blinkCounter = 0;
        ctx.ui.blink = ! ctx.ui.blink;
    }

    const int rev = proc.revision.load();
    const int pat = ctx.currentPattern();
    if (rev != lastRevision || pat != lastPattern || Theme::revision() != lastTheme)
    {
        if (pat != lastPattern && ! ctx.ui.patternsView)
            ctx.ui.selection = 0;
        refreshAll();
    }

    const bool queued = proc.engine.queuedPattern.load() >= 0;
    const bool playing = proc.engine.isPlaying;
    const double bpm = proc.hostBpm.load();
    if (queued || queued != lastQueued || playing != lastPlaying || ! juce::exactlyEqual (bpm, lastBpm))
        header.repaint();
    lastQueued = queued;
    lastBpm = bpm;

    const int step = playing ? proc.engine.playStep[(size_t) ctx.ui.track].load() : -1;
    if (step != lastPlayStep || playing != lastPlaying)
    {
        lastPlayStep = step;
        if (grid.isVisible()) grid.repaintLeds();
    }
    lastPlaying = playing;

    if (patterns.isVisible())
    {
        const int cp = proc.engine.chainPosition.load();
        const float pr = proc.engine.chainEntryProgress.load();
        if (cp != lastChainPos || std::abs (pr - lastProgress) > 0.004f || queued)
        {
            lastChainPos = cp;
            lastProgress = pr;
            patterns.repaint();
        }
    }
}
