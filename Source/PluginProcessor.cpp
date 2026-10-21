#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

const std::array<VimaProcessor::TestChannel, VimaProcessor::numTestChannels>& VimaProcessor::testChannels()
{
    static const std::array<TestChannel, numTestChannels> channels { {
        { 1, 36, "every beat" },
        { 2, 42, "off-beat 8ths" },
        { 3, 48, "every 16th" },
        { 4, 60, "every bar" },
    } };
    return channels;
}

VimaProcessor::VimaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < numTestChannels; ++i)
    {
        notesEnabled[(size_t) i] = true;
        ccEnabled[(size_t) i] = true;
        noteCounter[(size_t) i] = 0;
        ccValue[(size_t) i] = 0;
    }
    pending.reserve (64);
}

bool VimaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void VimaProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    pending.clear();
    wasPlaying = false;
    lastPpqEnd = -1.0;
}

void VimaProcessor::flushNotes (juce::MidiBuffer& midi, int samplePos)
{
    for (const auto& p : pending)
        midi.addEvent (juce::MidiMessage::noteOff (p.channel, p.note), samplePos);
    pending.clear();
}

static bool hitsOnStep (int index, long long sixteenth)
{
    switch (index)
    {
        case 0:  return sixteenth % 4 == 0;     // every beat
        case 1:  return sixteenth % 4 == 2;     // off-beat 8ths
        case 2:  return true;                   // every 16th
        case 3:  return sixteenth % 16 == 0;    // every bar (4/4)
        default: return false;
    }
}

void VimaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();   // VIMA generates its own MIDI; incoming MIDI is not passed through

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;

    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            else playing = false;   // cannot sync without a musical position
        }
    }

    hostPlaying = playing;
    hostBpm = bpm;
    hostPpq = ppq;

    if (! playing)
    {
        if (wasPlaying)
            flushNotes (midi, 0);
        wasPlaying = false;
        lastPpqEnd = -1.0;
        return;
    }

    const double ppqPerSample = (bpm / 60.0) / sampleRate;
    const double ppqEnd = ppq + numSamples * ppqPerSample;

    // A jump in song position (loop, locate): stop hanging notes first
    if (wasPlaying && lastPpqEnd >= 0.0 && std::abs (ppq - lastPpqEnd) > 0.01)
        flushNotes (midi, 0);

    auto toSample = [&] (double eventPpq)
    {
        auto s = (int) std::floor ((eventPpq - ppq) / ppqPerSample);
        return juce::jlimit (0, numSamples - 1, s);
    };

    // Note-offs that fall inside this block
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->ppq < ppqEnd)
        {
            midi.addEvent (juce::MidiMessage::noteOff (it->channel, it->note), toSample (it->ppq));
            it = pending.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // 16th-note grid
    const auto& channels = testChannels();
    for (auto k = (long long) std::ceil (ppq * 4.0 - 1.0e-9); (double) k / 4.0 < ppqEnd; ++k)
    {
        const double stepPpq = (double) k / 4.0;
        const int s = toSample (stepPpq);

        for (int i = 0; i < numTestChannels; ++i)
        {
            const auto& tc = channels[(size_t) i];

            if (ccEnabled[(size_t) i])
            {
                // triangle sweep over 2 bars, phase-shifted per channel
                const auto phase = (k + i * 8) % 32;
                const int value = (int) juce::jlimit (0LL, 127LL, phase < 16 ? phase * 8 : (32 - phase) * 8 - 1);
                midi.addEvent (juce::MidiMessage::controllerEvent (tc.midiChannel, 74, value), s);
                ccValue[(size_t) i] = value;
            }

            if (notesEnabled[(size_t) i] && hitsOnStep (i, k))
            {
                midi.addEvent (juce::MidiMessage::noteOn (tc.midiChannel, tc.note, (juce::uint8) 100), s);
                pending.push_back ({ stepPpq + 0.125, tc.midiChannel, tc.note });   // half a 16th
                noteCounter[(size_t) i] = noteCounter[(size_t) i] + 1;
            }
        }
    }

    wasPlaying = true;
    lastPpqEnd = ppqEnd;
}

juce::AudioProcessorEditor* VimaProcessor::createEditor()
{
    return new VimaEditor (*this);
}

void VimaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("VimaTest");
    for (int i = 0; i < numTestChannels; ++i)
    {
        state.setProperty ("notes" + juce::String (i), notesEnabled[(size_t) i].load(), nullptr);
        state.setProperty ("cc" + juce::String (i), ccEnabled[(size_t) i].load(), nullptr);
    }
    juce::MemoryOutputStream out (destData, false);
    state.writeToStream (out);
}

void VimaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! state.hasType ("VimaTest"))
        return;
    for (int i = 0; i < numTestChannels; ++i)
    {
        notesEnabled[(size_t) i] = (bool) state.getProperty ("notes" + juce::String (i), true);
        ccEnabled[(size_t) i] = (bool) state.getProperty ("cc" + juce::String (i), true);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VimaProcessor();
}
