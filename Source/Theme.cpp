#include "Theme.h"
#include "BinaryData.h"

namespace vima
{
static juce::Colour c (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }

const Theme& Theme::dark()
{
    static const Theme t {
        c (0x0E1114), c (0x12161B), c (0x161A20), c (0x1A1F27), c (0x101317), c (0x1F252D), c (0x222833),
        c (0x343C48), c (0x3A4250), c (0x404957), c (0x3A414C), c (0x4A525E), c (0x5A6372),   // borders: firm strokes
        c (0x8A94A3), c (0xC5CBD4), c (0xEEF1F5),
        c (0xDDE2E8), c (0x4FD98A), c (0x5CC8FF), c (0xB48CFF), c (0xFFB547),
        c (0x0B1410), c (0x0E1114), c (0xEEF1F5), juce::Colour (0x80000000),
        c (0x222830), c (0x2A313B), c (0x101317)
    };
    return t;
}

const Theme& Theme::light()
{
    static const Theme t {
        // cool greys (blue about 19 above red). A darker frame with brighter tiles, firm lines
        // and saturated accents; knob tracks and idle LEDs stay soft so values read clearly (SPEC.md 12.1)
        c (0xC3CDDA), c (0xD6DEE9), c (0xEAEFF6), c (0xE1E8F1), c (0xCDD5E0), c (0xC2CCD9), c (0xD9E1EC),
        c (0xA3AEBD), c (0x97A2B2), c (0x8C98A8), c (0xA9B3C2), c (0x8A929D), c (0x6F7782),
        c (0x525B68), c (0x2C333D), c (0x11151A),
        c (0x2A313B), c (0x00954D), c (0x0077C8), c (0x6A2CE6), c (0xC26D00),
        c (0xF5F7FA), c (0xC3CDDA), c (0x11151A), juce::Colour (0x38141E2D),
        c (0xBFC7D2), c (0xADB5C0), c (0xC7CFDA)
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
