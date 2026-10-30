# work in progress - nothing to download yet, follow to be informed when the release is out

<p align="center">
<img src="docs/images/vima-icon-dark.png" width="150" alt="Vima logo">
</p>

# Vima

Vima (from Greek "βήμα", step) is an 8-track MIDI step sequencer plugin, built with JUCE.
It loads as an instrument (VST3) and sends MIDI notes and CC to other tracks and plugins.


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
