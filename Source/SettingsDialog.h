#pragma once

// Settings dialog (SPEC.md 11, board E).

#include "Context.h"
#include "Widgets.h"

namespace vima
{
class SettingsDialog : public juce::Component
{
public:
    explicit SettingsDialog (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onClose;

private:
    juce::Rectangle<int> panelBounds() const;
    Pill& pill();
    Knob& knob();

    Context& ctx;
    juce::OwnedArray<Pill> pills;
    juce::OwnedArray<Knob> knobs;
    Pill* closeBtn = nullptr;
    Pill* resetBtn = nullptr;
    Pill* doneBtn = nullptr;
    juce::Array<Pill*> themeBtns, midCBtns, rootBtns, userBtns, quantBtns;
    Pill* smoothBtn = nullptr;
    Pill* patternCCBtn = nullptr;
    Knob* timeKnob = nullptr;
    Knob* pcChannel = nullptr;
    Knob* pcNumber = nullptr;

    struct Row { juce::String text; juce::Rectangle<int> area; float alpha = 1.0f; bool section = false; };
    juce::Array<Row> rows;
    juce::Array<int> dividers;
};
} // namespace vima
