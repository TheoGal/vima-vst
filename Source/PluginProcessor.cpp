#include "PluginProcessor.h"
#include "PluginEditor.h"

VimaProcessor::VimaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    outBuffer.ensureSize (8192);
    paramValues.reserve ((size_t) hostParams.size());
    startTimerHz (20);
}

void VimaProcessor::syncParamsToHost()
{
    {
        const Lock sl (lock);
        hostParams.capture (project, paramValues);
    }
    hostParams.pushToHost (paramValues);
}

bool VimaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void VimaProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const Lock sl (lock);
    engine.prepare (sampleRate);
}

void VimaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    vima::Engine::Host host;
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            host.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) host.bpm = *b;
            if (auto p = pos->getPpqPosition()) host.ppq = *p;
            else host.playing = false;   // cannot sync without a musical position
            if (auto ts = pos->getTimeSignature())
            {
                host.timeSigNumerator = ts->numerator;
                host.timeSigDenominator = ts->denominator;
            }
        }
    }
    hostPlaying = host.playing;
    hostBpm = host.bpm;

    outBuffer.clear();
    {
        const Lock sl (lock);
        if (hostParams.applyFromHost (project, engine))
            ++revision;
        engine.process (project, host, buffer.getNumSamples(), midi, outBuffer);
    }
    midi.swapWith (outBuffer);   // incoming MIDI is consumed, not passed through
}

juce::AudioProcessorEditor* VimaProcessor::createEditor()
{
    return new VimaEditor (*this);
}

bool VimaProcessor::performUndo()
{
    const Lock sl (lock);
    int restored = -1;
    if (! undo.undo (project, restored))
        return false;
    ++revision;
    return true;
}

void VimaProcessor::queuePattern (int index)
{
    engine.queuedPattern = juce::jlimit (0, vima::numPatterns - 1, index);
}

void VimaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state;
    {
        const Lock sl (lock);
        state = project.toValueTree();
    }
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VimaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("VIMA"))
        return;
    uiScale = juce::jlimit (0.75f, 2.0f, (float) (double) state.getProperty ("uiScale", 1.0));
    auto loaded = std::make_unique<vima::Project>();
    loaded->fromValueTree (state);
    {
        const Lock sl (lock);
        project = *loaded;
        undo.clear();
    }
    ++revision;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VimaProcessor();
}
