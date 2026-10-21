#include "PluginEditor.h"

VimaEditor::VimaEditor (VimaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (1200, 720);
}

void VimaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1114));
}
