#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VimaEditor : public juce::AudioProcessorEditor
{
public:
    explicit VimaEditor (VimaProcessor&);
    void paint (juce::Graphics&) override;

private:
    VimaProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimaEditor)
};
