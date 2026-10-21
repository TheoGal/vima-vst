#pragma once

// Colours and fonts for both themes (SPEC.md section 12, "Look").

#include <juce_gui_basics/juce_gui_basics.h>

namespace vima
{
struct Theme
{
    juce::Colour bg, panel, tile, tileAlt, beyond, sliderTrack, selected, border, border2, border3, ledOff, dim, dim2;
    juce::Colour muted, text2, text;
    juce::Colour neutral;     // track-wide controls
    juce::Colour green;       // note part, velocity
    juce::Colour cyan;        // CC part
    juce::Colour purple;      // gate
    juce::Colour amber;       // probability
    juce::Colour onAccent;    // dark text on accent fills
    juce::Colour onNeutral;   // text on neutral fills
    juce::Colour selection;   // selected LEDs
    juce::Colour shadow;

    static const Theme& dark();
    static const Theme& light();
    static const Theme& current();
    static void setLight (bool);
    static bool isLight();
};

struct Fonts
{
    static juce::Font sans (float size);
    static juce::Font sansSemi (float size);
    static juce::Font sansBold (float size);
    static juce::Font mono (float size);
    static juce::Font monoSemi (float size);
    // Upper case label with letter spacing, like the mockups' small section labels
    static juce::Font label (float size = 10.0f);
};
} // namespace vima
