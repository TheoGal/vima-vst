#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Context.h"
#include "Header.h"
#include "ParamRow.h"
#include "StepGrid.h"
#include "OpsPanel.h"
#include "PatternsView.h"
#include "SettingsDialog.h"
#include "MacrosView.h"

namespace vima
{
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void updateColours();
    juce::Font getPopupMenuFont() override { return Fonts::monoSemi (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    // The info box (UNDO): themed, under the mouse
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
};

// While LEARN is on, covers the step and patterns views: a click on a control maps the
// selected macro to it instead of changing the control (SPEC.md 13.1).
// Mapped controls keep an outline and a macro tag while LEARN is on; a mapped control cannot be
// mapped again (right-click frees it).
class LearnOverlay : public juce::Component, private juce::Timer
{
public:
    LearnOverlay (Context& c, juce::Component& contentRoot) : ctx (c), content (contentRoot) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    struct Hit { juce::Component* component = nullptr; MacroTarget target = MacroTarget::none; int step = 0; };
    Hit hitAt (juce::Point<int> localPoint);
    Hit resolve (juce::Component*);   // the learnable control a component belongs to
    std::vector<std::pair<Hit, int>> mappedControls();   // visible mapped controls and their macro

private:
    void timerCallback() override { stopTimer(); flash = {}; repaint(); }
    void collect (juce::Component&, std::vector<Hit>&);
    Context& ctx;
    juce::Component& content;
    juce::Rectangle<int> hover, flash;
};
}

class VimaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VimaEditor (VimaProcessor&);
    ~VimaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For tests and screenshots
    vima::Context& context() { return ctx; }
    juce::Component& contentComponent() { return content; }
    void refreshAll();

private:
    void timerCallback() override;
    void setPatternsView (bool);
    void setMacrosView (bool);
    void openSettings();
    void closeSettings();

    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> paintBackground;
        void paint (juce::Graphics& g) override { if (paintBackground) paintBackground (g); }
    };

    void layoutContent();
    static constexpr float minScale = 0.75f, maxScale = 2.0f;
    bool initialised = false;

    VimaProcessor& proc;
    Content content;
    vima::Context ctx;
    vima::LookAndFeel lnf;
    vima::Header header;
    vima::ParamRow paramRow;
    vima::StepGrid grid;
    vima::OpsPanel ops;
    vima::PatternsView patterns;
    vima::MacrosView macrosView;
    vima::LearnOverlay learnOverlay;
    juce::TooltipWindow tooltips { &content, 250 };   // the UNDO info box, after a very short delay
    std::unique_ptr<vima::SettingsDialog> settings;

    int lastTheme = -1, lastRevision = -1, lastPattern = -1, lastPlayStep = -2, blinkCounter = 0, lastChainPos = -2;
    bool lastPlaying = false, lastQueued = false;
    double lastBpm = 0.0;
    float lastProgress = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaEditor)
};
