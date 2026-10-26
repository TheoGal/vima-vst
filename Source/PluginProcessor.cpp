#include "PluginProcessor.h"
#include "PluginEditor.h"

VimaProcessor::VimaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    outBuffer.ensureSize (8192);
    startTimerHz (20);
}

void VimaProcessor::syncParamsToHost()
{
    {
        const Lock sl (lock);
        macros.capture (project, engine.queuedPattern.load());
    }
    macros.pushToHost();
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

    // Transport button: stopped with the button while the host played = held (userStopped).
    // A held VIMA ignores the host stopping and starting again; only its own button releases it.
    if (host.playing)
    {
        freeRun = false;
        if (userStopped) host.playing = false;
    }
    else if (freeRun)
    {
        if (freeRunRestart.exchange (false)) freePpq = 0.0;
        host.playing = true;
        host.ppq = freePpq;
        freePpq += buffer.getNumSamples() * (host.bpm / 60.0) / sampleRate;
    }

    outBuffer.clear();
    {
        const Lock sl (lock);
        if (macros.applyFromHost (project, engine))
            ++revision;
        engine.process (project, host, buffer.getNumSamples(), midi, outBuffer);
    }
    midi.swapWith (outBuffer);   // incoming MIDI is consumed, not passed through
}

juce::AudioProcessorEditor* VimaProcessor::createEditor()
{
    return new VimaEditor (*this);
}

void VimaProcessor::toggleTransport()
{
    if (engine.isPlaying.load())
    {
        if (hostPlaying.load()) userStopped = true;
        freeRun = false;
    }
    else if (hostPlaying.load())
        userStopped = false;
    else
    {
        userStopped = false;   // pressing play always releases the hold
        freeRunRestart = true;
        freeRun = true;
    }
}

// ---------------------------------------------------------------------------------------
static juce::File vimaFolder (const char* sub)
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VIMA").getChildFile (sub);
    f.createDirectory();
    return f;
}
juce::File VimaProcessor::presetFolder() { return vimaFolder ("Presets"); }
juce::File VimaProcessor::macroFolder() { return vimaFolder ("Macros"); }

static bool writeTree (const juce::ValueTree& tree, const juce::File& file)
{
    auto xml = tree.createXml();
    return xml != nullptr && file.getParentDirectory().createDirectory() && xml->writeTo (file);
}

static juce::ValueTree readTree (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

bool VimaProcessor::savePreset (const juce::File& file)
{
    const auto tree = read ([] (const vima::Project& p) { return p.musicToValueTree(); });
    if (! writeTree (tree, file))
        return false;
    presetFile = file;
    return true;
}

bool VimaProcessor::loadPreset (const juce::File& file)
{
    const auto tree = readTree (file);
    if (! tree.hasType ("VIMA"))
        return false;
    auto loaded = std::make_unique<vima::Project>();
    {
        const Lock sl (lock);
        *loaded = project;
    }
    loaded->loadMusic (tree);
    {
        const Lock sl (lock);
        project = *loaded;
        undo.clear();
    }
    engine.queuedPattern = -1;
    presetFile = file;
    ++revision;
    return true;
}

void VimaProcessor::newPreset()
{
    auto fresh = std::make_unique<vima::Project>();
    {
        const Lock sl (lock);
        *fresh = project;
    }
    fresh->resetMusic();
    {
        const Lock sl (lock);
        project = *fresh;
        undo.clear();
    }
    engine.queuedPattern = -1;
    presetFile = juce::File();
    ++revision;
}

bool VimaProcessor::saveMacros (const juce::File& file)
{
    return writeTree (read ([] (const vima::Project& p) { return p.macrosToValueTree(); }), file);
}

bool VimaProcessor::loadMacros (const juce::File& file)
{
    const auto tree = readTree (file);
    if (! tree.hasType ("VIMAMACROS"))
        return false;
    edit (-1, [&tree] (vima::Project& p) { p.loadMacros (tree); });
    return true;
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
    state.setProperty ("uiTrack", uiTrack, nullptr);
    state.setProperty ("presetPath", presetFile.getFullPathName(), nullptr);
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
    uiTrack = juce::jlimit (0, vima::numTracks - 1, (int) state.getProperty ("uiTrack", 0));
    {
        const auto path = state.getProperty ("presetPath").toString();
        presetFile = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    }
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
