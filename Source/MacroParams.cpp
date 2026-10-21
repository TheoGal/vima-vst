#include "MacroParams.h"
#include "Macros.h"

namespace vima
{
MacroParameter::MacroParameter (int i, std::function<juce::String (int, float)> text)
    : juce::AudioParameterFloat (juce::ParameterID { "macro" + juce::String (i + 1).paddedLeft ('0', 3), 1 },
                                 "Macro " + juce::String (i + 1), juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
      index (i), textFn (std::move (text))
{
}

juce::String MacroParameter::getText (float normalised, int maximumLength) const
{
    auto s = textFn ? textFn (index, normalised) : juce::String (normalised, 2);
    return maximumLength > 0 ? s.substring (0, maximumLength) : s;
}

// ---------------------------------------------------------------------------------------
MacroParams::MacroParams (juce::AudioProcessor& processor)
{
    for (int b = 0; b < numMacros / macrosPerBank; ++b)
    {
        const int first = b * macrosPerBank + 1;
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            "macros" + juce::String (b + 1), "Macros " + juce::String (first) + "-" + juce::String (first + macrosPerBank - 1), "|");
        for (int k = 0; k < macrosPerBank; ++k)
        {
            const int i = b * macrosPerBank + k;
            auto p = std::make_unique<MacroParameter> (i, [this] (int idx, float v) { return textFor (idx, v); });
            entries[(size_t) i].param = p.get();
            entries[(size_t) i].lastApplied = static_cast<juce::AudioProcessorParameter*> (p.get())->getValue();
            group->addChild (std::move (p));
        }
        processor.addParameterGroup (std::move (group));
    }
}

juce::String MacroParams::textFor (int index, float normalised)
{
    const juce::SpinLock::ScopedLockType sl (textLock);
    const auto& m = textMaps[(size_t) index];
    if (! m.mapped())
        return "-";
    return macroValueText (textGlobals, m, macroPlain (m, normalised, textNotes[(size_t) index]));
}

bool MacroParams::applyFromHost (Project& p, Engine& engine, int shownTrack)
{
    bool changed = false;
    for (int i = 0; i < numMacros; ++i)
    {
        auto& e = entries[(size_t) i];
        const float v = static_cast<juce::AudioProcessorParameter*> (e.param)->getValue();
        if (juce::exactlyEqual (v, e.lastApplied))
            continue;
        e.lastApplied = v;
        if (std::abs (v - e.pushed.load()) < 1.0e-6f)
            continue;   // VIMA sent this value itself: the target already has it
        const auto& m = p.macros[(size_t) i];
        if (m.mapped())
            changed = applyMacroValue (p, engine.queuedPattern, m, shownTrack, macroPlain (m, v, macroNotes (p, m, shownTrack))) || changed;
    }
    return changed;
}

void MacroParams::capture (const Project& p, int shownTrack)
{
    for (int i = 0; i < numMacros; ++i)
    {
        maps[(size_t) i] = p.macros[(size_t) i];
        const auto v = macroTargetValue (p, maps[(size_t) i], shownTrack);
        valid[(size_t) i] = v.has_value();
        values[(size_t) i] = v.value_or (0);
        notes[(size_t) i] = macroNotes (p, maps[(size_t) i], shownTrack);
    }
    const juce::SpinLock::ScopedLockType sl (textLock);
    textMaps = maps;
    textGlobals = p.globals;
    textNotes = notes;
}

void MacroParams::pushToHost()
{
    for (int i = 0; i < numMacros; ++i)
    {
        auto& e = entries[(size_t) i];
        const auto& m = maps[(size_t) i];
        if (! valid[(size_t) i])
        {
            e.lastMap = m;
            continue;
        }
        const int plain = values[(size_t) i];
        if (plain == e.lastPushed && m == e.lastMap)
            continue;
        e.lastPushed = plain;
        e.lastMap = m;
        const auto& n = notes[(size_t) i];
        const float norm = macroNormalized (m, plain, n);
        if (macroPlain (m, static_cast<juce::AudioProcessorParameter*> (e.param)->getValue(), n) == macroPlain (m, norm, n))
            continue;   // the macro already shows this value (for example the host set it)
        e.pushed = norm;
        e.param->beginChangeGesture();
        e.param->setValueNotifyingHost (norm);
        e.param->endChangeGesture();
    }
}

void MacroParams::beginGesture (int index) { entries[(size_t) index].param->beginChangeGesture(); }
void MacroParams::endGesture (int index) { entries[(size_t) index].param->endChangeGesture(); }

void MacroParams::setFromUi (int index, float normalised)
{
    auto& e = entries[(size_t) index];
    e.pushed = -1.0f;   // a UI move must be applied
    e.param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}
} // namespace vima
