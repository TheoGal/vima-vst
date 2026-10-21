# VIMA

VIMA (from Greek "βήμα", step) is a 16-track MIDI step sequencer plugin, built with JUCE.
It loads as an instrument (VST3) and sends MIDI notes and CC to other tracks and plugins.

The full design is in [SPEC.md](SPEC.md).

## Features

- 16 tracks, each on its own MIDI channel, with independent note and CC outputs
- Up to 32 steps per track: note, octave, velocity, gate, probability and a CC value per step
- Per track: length, division (with triplets), direction, swing, scale on/off
- 14 scales plus a user scale, picked in the main window
- LED bar per row: playhead and drag selection
- Selection by drag, plus Ctrl-click / Ctrl-drag for non-adjacent steps
- Step operations on the selection (or all steps): copy / paste across tracks (keeps spacing), randomize with
  lane ranges, smooth, shift all (with capping)
- 192 patterns in 12 banks, quick picker in the header, patterns view with thumbnails,
  chain with repeats and drag-to-reorder, quantized pattern changes, pattern change by incoming CC
- CC smoothing between steps, undo, dark and light themes, resizable window (75% to 200%)
- Follows the host transport

## Download

Every push builds Windows (x64) and macOS (universal) VST3 zips. Open the latest run on the
[Actions page](../../actions), and download **Vima-Windows-x64** or **Vima-macOS-universal**.

Windows: copy `Vima.vst3` to `C:\Program Files\Common Files\VST3\` and rescan plugins.

## Using it in Ableton Live

1. Put VIMA on a MIDI track.
2. On each track you want to play, set **MIDI From** to the VIMA track, the lower chooser to
   **Vima**, and Monitor to **In**. Choose the MIDI channel of the VIMA track you want there.
3. Press play: VIMA plays while the host plays.

## Building

```
cmake -S . -B build
cmake --build build --config Release --target Vima_VST3
```

JUCE 8.0.8 is downloaded automatically. Use `-DJUCE_DIR=/path/to/JUCE` for a local checkout.
Tests: configure with `-DVIMA_BUILD_TESTS=ON`, build `VimaTests`, run it. It runs the engine
checks offline and writes screenshots of every screen.

## License

AGPL-3.0, see [LICENSE](LICENSE). IBM Plex fonts: SIL Open Font License, see
`Resources/fonts/OFL-IBMPlex.txt`.
