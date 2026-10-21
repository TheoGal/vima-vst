#include "Theme.h"
#include "BinaryData.h"

namespace vima
{
static juce::Colour c (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }

const Theme& Theme::dark()
{
    static const Theme t {
        c (0x0E1114), c (0x12161B), c (0x161A20), c (0x1A1F27), c (0x101317), c (0x1F252D), c (0x222833),
        c (0x222830), c (0x262C35), c (0x2A313B), c (0x3A414C), c (0x4A525E), c (0x5A6372),
        c (0x8A94A3), c (0xC5CBD4), c (0xEEF1F5),
        c (0xDDE2E8), c (0x4FD98A), c (0x5CC8FF), c (0xB48CFF), c (0xFFB547),
        c (0x0B1410), c (0x0E1114), c (0xEEF1F5), juce::Colour (0x80000000)
    };
    return t;
}

const Theme& Theme::light()
{
    static const Theme t {
        c (0xD3D7DD), c (0xDCE0E5), c (0xE3E6EA), c (0xDADDE2), c (0xCACFD6), c (0xC7CCD3), c (0xCDD2D8),
        c (0xC2C7CF), c (0xB9BFC8), c (0xAEB5BF), c (0xA2A9B4), c (0x8A929E), c (0x6E7784),
        c (0x525B68), c (0x2C333D), c (0x11151A),
        c (0x2A313B), c (0x12864A), c (0x0874AE), c (0x6539C2), c (0xA86600),
        c (0xF5F7FA), c (0xD3D7DD), c (0x11151A), juce::Colour (0x38141E2D)
    };
    return t;
}

static bool lightOn = false;
const Theme& Theme::current() { return lightOn ? light() : dark(); }
void Theme::setLight (bool l) { lightOn = l; }
bool Theme::isLight() { return lightOn; }

// ---------------------------------------------------------------------------------------
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr sans, sansSemi, sansBold, mono, monoSemi;
        Typefaces()
        {
            sans     = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansRegular_ttf, BinaryData::IBMPlexSansRegular_ttfSize);
            sansSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansSemiBold_ttf, BinaryData::IBMPlexSansSemiBold_ttfSize);
            sansBold = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansBold_ttf, BinaryData::IBMPlexSansBold_ttfSize);
            mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
            monoSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);
        }
    };

    Typefaces& faces()
    {
        static Typefaces f;
        return f;
    }

    juce::Font make (juce::Typeface::Ptr tf, float size, float tracking = 0.0f)
    {
        // Sizes are given like CSS pixel sizes (em size), as in the mockups
        return juce::Font (juce::FontOptions (tf).withPointHeight (size).withKerningFactor (tracking));
    }
}

juce::Font Fonts::sans (float s)     { return make (faces().sans, s); }
juce::Font Fonts::sansSemi (float s) { return make (faces().sansSemi, s); }
juce::Font Fonts::sansBold (float s) { return make (faces().sansBold, s); }
juce::Font Fonts::mono (float s)     { return make (faces().mono, s); }
juce::Font Fonts::monoSemi (float s) { return make (faces().monoSemi, s); }
juce::Font Fonts::label (float s)    { return make (faces().sansSemi, s, 0.12f); }
} // namespace vima
