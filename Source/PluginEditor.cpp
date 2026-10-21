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
}

using namespace vima;

static constexpr int editorWidth = 1200, editorHeight = 720, headerHeight = 52;

VimaEditor::VimaEditor (VimaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ctx (p),
      header (ctx), paramRow (ctx), grid (ctx), ops (ctx), patterns (ctx)
{
    ctx.root = &content;
    ctx.refresh = [this] { refreshAll(); };
    ctx.showPatterns = [this] (bool b) { setPatternsView (b); };
    ctx.openSettings = [this] { openSettings(); };
    ctx.ui.selectedPattern = ctx.currentPattern();
    ctx.ui.viewBank = ctx.ui.selectedPattern / patternsPerBank;

    Theme::setLight (ctx.globalsCopy().lightTheme);
    lnf.updateColours();
    setLookAndFeel (&lnf);

    // Everything is laid out at 1200 x 720 inside `content`, which is scaled as a whole
    // when the window is resized (SPEC.md 12: resizing)
    content.paintBackground = [] (juce::Graphics& g) { g.fillAll (Theme::current().bg); };
    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (paramRow);
    content.addAndMakeVisible (grid);
    content.addAndMakeVisible (ops);
    content.addChildComponent (patterns);
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
    auto body = r.reduced (20, 12);
    paramRow.setBounds (body.removeFromTop (48));
    body.removeFromTop (8);
    grid.setBounds (body.removeFromTop (2 * (StepGrid::ledHeight + StepGrid::ledGap + StepGrid::cellHeight) + StepGrid::rowGap));
    ops.setBounds (body.removeFromBottom (146));
    if (settings) settings->setBounds (content.getLocalBounds());
}

void VimaEditor::refreshAll()
{
    const auto g = ctx.globalsCopy();
    if (Theme::isLight() != g.lightTheme)
    {
        Theme::setLight (g.lightTheme);
        lnf.updateColours();
    }
    const bool pv = ctx.ui.patternsView;
    paramRow.setVisible (! pv);
    grid.setVisible (! pv);
    ops.setVisible (! pv);
    patterns.setVisible (pv);
    header.resized();
    paramRow.resized();
    grid.refreshCells();
    ops.refreshState();
    patterns.refreshState();
    lastRevision = proc.revision.load();
    lastPattern = ctx.currentPattern();
    repaint();
    if (settings) settings->repaint();
}

void VimaEditor::setPatternsView (bool show)
{
    ctx.ui.patternsView = show;
    if (show)
    {
        ctx.ui.selectedPattern = ctx.currentPattern();
        ctx.ui.viewBank = ctx.ui.selectedPattern / patternsPerBank;
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
    if (rev != lastRevision || pat != lastPattern)
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
