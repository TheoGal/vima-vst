#pragma once

// Reusable controls: arc knobs, pill buttons, popovers and drawing helpers.

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>

namespace vima
{
namespace draw
{
    // 270 degree arc (from -135 to +135 degrees, 0 = up), like the mockups' flat knobs
    void arc (juce::Graphics&, juce::Point<float> centre, float radius, float thickness, float fraction, juce::Colour);
    void tile (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, juce::Colour border, float radius = 6.0f, float borderWidth = 1.0f);
    void label (juce::Graphics&, const juce::String&, juce::Rectangle<int>, juce::Colour, float size = 10.0f,
                juce::Justification = juce::Justification::centredLeft);
    void led (juce::Graphics&, juce::Point<float> centre, float diameter, juce::Colour, bool glow);
    void chevronDown (juce::Graphics&, juce::Point<float> centre, juce::Colour, float size = 9.0f);
}

// ---------------------------------------------------------------------------------------
// Knob: drag up/down, mouse wheel, Shift for fine, double-click to type, right-click or
// Alt+click to reset. Works on integer values, either absolute (get/set) or incremental (nudge).
class Knob : public juce::Component
{
public:
    Knob();

    std::function<int()> getValue;
    std::function<void (int)> setValue;          // absolute mode
    std::function<void (int)> nudge;             // incremental mode (used when set)
    std::function<juce::String (int)> format;    // text in the middle
    std::function<int (const juce::String&)> parse;   // typed text -> value (default: integer)
    std::function<juce::Colour()> colour;
    std::function<float()> fraction;             // arc fill (default from min/max)
    std::function<void()> onGestureStart, onGestureEnd;

    int minValue = 0, maxValue = 127, defaultValue = 0;
    float thickness = 3.0f;
    float fontSize = 11.0f;
    bool semiboldText = true;
    juce::Colour textColour;   // transparent = theme text
    float pixelsPerStep = 5.0f;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void apply (int target);
    void startTyping();
    int dragStartY = 0, dragStartValue = 0, appliedSteps = 0;
    bool dragging = false;
    std::unique_ptr<juce::TextEditor> typing;
};

// ---------------------------------------------------------------------------------------
// Pill: a clickable rounded box with text. All looks are computed on paint, so it follows
// state and theme changes automatically.
class Pill : public juce::Component
{
public:
    struct Look
    {
        juce::Colour fill, border, text;
        float borderWidth = 1.0f;
        bool dashed = false;
    };

    Pill() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    std::function<Look()> look;
    std::function<juce::String()> text;
    std::function<juce::Font()> font;
    std::function<juce::Colour()> ledColour;    // optional LED dot left of the text
    std::function<bool()> ledGlow;
    bool ledOnRight = false;
    std::function<bool()> cornerDot;            // small marker dot in the top right corner
    std::function<void()> onClick;
    std::function<void()> onPress, onRelease;   // for press-and-hold buttons
    std::function<void (int)> onWheel;          // +1 / -1 per wheel step
    std::function<float()> alpha;               // optional opacity
    float radius = 5.0f;
    juce::Justification justification = juce::Justification::centred;
    int textInset = 0;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Standard looks
    static Look filled (juce::Colour accent, juce::Colour text);
    static Look outline (juce::Colour border, juce::Colour text, float width = 1.0f);
    static Look tinted (juce::Colour accent);
    static Look toggle (bool on, juce::Colour accent);   // ON = filled, OFF = outline only
};

// ---------------------------------------------------------------------------------------
// Popover: shows a content component above everything, closes on a click outside it.
class Popover : public juce::Component
{
public:
    static void show (juce::Component& root, std::unique_ptr<juce::Component> content, juce::Rectangle<int> bounds,
                      std::function<void()> onClose = {});
    static void closeAll (juce::Component& root);
    static bool isOpen (juce::Component& root, const juce::String& id);

    void mouseDown (const juce::MouseEvent&) override;
    void resized() override {}

private:
    std::unique_ptr<juce::Component> content;
    std::function<void()> onClose;
};

// Panel background for popovers and dialogs
void paintPanel (juce::Graphics&, juce::Rectangle<float>, float radius = 8.0f);
} // namespace vima
