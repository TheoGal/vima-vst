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
    juce::Colour knobTrack;   // inactive part of knob arcs
    juce::Colour ledIdle;     // LED bar: unlit step within the length
    juce::Colour ledBeyond;   // LED bar: step beyond the length

    static const Theme& dark();
    static const Theme& light();
    static const Theme& current();

    // The theme is a user-wide preference (SPEC.md 11): stored once in the app settings folder and shared
    // by every VIMA instance. setLight saves it; revision() changes on every switch, so open
    // editors can follow.
    static void setLight (bool);
    static bool isLight();
    static int revision();
    static void setSettingsFile (const juce::File&);   // tests: use another file
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
