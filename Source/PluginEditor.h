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

namespace vima
{
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void updateColours();
    juce::Font getPopupMenuFont() override { return Fonts::monoSemi (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
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
    void refreshAll();

private:
    void timerCallback() override;
    void setPatternsView (bool);
    void openSettings();
    void closeSettings();

    VimaProcessor& proc;
    vima::Context ctx;
    vima::LookAndFeel lnf;
    vima::Header header;
    vima::ParamRow paramRow;
    vima::StepGrid grid;
    vima::OpsPanel ops;
    vima::PatternsView patterns;
    std::unique_ptr<vima::SettingsDialog> settings;

    int lastRevision = -1, lastPattern = -1, lastPlayStep = -2, blinkCounter = 0, lastChainPos = -2;
    bool lastPlaying = false, lastQueued = false;
    double lastBpm = 0.0;
    float lastProgress = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaEditor)
};
