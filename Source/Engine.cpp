#include "Engine.h"
#include <cmath>

namespace vima
{
Engine::Engine()
{
    for (auto& s : playStep) s = -1;
    lastCCSent.fill (-1);
    random.setSeedRandomly();
}

void Engine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void Engine::reset()
{
    numPending = 0;
    wasPlaying = false;
    lastPpqEnd = -1.0;
    lastCCSent.fill (-1);
    chainActive = false;
    lastChainOnState = -1;
    for (auto& s : playStep) s = -1;
    isPlaying = false;
}

int Engine::stepIndexFor (long long n, int length, Direction dir)
{
    length = juce::jlimit (1, maxSteps, length);
    const auto len = (long long) length;
    switch (dir)
    {
        case Direction::forward:  return (int) (((n % len) + len) % len);
        case Direction::backward: return (int) (len - 1 - (((n % len) + len) % len));
        case Direction::pingPong:
        {
            if (len < 2) return 0;
            const long long period = 2 * len - 2;
            const long long p = ((n % period) + period) % period;
            return (int) (p < len ? p : period - p);
        }
    }
    return 0;
}

double Engine::swingOffset (long long n, double stepPpq, int swingPercent)
{
    const bool odd = (((n % 2) + 2) % 2) == 1;
    return odd ? (juce::jlimit (0, 100, swingPercent) / 100.0) * 0.5 * stepPpq : 0.0;
}

// ---------------------------------------------------------------------------------------
void Engine::addPendingOff (double ppq, int channel, int note)
{
    if (numPending < maxPending)
        pending[(size_t) numPending++] = { ppq, (std::uint8_t) channel, (std::uint8_t) note };
}

void Engine::releaseSameNote (int channel, int note, int samplePos, juce::MidiBuffer& out)
{
    for (int i = 0; i < numPending;)
    {
        auto& p = pending[(size_t) i];
        if (p.channel == channel && p.note == note)
        {
            out.addEvent (juce::MidiMessage::noteOff (channel, note), samplePos);
            pending[(size_t) i] = pending[(size_t) --numPending];
        }
        else
        {
            ++i;
        }
    }
}

void Engine::emitDueNoteOffs (double beforePpq, double blockStartPpq, double ppqPerSample, int numSamples, juce::MidiBuffer& out)
{
    for (int i = 0; i < numPending;)
    {
        auto& p = pending[(size_t) i];
        if (p.ppq < beforePpq)
        {
            const int s = juce::jlimit (0, numSamples - 1, (int) std::floor ((p.ppq - blockStartPpq) / ppqPerSample));
            out.addEvent (juce::MidiMessage::noteOff (p.channel, p.note), s);
            pending[(size_t) i] = pending[(size_t) --numPending];
        }
        else
        {
            ++i;
        }
    }
}

void Engine::flushAllNotes (juce::MidiBuffer& out, int samplePos)
{
    for (int i = 0; i < numPending; ++i)
        out.addEvent (juce::MidiMessage::noteOff (pending[(size_t) i].channel, pending[(size_t) i].note), samplePos);
    numPending = 0;
}

// ---------------------------------------------------------------------------------------
double Engine::nextQuantizePoint (const Project& project, const Host& host, double ppq) const
{
    double unit = 0.0;
    const double beat = 4.0 / juce::jmax (1, host.timeSigDenominator);
    switch (project.globals.quantize)
    {
        case Quantize::off:        return ppq;
        case Quantize::beat:       unit = beat; break;
        case Quantize::bar:        unit = beat * juce::jmax (1, host.timeSigNumerator); break;
        case Quantize::patternEnd: unit = project.patterns[(size_t) project.currentPattern].lengthPpq(); break;
    }
    if (unit <= 1.0e-9)
        return ppq;
    return std::ceil ((ppq - 1.0e-9) / unit) * unit;
}

double Engine::chainEntryEnd (const Project& project) const
{
    const auto& chain = project.globals.chain;
    if (chain.isEmpty())
        return 1.0e18;
    const auto& entry = chain.getReference (juce::jlimit (0, chain.size() - 1, chainIndex));
    const double len = project.patterns[(size_t) project.currentPattern].lengthPpq();
    return chainEntryStart + juce::jmax (1.0e-6, len) * juce::jmax (1, (int) entry.repeats);
}

// ---------------------------------------------------------------------------------------
void Engine::renderSegment (const Project& project, int patternIndex, double a, double b, double blockStartPpq,
                            double ppqPerSample, int numSamples, juce::MidiBuffer& out)
{
    if (b <= a)
        return;

    const auto& pattern = project.patterns[(size_t) patternIndex];
    const auto& g = project.globals;
    const auto mask = g.scaleMask();

    auto sampleFor = [&] (double t)
    {
        return juce::jlimit (0, numSamples - 1, (int) std::floor ((t - blockStartPpq) / ppqPerSample + 1.0e-6));
    };

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& tp = pattern.tracks[(size_t) t];
        const auto& route = project.routing[(size_t) t];
        const double stepLen = divisionInfo (tp.division).stepPpq;
        const int length = juce::jlimit (minLength, maxSteps, (int) tp.length);

        const auto nFirst = (long long) std::floor ((a - 0.5 * stepLen) / stepLen) - 1;
        const auto nLast = (long long) std::floor (b / stepLen) + 1;

        for (auto n = nFirst; n <= nLast; ++n)
        {
            const double tn = (double) n * stepLen + swingOffset (n, stepLen, tp.swing);
            if (tn < a - 1.0e-9 || tn >= b - 1.0e-9)
                continue;

            const int idx = stepIndexFor (n, length, tp.direction);
            playStep[(size_t) t] = idx;

            const auto& step = tp.steps[(size_t) idx];
            if (! step.on)
                continue;

            const int s = sampleFor (tn);
            const int channel = juce::jlimit (1, 16, (int) route.midiChannel);

            if (route.noteOut && (step.probability >= 100 || random.nextInt (100) < step.probability))
            {
                int pitch = midiNote (step.octave, step.pitchClass);
                if (tp.scaleOn)
                    pitch = quantizeToScale (mask, g.root, pitch);
                if (pitch >= 0 && pitch <= 127)
                {
                    releaseSameNote (channel, pitch, s, out);
                    out.addEvent (juce::MidiMessage::noteOn (channel, pitch, (juce::uint8) juce::jlimit (1, 127, (int) step.velocity)), s);
                    const double len = juce::jmax (stepLen * juce::jlimit (1, 100, (int) step.gate) / 100.0, ppqPerSample * 2.0);
                    addPendingOff (tn + len, channel, pitch);
                }
            }

            if (route.ccOut && ! g.ccSmoothing)
            {
                out.addEvent (juce::MidiMessage::controllerEvent (channel, route.ccNumber, step.cc), s);
                lastCCSent[(size_t) t] = step.cc;
            }
        }
    }
}

