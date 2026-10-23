#include "Widgets.h"

namespace vima
{
namespace draw
{
void arc (juce::Graphics& g, juce::Point<float> c, float r, float thickness, float fraction, juce::Colour colour)
{
    fraction = juce::jlimit (0.0f, 1.0f, fraction);
    const float start = -0.75f * juce::MathConstants<float>::pi;
    const float end = start + 1.5f * juce::MathConstants<float>::pi * juce::jmax (0.002f, fraction);
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void tile (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour border, float radius, float borderWidth)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    if (borderWidth > 0.0f)
    {
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (borderWidth * 0.5f), radius, borderWidth);
    }
}

void label (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Colour c, float size, juce::Justification j)
{
    g.setColour (c);
    g.setFont (Fonts::label (size));
    g.drawText (text, r, j, false);
}

void led (juce::Graphics& g, juce::Point<float> c, float d, juce::Colour colour, bool glow)
{
    if (glow)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (d * 2.2f, d * 2.2f).withCentre (c));
    }
    g.setColour (colour);
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
}

void chevronDown (juce::Graphics& g, juce::Point<float> c, juce::Colour colour, float size)
{
    juce::Path p;
    const float h = size * 0.3f, w = size * 0.3f;
    p.startNewSubPath (c.x - w, c.y - h * 0.5f);
    p.lineTo (c.x, c.y + h * 0.5f);
    p.lineTo (c.x + w, c.y - h * 0.5f);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace draw

// ---------------------------------------------------------------------------------------
Knob::Knob()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    auto b = getLocalBounds().toFloat();
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    const float r = size * 0.5f - thickness * 0.5f - 1.0f;
    const auto c = b.getCentre();
    const int v = getValue ? getValue() : 0;

    draw::arc (g, c, r, thickness, 1.0f, t.knobTrack);
    float f = 0.0f;
    if (fraction) f = fraction();
    else if (maxValue > minValue) f = (float) (v - minValue) / (float) (maxValue - minValue);
    draw::arc (g, c, r, thickness, f, colour ? colour() : t.neutral);

    if (typing == nullptr)
    {
        g.setColour (textColour.isTransparent() ? t.text : textColour);
        g.setFont (semiboldText ? Fonts::monoSemi (fontSize) : Fonts::mono (fontSize));
        g.drawText (format ? format (v) : juce::String (v), getLocalBounds(), juce::Justification::centred, false);
    }
}

void Knob::apply (int target)
{
    if (nudge)
    {
        const int delta = target - appliedSteps;
        if (delta != 0) nudge (delta);
        appliedSteps = target;
        return;
    }
    if (setValue)
        setValue (juce::jlimit (minValue, maxValue, target));
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isAltDown())
    {
        if (onGestureStart) onGestureStart();
        if (setValue) setValue (defaultValue);
        if (onGestureEnd) onGestureEnd();
        repaint();
        return;
    }
    dragging = true;
    dragStartY = e.getScreenY();
    dragStartValue = getValue ? getValue() : 0;
    appliedSteps = 0;
    if (onGestureStart) onGestureStart();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float pps = e.mods.isShiftDown() ? pixelsPerStep * 3.0f : pixelsPerStep;
    const int steps = juce::roundToInt ((float) (dragStartY - e.getScreenY()) / pps);
    apply (nudge ? steps : dragStartValue + steps);
    repaint();
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onGestureEnd) onGestureEnd();
    dragging = false;
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (juce::exactlyEqual (d, 0.0f)) return;
    const int dir = (d > 0.0f) != w.isReversed ? 1 : -1;
    if (onGestureStart) onGestureStart();
    appliedSteps = 0;
    apply (nudge ? dir : (getValue ? getValue() : 0) + dir);
    if (onGestureEnd) onGestureEnd();
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! setValue && ! parse) return;
    startTyping();
}

void Knob::startTyping()
{
    const auto& t = Theme::current();
    typing = std::make_unique<juce::TextEditor>();
    typing->setJustification (juce::Justification::centred);
    typing->setFont (Fonts::monoSemi (fontSize));
    typing->setColour (juce::TextEditor::backgroundColourId, t.tile);
    typing->setColour (juce::TextEditor::textColourId, t.text);
    typing->setColour (juce::TextEditor::outlineColourId, t.neutral);
    typing->setColour (juce::TextEditor::focusedOutlineColourId, t.neutral);
    typing->setText (format ? format (getValue ? getValue() : 0) : juce::String (getValue ? getValue() : 0), false);
    typing->selectAll();
    const int h = juce::roundToInt (fontSize * 1.9f);
    typing->setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmax (getWidth(), 44), h));
    addAndMakeVisible (*typing);

    auto finish = [this] (bool commit)
    {
        if (typing == nullptr) return;
        if (commit)
        {
            const auto text = typing->getText().trim();
            const int v = parse ? parse (text) : text.getIntValue();
            if (onGestureStart) onGestureStart();
            if (setValue) setValue (juce::jlimit (minValue, maxValue, v));
            if (onGestureEnd) onGestureEnd();
        }
        auto* ed = typing.release();
        juce::MessageManager::callAsync ([ed] { delete ed; });
        repaint();
    };
    typing->onReturnKey = [finish] { finish (true); };
    typing->onEscapeKey = [finish] { finish (false); };
    typing->onFocusLost = [finish] { finish (false); };
    typing->grabKeyboardFocus();
    repaint();
}

