#include "FileActions.h"

namespace vima
{
FileActions::FileActions (Context& c, juce::Component& o) : ctx (c), owner (o) {}

void FileActions::addMenu (juce::PopupMenu& m, int patternIndex)
{
    const auto preset = ctx.proc.presetFile;
    const auto macros = ctx.proc.macroFile;
    m.addSectionHeader (preset == juce::File() ? juce::String ("PRESET: UNSAVED") : "PRESET: " + preset.getFileNameWithoutExtension().toUpperCase());
    m.addItem (1, "NEW");
    m.addItem (2, "LOAD...");
    m.addItem (3, "SAVE", preset != juce::File());
    m.addItem (4, "SAVE AS...");
    m.addSectionHeader ("PATTERN " + patternLabel (patternIndex));
    m.addItem (11, "NEW");
    m.addItem (12, "LOAD...");
    m.addItem (13, "SAVE AS...");
    m.addSectionHeader ("MACROS");
    m.addItem (21, "NEW");
    m.addItem (22, "LOAD...");
    m.addItem (23, "SAVE", macros != juce::File());
    m.addItem (24, "SAVE AS...");
}

void FileActions::perform (int id, int patternIndex)
{
    switch (id)
    {
        case 1: newPreset(); break;
        case 2: loadPreset(); break;
        case 3: savePreset(); break;
        case 4: savePresetAs(); break;
        case 11: newPattern (patternIndex); break;
        case 12: loadPattern (patternIndex); break;
        case 13: savePatternAs (patternIndex); break;
        case 21: newMacros(); break;
        case 22: loadMacros(); break;
        case 23: saveMacros(); break;
        case 24: saveMacrosAs(); break;
        default: break;
    }
}

void FileActions::confirm (const juce::String& title, const juce::String& text, std::function<void()> ok)
{
    juce::Component::SafePointer<juce::Component> safe (&owner);
    juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, title, text, &owner,
        juce::ModalCallbackFunction::create ([safe, ok] (int r)
        {
            if (r != 0 && safe != nullptr) ok();
        }));
}

void FileActions::choose (bool save, const juce::String& title, const juce::File& start, const char* extension,
                          std::function<void (const juce::File&)> done)
{
    chooser = std::make_unique<juce::FileChooser> (title, start, juce::String ("*") + extension);
    const int flags = juce::FileBrowserComponent::canSelectFiles
                      | (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode);
    juce::Component::SafePointer<juce::Component> safe (&owner);
    const juce::String ext (extension);
    chooser->launchAsync (flags, [safe, save, ext, done] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto file = fc.getResult();
        if (file == juce::File()) return;
        if (save && ! file.hasFileExtension (ext))
            file = file.withFileExtension (ext);
        done (file);
    });
}

// ---- presets
void FileActions::newPreset()
{
    confirm ("New preset", "Clear all patterns, chain and settings? Macro mappings are kept.", [this]
    {
        ctx.proc.newPreset();
        ctx.ui.selection = 0;
        ctx.changed();
    });
}

void FileActions::loadPreset()
{
    const auto current = ctx.proc.presetFile;
    const auto start = current != juce::File() ? current : VimaProcessor::presetFolder().getChildFile (juce::String ("Untitled") + VimaProcessor::presetExtension);
    choose (false, "Load VIMA preset", start, VimaProcessor::presetExtension, [this] (const juce::File& f)
    {
        if (ctx.proc.loadPreset (f)) { ctx.ui.selection = 0; ctx.changed(); }
    });
}

void FileActions::savePreset()
{
    if (ctx.proc.presetFile != juce::File()) { ctx.proc.savePreset (ctx.proc.presetFile); ctx.changed(); }
}

void FileActions::savePresetAs()
{
    const auto current = ctx.proc.presetFile;
    const auto start = current != juce::File() ? current : VimaProcessor::presetFolder().getChildFile (juce::String ("Untitled") + VimaProcessor::presetExtension);
    choose (true, "Save VIMA preset", start, VimaProcessor::presetExtension, [this] (const juce::File& f)
    {
        ctx.proc.savePreset (f);
        ctx.changed();
    });
}

// ---- patterns
void FileActions::newPattern (int index)
{
    confirm ("New pattern", "Reset pattern " + patternLabel (index) + " to the init pattern? You can undo this.", [this, index]
    {
        ctx.proc.newPattern (index);
        ctx.ui.selection = 0;
        ctx.changed();
    });
}

void FileActions::loadPattern (int index)
{
    const auto start = VimaProcessor::patternFolder().getChildFile (juce::String ("Pattern") + VimaProcessor::patternExtension);
    choose (false, "Load pattern into " + patternLabel (index), start, VimaProcessor::patternExtension, [this, index] (const juce::File& f)
    {
        auto apply = [this, index, f]
        {
            if (ctx.proc.loadPattern (index, f)) { ctx.ui.selection = 0; ctx.changed(); }
        };
        const bool empty = ctx.proc.read ([index] (const Project& p) { return p.patterns[(size_t) index].isEmpty(); });
        if (empty)
            apply();
        else
            confirm ("Load pattern", "Replace pattern " + patternLabel (index) + " with " + f.getFileNameWithoutExtension().toUpperCase()
                                         + "? You can undo this.", apply);
    });
}

void FileActions::savePatternAs (int index)
{
    const auto start = VimaProcessor::patternFolder().getChildFile ("Pattern " + patternLabel (index) + VimaProcessor::patternExtension);
    choose (true, "Save pattern " + patternLabel (index), start, VimaProcessor::patternExtension, [this, index] (const juce::File& f)
    {
        ctx.proc.savePattern (index, f);
    });
}

// ---- macros
void FileActions::newMacros()
{
    confirm ("New macro mappings", "Clear all 128 macro mappings?", [this]
    {
        ctx.proc.newMacros();
        ctx.changed();
    });
}

void FileActions::loadMacros()
{
    const auto current = ctx.proc.macroFile;
    const auto start = current != juce::File() ? current : VimaProcessor::macroFolder().getChildFile (juce::String ("Mappings") + VimaProcessor::macroExtension);
    choose (false, "Load macro mappings", start, VimaProcessor::macroExtension, [this] (const juce::File& f)
    {
        auto apply = [this, f] { if (ctx.proc.loadMacros (f)) ctx.changed(); };
        const bool any = ctx.proc.read ([] (const Project& p)
        {
            for (const auto& m : p.macros) if (m.mapped()) return true;
            return false;
        });
        if (any)
            confirm ("Load macro mappings", "Replace all macro mappings with " + f.getFileNameWithoutExtension().toUpperCase() + "?", apply);
        else
            apply();
    });
}

void FileActions::saveMacros()
{
    if (ctx.proc.macroFile != juce::File()) { ctx.proc.saveMacros (ctx.proc.macroFile); ctx.changed(); }
}

void FileActions::saveMacrosAs()
{
    const auto current = ctx.proc.macroFile;
    const auto start = current != juce::File() ? current : VimaProcessor::macroFolder().getChildFile (juce::String ("Mappings") + VimaProcessor::macroExtension);
    choose (true, "Save macro mappings", start, VimaProcessor::macroExtension, [this] (const juce::File& f)
    {
        ctx.proc.saveMacros (f);
        ctx.changed();
    });
}
} // namespace vima