void Engine::renderCCSmoothing (const Project& project, int patternIndex, double a, double b, double blockStartPpq,
                                double ppqPerSample, int numSamples, juce::MidiBuffer& out)
{
    const auto& g = project.globals;
    if (! g.ccSmoothing || b <= a)
        return;

    const auto& pattern = project.patterns[(size_t) patternIndex];
    constexpr int resolution = 32;   // samples between CC updates

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& route = project.routing[(size_t) t];
        if (! route.ccOut)
            continue;

        const auto& tp = pattern.tracks[(size_t) t];
        const double stepLen = divisionInfo (tp.division).stepPpq;
        const int length = juce::jlimit (minLength, maxSteps, (int) tp.length);
        const int searchRange = 2 * length + 2;
        const int channel = juce::jlimit (1, 16, (int) route.midiChannel);

        auto stepAt = [&] (long long n) -> const Step& { return tp.steps[(size_t) stepIndexFor (n, length, tp.direction)]; };
        auto timeOf = [&] (long long n) { return (double) n * stepLen + swingOffset (n, stepLen, tp.swing); };

        auto valueAt = [&] (double x) -> int
        {
            const auto base = (long long) std::floor (x / stepLen);
            long long n0 = 0;
            bool found0 = false;
            for (long long n = base + 1; n >= base - searchRange; --n)
                if (timeOf (n) <= x + 1.0e-9 && stepAt (n).on) { n0 = n; found0 = true; break; }
            if (! found0)
                return -1;
            long long n1 = 0;
            bool found1 = false;
            for (long long n = base - 1; n <= base + searchRange; ++n)
                if (timeOf (n) > x + 1.0e-9 && stepAt (n).on) { n1 = n; found1 = true; break; }
            const int v0 = stepAt (n0).cc;
            if (! found1)
                return v0;
            const int v1 = stepAt (n1).cc;
            const double t0 = timeOf (n0), t1 = timeOf (n1);
            const double glide = (t1 - t0) * juce::jlimit (1, 100, g.ccSmoothingTime) / 100.0;
            const double glideStart = t1 - glide;
            if (x <= glideStart || glide <= 0.0)
                return v0;
            const double f = (x - glideStart) / glide;
            return juce::jlimit (0, 127, (int) std::lround (v0 + (v1 - v0) * f));
        };

        const int s0 = juce::jlimit (0, numSamples - 1, (int) std::ceil ((a - blockStartPpq) / ppqPerSample - 1.0e-6));
        const int s1 = juce::jlimit (0, numSamples, (int) std::ceil ((b - blockStartPpq) / ppqPerSample - 1.0e-6));
        for (int s = s0; s < s1; s += resolution)
        {
            const int v = valueAt (blockStartPpq + s * ppqPerSample);
            if (v >= 0 && v != lastCCSent[(size_t) t])
            {
                out.addEvent (juce::MidiMessage::controllerEvent (channel, route.ccNumber, v), s);
                lastCCSent[(size_t) t] = v;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
void Engine::process (Project& project, const Host& host, int numSamples, const juce::MidiBuffer& midiIn, juce::MidiBuffer& out)
{
    if (numSamples <= 0)
        return;

    auto& g = project.globals;

    if (! host.playing)
    {
        if (wasPlaying)
            flushAllNotes (out, 0);
        wasPlaying = false;
        lastPpqEnd = -1.0;
        chainActive = false;
        lastCCSent.fill (-1);
        for (auto& s : playStep) s = -1;
        isPlaying = false;
        // While stopped, a queued pattern switches immediately (nothing is playing)
        const int q = queuedPattern.exchange (-1);
        if (q >= 0 && q < numPatterns)
            project.currentPattern = q;
        return;
    }

    isPlaying = true;
    const double ppqPerSample = (host.bpm / 60.0) / sampleRate;
    const double ppq = host.ppq;
    const double ppqEnd = ppq + numSamples * ppqPerSample;

    const bool jumped = wasPlaying && lastPpqEnd >= 0.0 && std::abs (ppq - lastPpqEnd) > 0.02;
    if (jumped)
    {
        flushAllNotes (out, 0);
        lastCCSent.fill (-1);
    }

    const bool chainEnabled = g.chainOn && ! g.chain.isEmpty();
    if (! wasPlaying)
    {
        // Transport start: a chain starts from its first entry
        chainActive = chainEnabled;
        if (chainActive)
        {
            chainIndex = 0;
            project.currentPattern = juce::jlimit (0, numPatterns - 1, (int) g.chain.getReference (0).pattern);
            chainEntryStart = ppq;
        }
    }
    else if (jumped && chainActive)
    {
        // Loop or locate while playing: the current chain entry starts again from here
        chainEntryStart = ppq;
    }
    else if (chainEnabled != chainActive)
    {
        // Chain switched on or off while playing
        if (chainEnabled)
        {
            chainActive = true;
            chainIndex = 0;
            queuedPattern = (int) g.chain.getReference (0).pattern;
        }
        else
        {
            chainActive = false;
        }
    }
    if (chainActive)
        chainIndex = juce::jlimit (0, g.chain.size() - 1, chainIndex);

    // Pattern change from incoming CC
    int ccSample = -1;
    if (g.patternCC)
    {
        for (const auto m : midiIn)
        {
            const auto& msg = m.getMessage();
            if (msg.isController() && msg.getChannel() == g.patternCCChannel && msg.getControllerNumber() == g.patternCCNumber)
            {
                const int target = msg.getControllerValue();
                if (target < numPatterns)
                {
                    queuedPattern = target;
                    ccSample = m.samplePosition;
                }
            }
        }
    }

    double a = ppq;
    for (int guard = 0; guard < 8 && a < ppqEnd; ++guard)
    {
        double switchAt = ppqEnd;
        int target = -1;
        bool fromChain = false;

        const int q = queuedPattern.load();
        if (q >= 0 && q < numPatterns)
        {
            double t;
            if (g.quantize == Quantize::off)
                t = (ccSample >= 0) ? ppq + ccSample * ppqPerSample : a;
            else
                t = nextQuantizePoint (project, host, a);
            if (t < ppqEnd)
            {
                switchAt = juce::jmax (t, a);
                target = q;
            }
        }
        else if (chainActive)
        {
            const double end = chainEntryEnd (project);
            if (end < ppqEnd)
            {
                switchAt = juce::jmax (end, a);
                fromChain = true;
            }
        }

        renderSegment (project, project.currentPattern, a, switchAt, ppq, ppqPerSample, numSamples, out);
        renderCCSmoothing (project, project.currentPattern, a, switchAt, ppq, ppqPerSample, numSamples, out);
        emitDueNoteOffs (switchAt, ppq, ppqPerSample, numSamples, out);

        if (switchAt >= ppqEnd)
            break;

        const int switchSample = juce::jlimit (0, numSamples - 1, (int) std::floor ((switchAt - ppq) / ppqPerSample + 1.0e-6));
        if (fromChain)
        {
            int next = chainIndex + 1;
            if (next >= g.chain.size())
            {
                if (g.chainLoop) next = 0;
                else { chainActive = false; next = g.chain.size() - 1; }
            }
            if (chainActive)
            {
                chainIndex = next;
                project.currentPattern = juce::jlimit (0, numPatterns - 1, (int) g.chain.getReference (chainIndex).pattern);
                chainEntryStart = switchAt;
            }
        }
        else
        {
            int expected = target;
            queuedPattern.compare_exchange_strong (expected, -1);
            project.currentPattern = target;
            if (chainActive)
                chainEntryStart = switchAt;
            if (g.quantize == Quantize::off)
                flushAllNotes (out, switchSample);
        }
        a = switchAt;
    }

    if (chainActive && ! g.chain.isEmpty())
    {
        const double len = juce::jmax (1.0e-6, project.patterns[(size_t) project.currentPattern].lengthPpq());
        const auto& entry = g.chain.getReference (chainIndex);
        const double elapsed = ppqEnd - chainEntryStart;
        chainPosition = chainIndex;
        chainRepeat = juce::jlimit (0, (int) entry.repeats - 1, (int) std::floor (elapsed / len));
        chainEntryProgress = (float) juce::jlimit (0.0, 1.0, elapsed / (len * juce::jmax (1, (int) entry.repeats)));
    }
    else
    {
        chainPosition = -1;
        chainEntryProgress = 0.0f;
    }

    wasPlaying = true;
    lastPpqEnd = ppqEnd;
}
} // namespace vima
