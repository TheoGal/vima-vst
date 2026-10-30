# work in progress - nothing to download yet, follow to be informed when the release is out

# VIMA

VIMA (from Greek "βήμα", step) is an 8-track MIDI step sequencer plugin, built with JUCE.
It loads as an instrument (VST3) and sends MIDI notes and CC to other tracks and plugins.

The full design is in [SPEC.md](SPEC.md).

## Features

- 8 tracks, each on its own MIDI channel, with independent note and CC outputs
- Up to 32 steps per track: note, octave, velocity, gate, probability and a CC value per step
- Per track: length, division (with triplets), direction, swing, scale on/off
- 13 scales plus a user scale, picked in the main window
- LED bar per row: playhead and drag selection
- Selection by drag, plus Ctrl-click / Ctrl-drag for non-adjacent steps
- Step operations on the selection (or all steps): copy / paste across tracks (keeps spacing), randomize with
  param ranges, smooth, shift (with capping)
- 64 patterns (pages of 16), quick picker in the header, patterns view with thumbnails,
  copy / paste of patterns and tracks, chain with repeats and drag-to-reorder, quantized pattern changes, pattern change by incoming CC
- CC smoothing between steps, undo, dark and light themes, resizable window (75% to 200%)
- Follows the host transport
- 128 macros for host automation: map any of them to pattern, chain, track settings or step values (each mapping fixed to its track, pattern and step), with LEARN

## Download

Every push builds Windows (x64) and macOS (universal) VST3 zips. Open the latest run on the
[Actions page](../../actions), and download **Vima-Windows-x64** or **Vima-macOS-universal**.

Windows: copy `Vima.vst3` to `C:\Program Files\Common Files\VST3\` and rescan plugins.

## Using it in Ableton Live

Live strips the MIDI channel from MIDI data internally, so all 8 tracks of one VIMA instance
arrive together on the destination track. In Live, use **one VIMA instance per destination track**.

1. Put VIMA on a MIDI track.
2. On the track you want to play, set **MIDI From** to the VIMA track, the lower chooser to
   **Vima**, and Monitor to **In**.
3. Press play: VIMA plays while the host plays.

Hosts that keep MIDI channels (for example Bitwig or Reaper) can route one instance to several
instruments by channel.

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
