// VIMA test harness: runs the processor offline against a fake host play head,
// checks the MIDI it produces, and renders the editor to PNG files.
// Build with -DVIMA_BUILD_TESTS=ON, run: VimaTests [output-dir]

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <map>

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
        if (! ok) ++failures;
    }

    struct FakePlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0, ppq = 0.0;
        bool playing = true;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    struct Run
    {
        std::map<int, int> noteOns, noteOffs, ccs;
        int maxSamplePos = 0;
    };

    Run runBars (VimaProcessor& proc, FakePlayHead& head, double bars, double sampleRate = 48000.0, int block = 512)
    {
        Run r;
        juce::AudioBuffer<float> audio (2, block);
        juce::MidiBuffer midi;
        const double ppqPerSample = head.bpm / 60.0 / sampleRate;
        const double startPpq = head.ppq;
        const auto numBlocks = (int) std::llround (bars * 4.0 / (block * ppqPerSample));
        for (int b = 0; b < numBlocks; ++b)
        {
            head.ppq = startPpq + (double) b * block * ppqPerSample;
            midi.clear();
            proc.processBlock (audio, midi);
            for (const auto m : midi)
            {
                const auto& msg = m.getMessage();
                r.maxSamplePos = juce::jmax (r.maxSamplePos, m.samplePosition);
                if (msg.isNoteOn()) r.noteOns[msg.getChannel()]++;
                else if (msg.isNoteOff()) r.noteOffs[msg.getChannel()]++;
                else if (msg.isController()) r.ccs[msg.getChannel()]++;
            }
        }
        head.ppq = startPpq + (double) numBlocks * block * ppqPerSample;
        return r;
    }

    void renderEditor (VimaProcessor& proc, const juce::File& file)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "  wrote " << file.getFullPathName() << std::endl;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    std::cout << "MIDI routing test pattern" << std::endl;
    {
        VimaProcessor proc;
        FakePlayHead head;
        proc.setPlayHead (&head);
        proc.prepareToPlay (48000.0, 512);

        auto r = runBars (proc, head, 2.0);
        check (r.noteOns[1] == 8, "channel 1 plays every beat (8 in 2 bars), got " + juce::String (r.noteOns[1]));
        check (r.noteOns[2] == 8, "channel 2 plays off-beat 8ths (8 in 2 bars), got " + juce::String (r.noteOns[2]));
        check (r.noteOns[3] == 32, "channel 3 plays every 16th (32 in 2 bars), got " + juce::String (r.noteOns[3]));
        check (r.noteOns[4] == 2, "channel 4 plays every bar (2 in 2 bars), got " + juce::String (r.noteOns[4]));
        check (r.ccs[1] == 32 && r.ccs[4] == 32, "CC 74 sent on every 16th per channel");
        check (r.noteOns.count (5) == 0, "nothing on channel 5");
        check (r.maxSamplePos < 512, "all events inside their block");

        // stop: hanging notes are released
        head.playing = false;
        auto stopRun = runBars (proc, head, 0.25);
        int totalOn = 0, totalOff = 0;
        for (auto& [ch, n] : r.noteOns) totalOn += n;
        for (auto& [ch, n] : r.noteOffs) totalOff += n;
        for (auto& [ch, n] : stopRun.noteOffs) totalOff += n;
        check (totalOn == totalOff, "every note-on has a note-off after stop (" + juce::String (totalOn) + " / " + juce::String (totalOff) + ")");

        proc.notesEnabled[2] = false;
        head.playing = true;
        head.ppq = 0.0;
        auto r2 = runBars (proc, head, 1.0);
        check (r2.noteOns.count (3) == 0, "disabled channel 3 sends no notes");

        renderEditor (proc, outDir.getChildFile ("editor.png"));
        proc.setPlayHead (nullptr);
    }

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
