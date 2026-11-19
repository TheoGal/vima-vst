#include "Engine.h"
#include <cmath>

namespace vima
{
Engine::Engine()
{
    for (auto& s : playStep) s = -1;
    for (auto& s : shownTranspose) s = 0;
    lastCCSent.fill (-1);
    trackFrom.fill (-1.0e18);
    trackTo.fill (1.0e18);
    random.setSeedRandomly();
    ccRollSeed = (std::uint32_t) random.nextInt();
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
    chainFinished = chainJoinPending = inserting = false;
    insertPattern = -1;
    insertPlaying = false;
    lastChainOnState = -1;
    for (auto& s : playStep) s = -1;
    isPlaying = false;
    beatsToSwitch = -1.0;
    numHeld.fill (0);
    transpose.fill (0); targetTranspose.fill (0); pendTranspose.fill (0); pendAt.fill (1.0e18);
    gating = false;
    for (auto& s : shownTranspose) s = 0;
    trigRunning = trigUsesDaw = false;
    trackOn.fill (false); startPending.fill (false); stopPending.fill (false);
    trackFrom.fill (-1.0e18); trackTo.fill (1.0e18); trackOrigin.fill (0.0);
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

        // the track's play window and its step 1 time (both open while Vima simply runs; set by the note trigger)
        const double o = trackOrigin[(size_t) t];
        const double la = juce::jmax (a, trackFrom[(size_t) t]) - o, lb = juce::jmin (b, trackTo[(size_t) t]) - o;
        if (lb <= la)
            continue;
        const auto nFirst = (long long) std::floor ((la - 0.5 * stepLen) / stepLen) - 1;
        const auto nLast = (long long) std::floor (lb / stepLen) + 1;

        for (auto n = nFirst; n <= nLast; ++n)
        {
            const double tl = (double) n * stepLen + swingOffset (n, stepLen, tp.swing);
            if (tl < la - 1.0e-9 || tl >= lb - 1.0e-9)
                continue;
            const double tn = tl + o;

            const int idx = stepIndexFor (n, length, tp.direction);
            playStep[(size_t) t] = idx;

            const auto& step = tp.steps[(size_t) idx];
            if (! step.on && ! step.ccOn)
                continue;

            const int s = sampleFor (tn);
            const int channel = juce::jlimit (1, 16, (int) route.midiChannel);

            // notes follow the note on/off, CC values the CC on/off (SPEC.md 5)
            if (step.on && route.noteOut && (step.probability >= 100 || random.nextInt (100) < step.probability))
            {
                int pitch = midiNote (step.octave, step.pitchClass);
                if (tp.scaleOn)
                    pitch = quantizeToScale (mask, g.root, pitch);
                const int k = tn >= pendAt[(size_t) t] - 1.0e-9 ? pendTranspose[(size_t) t] : transpose[(size_t) t];
                pitch = transposeNote (pitch, k, tp.scaleOn, mask, g.root);   // NOTE INPUT (quantized)
                if (pitch >= 0 && pitch <= 127)
                {
                    releaseSameNote (channel, pitch, s, out);
                    out.addEvent (juce::MidiMessage::noteOn (channel, pitch, (juce::uint8) juce::jlimit (1, 127, (int) step.velocity)), s);
                    const double len = juce::jmax (stepLen * juce::jlimit (1, maxGate, (int) step.gate) / 100.0, ppqPerSample * 2.0);
                    addPendingOff (tn + len, channel, pitch);
                }
            }

            if (step.ccOn && route.ccOut && ! g.ccSmoothing && ccRollPasses (step, t, n))
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

        const double o = trackOrigin[(size_t) t];
        const double ta = juce::jmax (a, trackFrom[(size_t) t]), tb = juce::jmin (b, trackTo[(size_t) t]);
        if (tb <= ta)
            continue;
        auto stepAt = [&] (long long n) -> const Step& { return tp.steps[(size_t) stepIndexFor (n, length, tp.direction)]; };
        auto timeOf = [&] (long long n) { return o + (double) n * stepLen + swingOffset (n, stepLen, tp.swing); };
        auto ccPlays = [&] (long long n) { const auto& st = stepAt (n); return st.ccOn && ccRollPasses (st, t, n); };

        auto valueAt = [&] (double x) -> int
        {
            const auto base = (long long) std::floor ((x - o) / stepLen);
            long long n0 = 0;
            bool found0 = false;
            for (long long n = base + 1; n >= base - searchRange; --n)
                if (timeOf (n) <= x + 1.0e-9 && ccPlays (n)) { n0 = n; found0 = true; break; }
            if (! found0)
                return -1;
            long long n1 = 0;
            bool found1 = false;
            for (long long n = base - 1; n <= base + searchRange; ++n)
                if (timeOf (n) > x + 1.0e-9 && ccPlays (n)) { n1 = n; found1 = true; break; }
            const int v0 = stepAt (n0).cc;
            if (! found1)
                return v0;
            const int v1 = stepAt (n1).cc;
            const double t0 = timeOf (n0), t1 = timeOf (n1);
            const double glide = t1 - t0;   // the whole gap
            if (x <= t0 || glide <= 0.0)
                return v0;
            double f = (x - t0) / glide;
            if (g.ccCurve == CCCurve::exp) f = f * f * f;                             // slow start, fast arrival
            else if (g.ccCurve == CCCurve::log) f = 1.0 - (1.0 - f) * (1.0 - f) * (1.0 - f);   // fast start, slow arrival
            return juce::jlimit (0, 127, (int) std::lround (v0 + (v1 - v0) * f));
        };

        const int s0 = juce::jlimit (0, numSamples - 1, (int) std::ceil ((ta - blockStartPpq) / ppqPerSample - 1.0e-6));
        const int s1 = juce::jlimit (0, numSamples, (int) std::ceil ((tb - blockStartPpq) / ppqPerSample - 1.0e-6));
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
// Note trigger and MIDI thru (SPEC.md 3.1)
int Engine::scopeOf (const Project& project, int track) const
{
    const auto& g = project.globals;
    return g.noteTrigger != NoteTrigger::off && ((g.triggerTracks >> track) & 1) ? 0 : -1;   // one scope: the chosen tracks
}

// One SHIFT step per semitone from the base note: a scale step on tracks using the scale, else a semitone
int Engine::transposeNote (int pitch, int steps, bool inScaleTrack, std::uint16_t mask, int root)
{
    if (steps == 0)
        return pitch;
    if (! inScaleTrack || (mask & 0x0FFF) == 0)
        return pitch + steps;
    const int dir = steps > 0 ? 1 : -1;
    for (int k = 0; k < std::abs (steps); ++k)
    {
        int guard = 0;
        do { pitch += dir; } while (! inScale (mask, root, ((pitch % 12) + 12) % 12) && ++guard < 12);
    }
    return pitch;
}

void Engine::passThru (const Project& project, const juce::MidiBuffer& midiIn, juce::MidiBuffer& out) const
{
    const auto& g = project.globals;
    if (! g.midiThru || g.noteTrigger != NoteTrigger::off)
        return;   // the note trigger uses every incoming note: nothing passes
    for (const auto m : midiIn)
    {
        const auto& msg = m.getMessage();
        if (g.patternCC && msg.isController() && msg.getChannel() == g.patternCCChannel && msg.getControllerNumber() == g.patternCCNumber)
            continue;   // pattern changes are always consumed
        out.addEvent (msg, m.samplePosition);
    }
}

void Engine::readTriggerNotes (const Project& project, const juce::MidiBuffer& midiIn)
{
    const auto& g = project.globals;
    pressSample.fill (-1);
    if (g.noteTrigger == NoteTrigger::off)
        numHeld.fill (0);   // OFF forgets the held keys; TRANSPOSE <-> TRIGGER keeps them
    lastTriggerMode = g.noteTrigger;
    if (g.noteTrigger != NoteTrigger::off)
    {
        for (const auto m : midiIn)
        {
            const auto& msg = m.getMessage();
            if (! msg.isNoteOnOrOff())
                continue;
            const int s = 0;   // one scope: every note, any channel
            auto& h = held[(size_t) s];
            auto& n = numHeld[(size_t) s];
            const int note = msg.getNoteNumber();
            for (int i = 0; i < n; ++i)
                if (h[(size_t) i] == note)
                {
                    for (int j = i; j < n - 1; ++j) h[(size_t) j] = h[(size_t) j + 1];
                    --n;
                    break;
                }
            if (msg.isNoteOn())
            {
                if (n == maxHeld)
                {
                    for (int j = 0; j < n - 1; ++j) h[(size_t) j] = h[(size_t) j + 1];
                    --n;
                }
                h[(size_t) n++] = note;   // the newest key wins
                if (pressSample[(size_t) s] < 0)
                    pressSample[(size_t) s] = m.samplePosition;
            }
        }
    }
    const int base = g.baseNote();
    for (int t = 0; t < numTracks; ++t)
    {
        const int s = scopeOf (project, t);
        targetTranspose[(size_t) t] = (s >= 0 && numHeld[(size_t) s] > 0) ? held[(size_t) s][(size_t) numHeld[(size_t) s] - 1] - base : 0;
    }
}

// Before rendering a block of a trigger run: start and stop tracks as their keys go down and up
void Engine::updateTriggerTracks (const Project& project, const Host& host, double a, double ppqPerSample, bool openUnchosen)
{
    const auto& g = project.globals;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto i = (size_t) t;
        const int s = scopeOf (project, t);
        if (s < 0)
        {
            // unchosen tracks: play as usual while Vima runs, silent in a run the keys started
            trackOn[i] = startPending[i] = stopPending[i] = false;
            trackFrom[i] = openUnchosen ? -1.0e18 : 1.0e18;
            trackTo[i] = 1.0e18;
            trackOrigin[i] = 0.0;
            continue;
        }
        const bool keys = s >= 0 && numHeld[(size_t) s] > 0;
        if (! trackOn[i] && ! startPending[i] && keys)
        {
            // starts on the trigger's Quantize setting, from step 1; from silence (the run's first block) at once
            double at = triggerQuantizePoint (g, host, a);
            if (g.triggerQuantize == TriggerQuantize::off || (! wasPlaying && ! trigUsesDaw))
                at = a + juce::jmax (0, pressSample[(size_t) s]) * ppqPerSample;
            startPending[i] = true;
            trackFrom[i] = at;
            trackTo[i] = 1.0e18;
            trackOrigin[i] = at;
            transpose[i] = targetTranspose[i];   // it starts with the key's transposition
            pendAt[i] = 1.0e18;
        }
        else if (startPending[i] && ! keys)
        {
            startPending[i] = false;
            trackFrom[i] = 1.0e18;
        }
        if (trackOn[i] && ! stopPending[i] && ! keys)
        {
            // stops on the trigger's Quantize setting
            const double at = triggerQuantizePoint (g, host, a);
            stopPending[i] = true;
            trackTo[i] = at;
        }
        else if (stopPending[i] && keys)
        {
            stopPending[i] = false;
            trackTo[i] = 1.0e18;
        }
    }
}

double Engine::triggerQuantizePoint (const Globals& g, const Host& host, double ppq)
{
    const double beat = 4.0 / juce::jmax (1, host.timeSigDenominator);
    double unit = 0.0;
    switch (g.triggerQuantize)
    {
        case TriggerQuantize::off:  return ppq;
        case TriggerQuantize::q32:  unit = 0.125; break;
        case TriggerQuantize::q16:  unit = 0.25; break;
        case TriggerQuantize::q8:   unit = 0.5; break;
        case TriggerQuantize::beat: unit = beat; break;
        case TriggerQuantize::bar:  unit = beat * juce::jmax (1, host.timeSigNumerator); break;
    }
    return std::ceil ((ppq - 1.0e-9) / unit) * unit;
}

// A new transposition starts at the first step on or after the next Quantize point (OFF: the next step)
void Engine::scheduleTranspose (const Project& project, const Host& host, double a, bool running)
{
    for (int t = 0; t < numTracks; ++t)
    {
        const auto i = (size_t) t;
        const int planned = pendAt[i] < 1.0e17 ? pendTranspose[i] : transpose[i];
        if (! running)
        {
            transpose[i] = targetTranspose[i];
            pendAt[i] = 1.0e18;
        }
        else if (targetTranspose[i] != planned)
        {
            pendTranspose[i] = targetTranspose[i];
            pendAt[i] = triggerQuantizePoint (project.globals, host, a);
        }
        shownTranspose[i] = pendAt[i] < 1.0e17 ? pendTranspose[i] : transpose[i];
    }
}

void Engine::commitTranspose (double ppqEnd)
{
    for (int t = 0; t < numTracks; ++t)
        if (pendAt[(size_t) t] < ppqEnd)
        {
            transpose[(size_t) t] = pendTranspose[(size_t) t];
            pendAt[(size_t) t] = 1.0e18;
        }
}

void Engine::finishTriggerBlock (double ppqEnd)
{
    bool anyTrack = false;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto i = (size_t) t;
        if (startPending[i] && trackFrom[i] < ppqEnd)
        {
            startPending[i] = false;
            trackOn[i] = true;
        }
        if (stopPending[i] && trackTo[i] < ppqEnd)
        {
            stopPending[i] = false;
            trackOn[i] = false;
            trackFrom[i] = trackTo[i] = 1.0e18;
            playStep[i] = -1;
        }
        anyTrack = anyTrack || trackOn[i] || startPending[i];
    }
    bool anyKeys = false;
    for (int s = 0; s < numScopes; ++s) anyKeys = anyKeys || numHeld[(size_t) s] > 0;
    if (! anyTrack && ! anyKeys)
        trigRunning = false;   // the next block stops (all notes released)
    if (trigRunning && ! trigUsesDaw)
        trigClock = ppqEnd;
}

// ---------------------------------------------------------------------------------------
void Engine::process (Project& project, const Host& hostIn, int numSamples, const juce::MidiBuffer& midiIn, juce::MidiBuffer& out)
{
    if (numSamples <= 0)
        return;

    auto& g = project.globals;
    passThru (project, midiIn, out);
    readTriggerNotes (project, midiIn);

    // NOTE INPUT (SPEC.md 3.1). TRANSPOSE: keys only transpose. TRIGGER: the chosen tracks play only
    // while keys are held: while Vima runs anyway they are gated; while it is stopped a key press starts
    // a run of its own (on the DAW's clock if the DAW rolls with Vima held, else on Vima's own clock).
    Host host = hostIn;
    bool trigMode = false;
    const bool resetWindows = trackFrom[0] != -1.0e18 || trigRunning || gating;
    if (g.noteTrigger != NoteTrigger::trigger)
    {
        if (resetWindows)
        {
            trigRunning = gating = false;
            trackOn.fill (false); startPending.fill (false); stopPending.fill (false);
            trackFrom.fill (-1.0e18); trackTo.fill (1.0e18); trackOrigin.fill (0.0);
        }
    }
    else if (host.playing)
    {
        if (! gating)
        {
            // Vima (re)started by the DAW or the play button: the chosen tracks wait for keys
            gating = true;
            trigRunning = false;
            trackOn.fill (false); startPending.fill (false); stopPending.fill (false);
            trackFrom.fill (1.0e18); trackTo.fill (1.0e18); trackOrigin.fill (0.0);
        }
        trigMode = true;
    }
    else
    {
        if (gating)
        {
            // the DAW stopped: tracks the keys still hold carry on, on Vima's own clock
            gating = false;
            bool any = false;
            for (int t = 0; t < numTracks; ++t) any = any || trackOn[(size_t) t] || startPending[(size_t) t];
            trigRunning = any;
            trigUsesDaw = false;
            trigClock = lastPpqEnd >= 0.0 ? lastPpqEnd : 0.0;
        }
        bool press = false;
        for (int t = 0; t < numTracks; ++t)
        {
            const int s = scopeOf (project, t);
            press = press || (s >= 0 && pressSample[(size_t) s] >= 0);
        }
        if (! trigRunning && press)
        {
            trigRunning = true;
            trigUsesDaw = hostIn.dawPlaying;
            trigClock = 0.0;
            trackOn.fill (false); startPending.fill (false); stopPending.fill (false);
            trackFrom.fill (1.0e18); trackTo.fill (1.0e18); trackOrigin.fill (0.0);
        }
        if (trigRunning)
        {
            if (trigUsesDaw && ! hostIn.dawPlaying)
            {
                trigUsesDaw = false;   // the DAW stopped: carry on with Vima's own clock
                trigClock = lastPpqEnd >= 0.0 ? lastPpqEnd : 0.0;
            }
            host.playing = true;
            if (! trigUsesDaw)
                host.ppq = trigClock;
            trigMode = true;
        }
    }

    if (! host.playing)
    {
        scheduleTranspose (project, host, 0.0, false);
        if (wasPlaying)
            flushAllNotes (out, 0);
        wasPlaying = false;
        lastPpqEnd = -1.0;
        chainActive = false;
        chainFinished = chainJoinPending = inserting = false;
        insertPattern = -1;
        insertPlaying = false;
        lastCCSent.fill (-1);
        for (auto& s : playStep) s = -1;
        isPlaying = false;
        beatsToSwitch = -1.0;
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
    if (trigMode)
        updateTriggerTracks (project, host, ppq, ppqPerSample, gating);
    scheduleTranspose (project, host, ppq, true);

    const bool jumped = wasPlaying && lastPpqEnd >= 0.0 && std::abs (ppq - lastPpqEnd) > 0.02;
    if (jumped)
    {
        flushAllNotes (out, 0);
        lastCCSent.fill (-1);
    }

    const bool chainEnabled = g.chainOn && ! g.chain.isEmpty();
    if (! wasPlaying)
    {
        // Transport start: a chain starts from its first entry; fresh CC probability rolls
        ccRollSeed = (std::uint32_t) random.nextInt();
        chainActive = chainEnabled;
        chainFinished = chainJoinPending = inserting = false;
        if (chainActive)
        {
            chainIndex = 0;
            project.currentPattern = juce::jlimit (0, numPatterns - 1, (int) g.chain.getReference (0).pattern);
            chainEntryStart = ppq;
        }
    }
    else if (jumped && chainActive)
    {
        // Loop or locate while playing: the current chain entry (or insertion) starts again from here
        chainEntryStart = ppq;
        insertStart = ppq;
    }
    else if (! chainEnabled)
    {
        // Chain switched off while playing
        chainActive = chainFinished = chainJoinPending = inserting = false;
    }
    else if (! chainActive && ! chainFinished)
    {
        // Chain switched on while playing: its first entry is queued (quantize applies)
        chainActive = true;
        chainJoinPending = true;
        inserting = false;
        chainIndex = 0;
        queuedPattern = (int) g.chain.getReference (0).pattern;
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
        const bool picked = q >= 0 && q < numPatterns;
        const bool chainRuns = chainActive && ! chainJoinPending;
        if (chainRuns)
        {
            // The chain plays: a hand pick waits for the end of the current entry (or insertion)
            // and then plays once as an insertion (SPEC.md 7.2)
            const double end = inserting ? insertStart + juce::jmax (1.0e-6, project.patterns[(size_t) project.currentPattern].lengthPpq())
                                         : chainEntryEnd (project);
            if (end < ppqEnd)
            {
                switchAt = juce::jmax (end, a);
                fromChain = true;
            }
        }
        else if (picked)
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

        renderSegment (project, project.currentPattern, a, switchAt, ppq, ppqPerSample, numSamples, out);
        renderCCSmoothing (project, project.currentPattern, a, switchAt, ppq, ppqPerSample, numSamples, out);
        emitDueNoteOffs (switchAt, ppq, ppqPerSample, numSamples, out);

        if (switchAt >= ppqEnd)
            break;

        const int switchSample = juce::jlimit (0, numSamples - 1, (int) std::floor ((switchAt - ppq) / ppqPerSample + 1.0e-6));
        if (fromChain && picked)
        {
            // an insertion starts: the picked pattern plays once, the chain waits after its entry
            int expected = q;
            queuedPattern.compare_exchange_strong (expected, -1);
            project.currentPattern = q;
            inserting = true;
            insertStart = switchAt;
        }
        else if (fromChain)
        {
            inserting = false;
            int next = chainIndex + 1;
            if (next >= g.chain.size())
            {
                if (g.chainLoop) next = 0;
                else { chainActive = false; chainFinished = true; next = g.chain.size() - 1; }   // LOOP OFF: keep playing
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
            {
                // the chain's first entry joins (chain switched on while playing)
                chainEntryStart = switchAt;
                chainJoinPending = false;
                inserting = false;
            }
            if (g.quantize == Quantize::off)
                flushAllNotes (out, switchSample);
        }
        a = switchAt;
    }

    const int pending = queuedPattern.load();
    const bool chainRuns = chainActive && ! chainJoinPending && ! g.chain.isEmpty();
    const double curLen = juce::jmax (1.0e-6, project.patterns[(size_t) project.currentPattern].lengthPpq());
    if (chainRuns)
    {
        const auto& entry = g.chain.getReference (chainIndex);
        const double elapsed = ppqEnd - chainEntryStart;
        chainPosition = chainIndex;
        chainRepeat = inserting ? (int) entry.repeats - 1 : juce::jlimit (0, (int) entry.repeats - 1, (int) std::floor (elapsed / curLen));
        chainEntryProgress = inserting ? 1.0f : (float) juce::jlimit (0.0, 1.0, elapsed / (curLen * juce::jmax (1, (int) entry.repeats)));
        // the insertion row in the chain list: the playing insertion, else a waiting hand pick
        insertPlaying = inserting;
        insertPattern = inserting ? project.currentPattern : (pending >= 0 && pending < numPatterns ? pending : -1);
        insertProgress = inserting ? (float) juce::jlimit (0.0, 1.0, (ppqEnd - insertStart) / curLen) : 0.0f;
    }
    else
    {
        chainPosition = -1;
        chainEntryProgress = 0.0f;
        insertPattern = -1;
        insertPlaying = false;
        insertProgress = 0.0f;
    }

    // Next scheduled switch, for the switch blink (SPEC.md 7.5)
    {
        const double beat = 4.0 / juce::jmax (1, host.timeSigDenominator);
        double at = -1.0, playLen = 0.0;
        int next = -1;
        if (chainRuns)
        {
            at = inserting ? insertStart + curLen : chainEntryEnd (project);
            playLen = inserting ? curLen : curLen * juce::jmax (1, (int) g.chain.getReference (chainIndex).repeats);
            if (pending >= 0 && pending < numPatterns)
                next = pending;   // an insertion comes next
            else
            {
                int ni = chainIndex + 1;
                if (ni >= g.chain.size()) ni = g.chainLoop ? 0 : -1;
                if (ni >= 0) next = (int) g.chain.getReference (ni).pattern;
            }
        }
        else if (pending >= 0 && pending < numPatterns && g.quantize != Quantize::off)
        {
            at = nextQuantizePoint (project, host, ppqEnd);
            next = pending;
            playLen = curLen;
        }
        if (next >= 0 && next != project.currentPattern && playLen > switchWarnBeats * beat + 1.0e-9 && at >= ppqEnd)
            beatsToSwitch = (at - ppqEnd) / beat;
        else
            beatsToSwitch = -1.0;
    }

    wasPlaying = true;
    lastPpqEnd = ppqEnd;
    commitTranspose (ppqEnd);
    if (trigMode)
        finishTriggerBlock (ppqEnd);
}
} // namespace vima