// ---------------------------------------------------------------------------------------
Pill::Look Pill::filled (juce::Colour accent, juce::Colour text) { return { accent, accent, text, 0.0f, false }; }
Pill::Look Pill::outline (juce::Colour border, juce::Colour text, float width) { return { juce::Colours::transparentBlack, border, text, width, false }; }
Pill::Look Pill::tinted (juce::Colour accent) { return { accent.withAlpha (0.16f), accent, accent, 1.0f, false }; }
Pill::Look Pill::toggle (bool on, juce::Colour accent)
{
    const auto& t = Theme::current();
    return on ? Look { accent, accent, t.onAccent, 1.5f, false } : Look { juce::Colours::transparentBlack, t.dim, t.muted, 1.5f, false };
}

void Pill::paint (juce::Graphics& g)
{
    const auto& t = Theme::current();
    const bool enabled = ! alpha || alpha() >= 0.999f;
    const bool pressed = pressedLook && enabled && isMouseButtonDown() && isMouseOver();
    const auto l = pressed ? pressedLook() : (look ? look() : outline (t.border2, t.text2));
    if (alpha) g.setOpacity (alpha());
    auto b = getLocalBounds().toFloat();
    if (! l.fill.isTransparent())
    {
        g.setColour (l.fill);
        g.fillRoundedRectangle (b, radius);
    }
    if (l.borderWidth > 0.0f && ! l.border.isTransparent())
    {
        g.setColour (l.border);
        auto rb = b.reduced (l.borderWidth * 0.5f);
        if (l.dashed)
        {
            juce::Path p;
            p.addRoundedRectangle (rb, radius);
            juce::Path dashedPath;
            const float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType (l.borderWidth).createDashedStroke (dashedPath, p, dashes, 2);
            g.fillPath (dashedPath);
        }
        else
        {
            g.drawRoundedRectangle (rb, radius, l.borderWidth);
        }
    }

    auto textArea = getLocalBounds().withTrimmedLeft (textInset);
    if (justification.testFlags (juce::Justification::horizontallyCentred))
        textArea = textArea.withTrimmedRight (textInset);
    if (ledColour)
    {
        const auto lc = ledColour();
        const float d = 7.0f;
        if (ledOnRight)
        {
            auto centre = juce::Point<float> ((float) getWidth() - 12.0f, b.getCentreY());
            if (! lc.isTransparent()) draw::led (g, centre, 6.0f, lc, ledGlow && ledGlow());
        }
        else
        {
            auto centre = juce::Point<float> ((float) textArea.getX() + 10.0f, b.getCentreY());
            draw::led (g, centre, d, lc, ledGlow && ledGlow());
            textArea.removeFromLeft (18);
        }
    }
    if (cornerDot && cornerDot())
    {
        g.setColour (l.text);
        g.fillEllipse ((float) getWidth() - 7.0f, 3.0f, 4.0f, 4.0f);
    }
    if (text)
    {
        g.setColour (l.text);
        const auto f = font ? font() : Fonts::monoSemi (11.0f);
        g.setFont (f);
        const auto s = text();
        g.drawText (s, textArea, justification, false);
        if (chevron)   // after the text (left-aligned text only)
        {
            const int w = juce::GlyphArrangement::getStringWidthInt (f, s);
            draw::chevronDown (g, { (float) (textArea.getX() + w + 11), b.getCentreY() }, l.text.withMultipliedAlpha (0.8f), 10.0f);
        }
    }
}

void Pill::mouseDown (const juce::MouseEvent& e)
{
    if (pressedLook) repaint();
    if (e.mods.isPopupMenu()) return;
    if (onPress) onPress();
}

void Pill::mouseDrag (const juce::MouseEvent&)
{
    if (pressedLook) repaint();
}

void Pill::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! onWheel)
    {
        Component::mouseWheelMove (e, w);
        return;
    }
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (! juce::exactlyEqual (d, 0.0f))
        onWheel ((d > 0.0f) != w.isReversed ? 1 : -1);
}

void Pill::mouseUp (const juce::MouseEvent& e)
{
    if (pressedLook) repaint();
    if (onRelease) onRelease();
    if (onClick && ! e.mods.isPopupMenu() && getLocalBounds().contains (e.getPosition()))
        onClick();
}

// ---------------------------------------------------------------------------------------
void paintPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    const auto& t = Theme::current();
    juce::DropShadow (t.shadow, 24, { 0, 10 }).drawForRectangle (g, r.toNearestInt());
    draw::tile (g, r, t.panel, t.border3, radius);
}

void Popover::show (juce::Component& root, std::unique_ptr<juce::Component> content, juce::Rectangle<int> bounds,
                    std::function<void()> onClose)
{
    closeAll (root);
    auto* p = new Popover();
    p->setComponentID ("popover");
    p->onClose = std::move (onClose);
    p->setBounds (root.getLocalBounds());
    p->content = std::move (content);
    p->addAndMakeVisible (*p->content);
    p->content->setBounds (bounds);
    root.addAndMakeVisible (p);
    p->toFront (false);
}

void Popover::closeAll (juce::Component& root)
{
    for (int i = root.getNumChildComponents(); --i >= 0;)
    {
        auto* c = root.getChildComponent (i);
        if (c->getComponentID() == "popover")
        {
            auto* p = static_cast<Popover*> (c);
            root.removeChildComponent (p);
            if (p->onClose) p->onClose();
            juce::Component::SafePointer<Popover> safe (p);
            p->setVisible (false);
            juce::MessageManager::callAsync ([safe] { delete safe.getComponent(); });
        }
    }
}

bool Popover::isOpen (juce::Component& root, const juce::String& id)
{
    for (auto* c : root.getChildren())
        if (c->getComponentID() == "popover")
            if (auto* p = static_cast<Popover*> (c); p->content != nullptr && p->content->getComponentID() == id)
                return true;
    return false;
}

void Popover::mouseDown (const juce::MouseEvent&)
{
    if (auto* parent = getParentComponent())
        closeAll (*parent);
}
} // namespace vima
