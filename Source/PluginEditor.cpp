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
    ctx.root = this;
    ctx.refresh = [this] { refreshAll(); };
    ctx.showPatterns = [this] (bool b) { setPatternsView (b); };
    ctx.openSettings = [this] { openSettings(); };
    ctx.ui.selectedPattern = ctx.currentPattern();
    ctx.ui.viewBank = ctx.ui.selectedPattern / patternsPerBank;

    Theme::setLight (ctx.globalsCopy().lightTheme);
    lnf.updateColours();
    setLookAndFeel (&lnf);

    addAndMakeVisible (header);
    addAndMakeVisible (paramRow);
    addAndMakeVisible (grid);
    addAndMakeVisible (ops);
    addChildComponent (patterns);

    setSize (editorWidth, editorHeight);
    refreshAll();
    startTimerHz (30);
}

VimaEditor::~VimaEditor()
{
    stopTimer();
    for (int i = getNumChildComponents(); --i >= 0;)
        if (auto* c = getChildComponent (i); c->getComponentID() == "popover")
        {
            removeChildComponent (c);
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
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (headerHeight));
    patterns.setBounds (r);
    auto body = r.reduced (20, 12);
    paramRow.setBounds (body.removeFromTop (48));
    body.removeFromTop (8);
    grid.setBounds (body.removeFromTop (2 * (StepGrid::ledHeight + StepGrid::ledGap + StepGrid::cellHeight) + StepGrid::rowGap));
    ops.setBounds (body.removeFromBottom (146));
    if (settings) settings->setBounds (getLocalBounds());
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
    Popover::closeAll (*this);
    refreshAll();
}

void VimaEditor::openSettings()
{
    settings = std::make_unique<SettingsDialog> (ctx);
    settings->onClose = [this] { closeSettings(); };
    addAndMakeVisible (*settings);
    settings->setBounds (getLocalBounds());
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
            ctx.ui.selFirst = ctx.ui.selLast = -1;
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
