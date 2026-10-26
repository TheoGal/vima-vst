#include "Theme.h"
#include "BinaryData.h"
#include <atomic>

namespace vima
{
static juce::Colour c (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }

const Theme& Theme::dark()
{
    static const Theme t {
        c (0x0E1114), c (0x171C23), c (0x1C2129), c (0x212730), c (0x13171C), c (0x090B0E), c (0x29303B),
        c (0x343C48), c (0x3A4250), c (0x404957), c (0x3A414C), c (0x4A525E), c (0x5A6372),   // borders: firm strokes
        c (0x8A94A3), c (0xC5CBD4), c (0xEEF1F5),
        c (0xDDE2E8), c (0x4FD98A), c (0x5CC8FF), c (0xB48CFF), c (0xFFB547),
        c (0x0B1410), c (0x0E1114), c (0xEEF1F5), juce::Colour (0x80000000),
        c (0x090B0E), c (0x2A313B), c (0x101317)
    };
    return t;
}

const Theme& Theme::light()
{
    static const Theme t {
        // cool greys (blue about 19 above red). A darker frame with brighter tiles, firm lines
        // and saturated accents; knob tracks and idle LEDs stay soft so values read clearly (SPEC.md 12.1)
        c (0xC3CDDA), c (0xD6DEE9), c (0xEAEFF6), c (0xE1E8F1), c (0xCDD5E0), c (0xC2CCD9), c (0xF7F9FC),
        c (0xA3AEBD), c (0x97A2B2), c (0x8C98A8), c (0xA9B3C2), c (0x8A929D), c (0x6F7782),
        c (0x525B68), c (0x2C333D), c (0x11151A),
        c (0x2A313B), c (0x00954D), c (0x0077C8), c (0x6A2CE6), c (0xC26D00),
        c (0xF5F7FA), c (0xC3CDDA), c (0x11151A), juce::Colour (0x38141E2D),
        c (0xBFC7D2), c (0xADB5C0), c (0xC7CFDA)
    };
    return t;
}

// ---------------------------------------------------------------------------------------
// User-wide theme preference
namespace
{
    std::atomic<int> themeRevision { 0 };
    bool lightOn = false;
    bool loaded = false;

    juce::File& settingsFile()
    {
        // The app settings folder (no permission prompts or blocks, unlike Documents):
        // Windows %APPDATA%\VIMA, macOS ~/Library/Application Support/VIMA, Linux ~/.config/VIMA
        static juce::File f = []
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
           #if JUCE_MAC
            dir = dir.getChildFile ("Application Support");
           #endif
            return dir.getChildFile ("VIMA").getChildFile ("Settings.xml");
        }();
        return f;
    }

    void ensureLoaded()
    {
        if (loaded) return;
        loaded = true;
        if (auto xml = juce::XmlDocument::parse (settingsFile()))
            lightOn = xml->getBoolAttribute ("lightTheme", false);
    }
}

const Theme& Theme::current() { ensureLoaded(); return lightOn ? light() : dark(); }
bool Theme::isLight() { ensureLoaded(); return lightOn; }
int Theme::revision() { return themeRevision.load(); }

void Theme::setLight (bool l)
{
    ensureLoaded();
    if (l == lightOn) return;
    lightOn = l;
    ++themeRevision;
    juce::XmlElement xml ("VIMASETTINGS");
    xml.setAttribute ("lightTheme", l);
    settingsFile().getParentDirectory().createDirectory();
    xml.writeTo (settingsFile());
}

void Theme::setSettingsFile (const juce::File& f)
{
    settingsFile() = f;
    loaded = false;
}

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
            mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::VIMAMonoRegular_ttf, BinaryData::VIMAMonoRegular_ttfSize);
            monoSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::VIMAMonoSemiBold_ttf, BinaryData::VIMAMonoSemiBold_ttfSize);
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
