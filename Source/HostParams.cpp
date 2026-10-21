#include "HostParams.h"

namespace vima
{
HostParams::HostParams (juce::AudioProcessor& processor)
{
    auto add = [this] (juce::AudioProcessorParameterGroup& group, std::unique_ptr<juce::RangedAudioParameter> p, Kind kind, int track)
    {
        Entry e;
        e.param = p.get();
        e.kind = kind;
        e.track = track;
        e.lastApplied = p->getValue();
        entries.push_back (e);
        group.addChild (std::move (p));
    };

    auto global = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", "|");
    add (*global, std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pattern", 1 }, "Pattern", 1, numPatterns, 1),
         Kind::pattern, -1);
    add (*global, std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "chainOn", 1 }, "Chain On", false),
         Kind::chainOn, -1);
    processor.addParameterGroup (std::move (global));

    juce::StringArray divisions;
    for (int i = 0; i < numDivisions; ++i) divisions.add (divisionInfo (i).name);
    const juce::StringArray directions { "Forward", "Backward", "Ping-pong" };

    for (int t = 0; t < numTracks; ++t)
    {
        const auto num = juce::String (t + 1).paddedLeft ('0', 2);
        const auto id = [&num] (const char* name) { return juce::ParameterID { "t" + num + "_" + name, 1 }; };
        const auto label = [&num] (const char* name) { return "T" + num + " " + name; };
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("track" + num, "Track " + num, "|");
        add (*group, std::make_unique<juce::AudioParameterBool> (id ("noteOut"), label ("Notes Out"), true), Kind::noteOut, t);
        add (*group, std::make_unique<juce::AudioParameterBool> (id ("ccOut"), label ("CC Out"), false), Kind::ccOut, t);
        add (*group, std::make_unique<juce::AudioParameterInt> (id ("length"), label ("Length"), minLength, maxSteps, 16), Kind::length, t);
        add (*group, std::make_unique<juce::AudioParameterChoice> (id ("division"), label ("Division"), divisions, defaultDivision()),
             Kind::division, t);
        add (*group, std::make_unique<juce::AudioParameterChoice> (id ("direction"), label ("Direction"), directions, 0), Kind::direction, t);
        add (*group, std::make_unique<juce::AudioParameterInt> (id ("swing"), label ("Swing"), 0, 100, 0), Kind::swing, t);
        processor.addParameterGroup (std::move (group));
    }
}

int HostParams::projectValue (const Project& p, const Entry& e)
{
    const auto& tp = e.track >= 0 ? p.patterns[(size_t) p.currentPattern].tracks[(size_t) e.track] : TrackPattern();
    switch (e.kind)
    {
        case Kind::pattern:   return p.currentPattern + 1;
        case Kind::chainOn:   return p.globals.chainOn ? 1 : 0;
        case Kind::noteOut:   return p.routing[(size_t) e.track].noteOut ? 1 : 0;
        case Kind::ccOut:     return p.routing[(size_t) e.track].ccOut ? 1 : 0;
        case Kind::length:    return tp.length;
        case Kind::division:  return tp.division;
        case Kind::direction: return (int) tp.direction;
        case Kind::swing:     return tp.swing;
    }
    return 0;
}

bool HostParams::applyFromHost (Project& p, Engine& engine)
{
    bool changed = false;
    for (auto& e : entries)
    {
        const float v = e.param->getValue();
        if (juce::exactlyEqual (v, e.lastApplied))
            continue;
        e.lastApplied = v;
        const int plain = juce::roundToInt (e.param->convertFrom0to1 (v));
        if (plain == projectValue (p, e))
            continue;

        auto& tp = e.track >= 0 ? p.patterns[(size_t) p.currentPattern].tracks[(size_t) e.track] : p.patterns[0].tracks[0];
        switch (e.kind)
        {
            case Kind::pattern:   engine.queuedPattern = juce::jlimit (0, numPatterns - 1, plain - 1); break;   // follows quantize
            case Kind::chainOn:   p.globals.chainOn = plain != 0; break;
            case Kind::noteOut:   p.routing[(size_t) e.track].noteOut = plain != 0; break;
            case Kind::ccOut:     p.routing[(size_t) e.track].ccOut = plain != 0; break;
            case Kind::length:    tp.length = (std::uint8_t) juce::jlimit (minLength, maxSteps, plain); break;
            case Kind::division:  tp.division = (std::uint8_t) juce::jlimit (0, numDivisions - 1, plain); break;
            case Kind::direction: tp.direction = (Direction) juce::jlimit (0, 2, plain); break;
            case Kind::swing:     tp.swing = (std::uint8_t) juce::jlimit (0, 100, plain); break;
        }
        changed = changed || e.kind != Kind::pattern;
    }
    return changed;
}

void HostParams::capture (const Project& p, std::vector<int>& values) const
{
    values.resize (entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
        values[i] = projectValue (p, entries[i]);
}

void HostParams::pushToHost (const std::vector<int>& values)
{
    for (size_t i = 0; i < entries.size() && i < values.size(); ++i)
    {
        auto& e = entries[i];
        const int plain = values[i];
        if (plain == e.lastPushed)
            continue;
        e.lastPushed = plain;
        const float norm = e.param->convertTo0to1 ((float) plain);
        if (std::abs (e.param->getValue() - norm) < 1.0e-6f)
            continue;   // already there (e.g. the host itself set it)
        e.param->beginChangeGesture();
        e.param->setValueNotifyingHost (norm);
        e.param->endChangeGesture();
    }
}

juce::RangedAudioParameter* HostParams::find (const juce::String& id) const
{
    for (const auto& e : entries)
        if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*> (e.param); withId != nullptr && withId->getParameterID() == id)
            return e.param;
    return nullptr;
}
} // namespace vima
