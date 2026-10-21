#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VimaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VimaEditor (VimaProcessor&);
    ~VimaEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Rectangle<int> rowBounds (int row) const;
    juce::Rectangle<int> toggleBounds (int row, int which) const;   // 0 = notes, 1 = cc

    VimaProcessor& proc;
    juce::Typeface::Ptr sans, sansSemi, mono, monoSemi;

    std::array<int, VimaProcessor::numTestChannels> lastCount {};
    std::array<float, VimaProcessor::numTestChannels> flash {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaEditor)
};
