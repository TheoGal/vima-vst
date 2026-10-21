# VIMA

VIMA (from Greek "βήμα", step) is a 16-track MIDI step sequencer plugin, built with JUCE.
It loads as an instrument (VST3) and sends MIDI notes and CC to other tracks and plugins.

The full design is in [SPEC.md](SPEC.md).

## Status

**Milestone 1: MIDI routing test.** The current build is not the sequencer yet. While the
host plays, it sends a fixed test pattern so the DAW routing can be checked:

| Channel | Note | Rhythm |
|---|---|---|
| 1 | C1 (36) | every beat |
| 2 | F#1 (42) | off-beat 8ths |
| 3 | C2 (48) | every 16th |
| 4 | C3 (60) | every bar |

Every channel also sends a CC 74 sweep on each 16th. Notes and CC can be switched off per channel.

### Checking the routing in Ableton Live

1. Put VIMA on a MIDI track.
2. Create a second MIDI track with a synth or a MIDI monitor. Set **MIDI From** to the VIMA track,
   the lower chooser to **Vima**, and Monitor to **In**.
3. Press play. Check which channels arrive: are channels 1-4 kept separate, or merged?
4. Try a VST3 instrument on the receiving track as well as a MIDI monitor.

## Download

Every push builds Windows (x64) and macOS (universal) VST3 zips. Open the latest run on the
[Actions page](../../actions), and download **Vima-Windows-x64** or **Vima-macOS-universal**.

Windows: copy `Vima.vst3` to `C:\Program Files\Common Files\VST3\` and rescan plugins.

## Building

```
cmake -S . -B build
cmake --build build --config Release --target Vima_VST3
```

JUCE 8.0.8 is downloaded automatically. Use `-DJUCE_DIR=/path/to/JUCE` for a local checkout.
Tests: configure with `-DVIMA_BUILD_TESTS=ON`, build `VimaTests`, run it (it writes UI screenshots).

## License

AGPL-3.0, see [LICENSE](LICENSE). IBM Plex fonts: SIL Open Font License, see
`Resources/fonts/OFL-IBMPlex.txt`.
