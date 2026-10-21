#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    namespace col
    {
        const juce::Colour bg       { 0xff0e1114 };
        const juce::Colour panel    { 0xff12161b };
        const juce::Colour tile     { 0xff161a20 };
        const juce::Colour border   { 0xff262c35 };
        const juce::Colour border2  { 0xff2a313b };
        const juce::Colour text     { 0xffeef1f5 };
        const juce::Colour text2    { 0xffc5cbd4 };
        const juce::Colour muted    { 0xff8a94a3 };
        const juce::Colour dim      { 0xff4a525e };
        const juce::Colour green    { 0xff4fd98a };
        const juce::Colour cyan     { 0xff5cc8ff };
        const juce::Colour onText   { 0xff0b1410 };
    }

    constexpr int width = 560;
    constexpr int height = 384;
    constexpr int rowTop = 96;
    constexpr int rowHeight = 48;
    constexpr int rowGap = 8;

    juce::Font font (juce::Typeface::Ptr tf, float size)
    {
        return juce::Font (juce::FontOptions (tf).withHeight (size));
    }

    const char* noteName (int note)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[note % 12];
    }
}

VimaEditor::VimaEditor (VimaProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    sans     = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansRegular_ttf, BinaryData::IBMPlexSansRegular_ttfSize);
    sansSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansSemiBold_ttf, BinaryData::IBMPlexSansSemiBold_ttfSize);
    mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    monoSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);

    for (int i = 0; i < VimaProcessor::numTestChannels; ++i)
        lastCount[(size_t) i] = proc.noteCounter[(size_t) i];

    setSize (width, height);
    startTimerHz (30);
}

juce::Rectangle<int> VimaEditor::rowBounds (int row) const
{
    return { 20, rowTop + row * (rowHeight + rowGap), width - 40, rowHeight };
}

juce::Rectangle<int> VimaEditor::toggleBounds (int row, int which) const
{
    auto r = rowBounds (row);
    const int w = 74;
    const int x = r.getRight() - 12 - (which == 0 ? 2 * w + 8 : w);
    return { x, r.getCentreY() - 13, w, 26 };
}

void VimaEditor::timerCallback()
{
    for (int i = 0; i < VimaProcessor::numTestChannels; ++i)
    {
        const int c = proc.noteCounter[(size_t) i];
        if (c != lastCount[(size_t) i])
        {
            lastCount[(size_t) i] = c;
            flash[(size_t) i] = 1.0f;
        }
        else
        {
            flash[(size_t) i] = juce::jmax (0.0f, flash[(size_t) i] - 0.12f);
        }
    }
    repaint();
}

void VimaEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < VimaProcessor::numTestChannels; ++i)
    {
        if (toggleBounds (i, 0).contains (e.getPosition()))
            proc.notesEnabled[(size_t) i] = ! proc.notesEnabled[(size_t) i];
        if (toggleBounds (i, 1).contains (e.getPosition()))
            proc.ccEnabled[(size_t) i] = ! proc.ccEnabled[(size_t) i];
    }
    repaint();
}

void VimaEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // Header
    g.setColour (col::panel);
    g.fillRect (0, 0, width, 52);
    g.setColour (col::border);
    g.fillRect (0, 52, width, 1);

    g.setColour (col::text);
    g.setFont (font (monoSemi, 15.0f));
    g.drawText ("VIMA", 20, 0, 80, 52, juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (sansSemi, 11.0f));
    g.drawText ("MIDI ROUTING TEST", 86, 0, 200, 52, juce::Justification::centredLeft);

    // Host status
    const bool playing = proc.hostPlaying;
    auto status = juce::Rectangle<int> (width - 220, 10, 200, 32);
    g.setColour (col::border);
    g.drawRoundedRectangle (status.toFloat().reduced (0.5f), 6.0f, 1.0f);
    {
        juce::Path tri;
        const float cx = (float) status.getX() + 16.0f, cy = (float) status.getCentreY();
        if (playing)
        {
            tri.addTriangle (cx - 4.0f, cy - 5.0f, cx - 4.0f, cy + 5.0f, cx + 5.0f, cy);
            g.setColour (col::green);
            g.fillPath (tri);
        }
        else
        {
            g.setColour (col::muted);
            g.fillRoundedRectangle (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, 1.0f);
        }
    }
    g.setFont (font (mono, 12.0f));
    g.setColour (col::text2);
    g.drawText ("HOST", status.getX() + 28, status.getY(), 40, status.getHeight(), juce::Justification::centredLeft);
    g.setColour (col::text);
    g.setFont (font (monoSemi, 12.0f));
    g.drawText (juce::String (proc.hostBpm.load(), 2) + " BPM", status.getX() + 66, status.getY(), 120, status.getHeight(),
                juce::Justification::centredLeft);

    // Column labels
    g.setColour (col::muted);
    g.setFont (font (sansSemi, 10.0f));
    g.drawText ("CHANNEL", 32, 70, 100, 16, juce::Justification::centredLeft);
    g.drawText ("TEST NOTE", 140, 70, 120, 16, juce::Justification::centredLeft);
    g.drawText ("CC 74", 300, 70, 60, 16, juce::Justification::centredLeft);

    const auto& channels = VimaProcessor::testChannels();
    for (int i = 0; i < VimaProcessor::numTestChannels; ++i)
    {
        const auto& tc = channels[(size_t) i];
        auto r = rowBounds (i);
        g.setColour (col::tile);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);

        // Activity LED
        const bool notesOn = proc.notesEnabled[(size_t) i];
        const float f = notesOn ? flash[(size_t) i] : 0.0f;
        auto led = juce::Rectangle<float> ((float) r.getX() + 12.0f, (float) r.getCentreY() - 4.0f, 8.0f, 8.0f);
        if (f > 0.0f)
        {
            g.setColour (col::green.withAlpha (0.35f * f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (f > 0.0f ? col::green.interpolatedWith (col::dim, 1.0f - f) : col::dim);
        g.fillEllipse (led);

        g.setColour (col::text);
        g.setFont (font (monoSemi, 14.0f));
        g.drawText ("CH " + juce::String (tc.midiChannel), r.getX() + 30, r.getY(), 80, r.getHeight(), juce::Justification::centredLeft);

        g.setFont (font (mono, 12.0f));
        g.setColour (col::text2);
        g.drawText (juce::String (noteName (tc.note)) + juce::String (tc.note / 12 - 2) + " (" + juce::String (tc.note) + ")",
                    r.getX() + 120, r.getY() + 6, 150, 18, juce::Justification::centredLeft);
        g.setColour (col::muted);
        g.setFont (font (sans, 11.0f));
        g.drawText (tc.rhythm, r.getX() + 120, r.getY() + 24, 150, 16, juce::Justification::centredLeft);

        // CC meter
        const bool ccOn = proc.ccEnabled[(size_t) i];
        auto meter = juce::Rectangle<float> ((float) r.getX() + 280.0f, (float) r.getCentreY() - 3.0f, 60.0f, 6.0f);
        g.setColour (col::border);
        g.fillRoundedRectangle (meter, 3.0f);
        if (ccOn)
        {
            g.setColour (col::cyan);
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * (float) proc.ccValue[(size_t) i] / 127.0f), 3.0f);
        }

        // Toggles
        for (int which = 0; which < 2; ++which)
        {
            const bool on = which == 0 ? notesOn : ccOn;
            const auto accent = which == 0 ? col::green : col::cyan;
            auto t = toggleBounds (i, which).toFloat();
            if (on)
            {
                g.setColour (accent);
                g.fillRoundedRectangle (t, 4.0f);
                g.setColour (col::onText);
            }
            else
            {
                g.setColour (col::dim);
                g.drawRoundedRectangle (t.reduced (0.75f), 4.0f, 1.5f);
                g.setColour (col::muted);
            }
            g.setFont (font (monoSemi, 10.0f));
            g.drawText (juce::String (which == 0 ? "NOTES " : "CC ") + (on ? "ON" : "OFF"), t, juce::Justification::centred);
        }
    }

    // Footer hint
    g.setColour (col::muted);
    g.setFont (font (sans, 11.0f));
    g.drawFittedText ("Plays only while the host plays. In each receiving MIDI track choose this track and \"Vima\" as MIDI From, "
                      "then filter by channel, to check that the channels arrive separately.",
                      20, height - 52, width - 40, 36, juce::Justification::topLeft, 2);
}
