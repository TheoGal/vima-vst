# VIMA - Design Spec

Name: **VIMA** (from Greek "βήμα", step). Repository: TheoGal/vima-vst.

Status: design phase. This file is the single source of truth for the build. Every design decision goes here; the code follows this file, not chat history.

Last updated: 2026-10-21

---

## 1. Identity

- A 16-track MIDI step sequencer plugin with its own design. It is NOT a clone of Zaquencer or any other product, and does not follow their behavior.
- Strong focus on randomization and quick, destructive "happy accident" editing, with undo as the safety net.

## 2. Plugin format and hosting

- Built with JUCE and CMake.
- Format: **VST3**. Windows is the primary target. macOS builds are produced by CI for testers (the developer has no Mac).
- The host sees it as an **instrument (synth) plugin**, not a MIDI effect.
- It produces no audio. It outputs MIDI only (notes and CC). A silent stereo output bus exists only so hosts accept it as an instrument.
- In the DAW, other tracks receive the plugin's MIDI output (for example in Ableton Live: "MIDI From" = the plugin's track, the plugin as input channel, monitor "In").

### Open: Ableton Live MIDI routing (pending tests by the user)

- Confirm Live accepts MIDI output from a VST3 instrument.
- Confirm whether Live keeps MIDI channels separate when routing plugin output to other tracks, or merges them. Ableton documents merging for track-to-track routing. If channels are merged, a workaround is needed (options discussed: per-instance track filter, a virtual MIDI port output, or accepting single-destination use in Live).

## 3. Transport and clock

- Follows the host only: plays when the host plays, stops when the host stops, follows host tempo and song position.
- No internal free-running clock.

## 4. Tracks

- 8 tracks, shown as "01" to "08". No track names.
- Selected with a **dropdown** in the header. No sidebar.
- Every track always has both a note part and a CC part. The NOTES / CC switch in the header only changes which part is shown and edited. It does not change what the track outputs; that is set by the NOTE OUT and CC OUT toggles.

Where track settings live (decided during the build):

- **Per track, per pattern** (stored in each pattern, so patterns can differ): length, division, direction, swing, scale on/off, and all steps.
- **Per track, global** (the same in every pattern, because they describe where the track is routed): MIDI channel, notes output on/off, CC output on/off, CC number.

Per track parameters:

| Parameter | Range / values | Notes |
|---|---|---|
| MIDI channel | 1-16 | Freely assignable per track. Track N is NOT tied to channel N (default may be N). |
| Notes output | on / off | Independent of CC output. |
| CC output | on / off | Independent of notes output. Sends on the same MIDI channel as the notes. |
| CC number | 0-127 | User-defined, one CC per track. |
| Length | 4-32 steps | |
| Division | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 and triplets 1/2T, 1/4T, 1/8T, 1/16T, 1/32T | |
| Direction | Forward, Backward, Ping-pong | |
| Swing | 0-100% | Per track. 0% = straight. Delays every second step; the exact curve is tuned during the build. |
| Scale | on / off | Per-track switch to use or bypass the global scale. |

## 5. Steps

Each track has 32 steps. Steps beyond the track length are shown dimmed and are not played.

Per step values:

| Value | Range | Notes |
|---|---|---|
| On / off | on / off | An off step sends nothing (no note, no CC) but still takes its time slot. It is not skipped. |
| Note | pitch class | Separate control from octave. |
| Octave | user-defined range (Settings) | |
| Velocity | 1-127 | Minimum is 1, never 0: in MIDI a Note On with velocity 0 is treated as a Note Off. Every operation (knob, Randomize, Smooth, Shift, ranges) clamps velocity to 1-127. |
| Gate | 1-100% | 100% = a note as long as one step at the track's division (for 1/16, a 1/16 note). |
| Probability | 0-100% | Chance that an "on" step actually plays each time it is reached. 100% = always. Applies to the note output (see open question about CC). |
| CC value | 0-127 | Used when the track's CC output is on. |

Pitch display and scale behavior:

- Every step always displays the note that will actually sound (for example "D#3").
- When the track's scale is on, the note knob can only land on notes in the scale.

There is no step skip, no chords, no drum mode. Note mode only.

## 6. Global settings

- Scale: 14 built-in scales plus 1 user scale, with a root note. Proposed list (editable): Major, Natural minor, Harmonic minor, Melodic minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major pentatonic, Minor pentatonic, Blues, Whole tone, Chromatic.
- The scale is **selected in the main window** (SCALE tile popover, see 12.2), never in the Settings dialog. The root can be set in the SCALE popover and also in the Settings user scale editor (same global value).
- User scale: 12 note toggles. It is **edited only in the Settings dialog**, and can only be **selected** (as "USER") from the main window.
- Pattern change from incoming CC: MIDI channel and CC number configurable.
- Pattern change quantization: OFF (instant, at the moment of the click), 1 beat, 1 bar or end of pattern.

## 7. Patterns

- 64 patterns, organized as 4 banks of 16. Every pattern can be reached by one incoming CC value.
- A pattern holds all 8 tracks.
- Copy a whole pattern, or a single track, to another slot.
- Pattern chaining.

### 7.1 Patterns view (proposed, board D, pending approval)

- Opened with the "OPEN PATTERNS VIEW" button in the header's pattern popover (7.4). It replaces the step view; a "STEPS" button in the header goes back. While open, the PATTERN box is highlighted and shows the playing pattern, blinking while a change is queued.
- Global screen, so it uses the neutral color. Green and cyan only appear in the pattern thumbnails (note content and CC content).
- **Banks**: a row of 4 buttons. A small dot marks the bank holding the playing or queued pattern.
- **Pattern grid**: 4 x 4 slots for the current bank. Each slot shows its number, a state (PLAYING with a lit LED, NEXT with a dashed outline, STORED, EMPTY), and an 8-bar thumbnail of the 8 tracks (see 7.1.1). A thin line at the bottom marks patterns used in the chain.
- **Clicking a slot** selects it and queues it as the next pattern. The switch happens at the next quantize point (Settings: 1 beat, 1 bar or pattern end), or instantly when quantize is OFF.
- **Selected bar**: COPY, PASTE and CLEAR for the selected pattern, plus a track dropdown with COPY TRACK to copy a single track. The clipboard content is shown at the right.

#### 7.1.1 Pattern thumbnail

- 8 bars, one per track, track 01 on the left to track 08 on the right.
- **Height = density**: the number of "on" steps divided by the track's length. A busy track is a tall bar, a sparse one a short bar.
- **Color = what the track outputs**:
  - Notes output on only: green.
  - CC output on only: cyan.
  - Both on: a split bar, green bottom half and cyan top half.
  - Both outputs off but the track has on steps: dim grey, at its density height (content exists but is silent).
  - No on steps: a thin dark baseline (empty track).

### 7.2 Chain (proposed)

- An ordered list (song list) of up to 64 entries, shown on the right of the patterns view.
- Each entry: playing LED, position number, pattern (for example "B02 · 07"), length in bars (one play of the pattern times its repeats) with a progress bar while playing, a repeat count (-/+, 1 to 99) and a remove button.
- **+ ADD** appends the selected pattern. CLEAR empties the chain. Entries can repeat the same pattern.
- **CHAIN ON/OFF** turns chain playback on. **LOOP ON/OFF**: at the end of the chain, loop back to the first entry, or stay on the last one.
- One play of a pattern lasts as long as its longest track (length x division).
- A manually queued pattern (clicking a slot) takes priority over the chain for the next change.
- **Reordering**: drag an entry up or down the list (a grip on its left shows it can be dragged). A line shows where it will land. Playback keeps following the entry that is playing, wherever it moves. Removing an entry also keeps playback on the right entry.

### 7.3 Pattern switching rules

- **Quantize OFF**: the switch happens the instant the user picks the pattern (or the moment the pattern-change CC arrives), not at the next step. The new pattern takes over at the same playhead position: each track continues from its current step index in the new pattern, so timing stays in sync. Notes still sounding from the old pattern get their note-offs at the switch.
- **Quantize ON**: the picked pattern is queued ("NEXT") and switches at the next quantize point. Picking another pattern before then replaces the queued one.
- Patterns can be picked from three places: the quick picker in the main header (7.4), the patterns view (7.1), and incoming CC (Settings).

### 7.4 Quick pattern picker (main window header)

- The PATTERN box in the main header has **‹ and › arrows** for the previous / next pattern (crossing into the next or previous bank, wrapping from the last pattern to the first).
- Clicking the pattern name opens a **popover** (styled like the DIVISION and SCALE popovers): a row of 4 banks (a dot marks the bank with the playing or queued pattern), the 16 slots of the chosen bank (playing = lit LED, queued = dashed outline, empty = dim), and an "OPEN PATTERNS VIEW" button for the full patterns view (chain, copy, paste).
- Picking a slot queues or switches it (following the quantize rules) and closes the popover.
- While a pattern is queued, the PATTERN box keeps showing the current pattern and makes it **blink** until the switch happens. It does not show the next pattern's number. The same applies to the PATTERN box in the patterns view.

## 8. Selection and the scope rule

### The LED bar

- Above each row of 16 steps is an LED bar, one segment per step.
- It shows the **playhead**: the segment of the playing step lights in the mode accent (green in note mode, cyan in CC mode) while the host plays.
- Segments beyond the track length are dark and cannot be selected.
- It is also the **selection tool**:
  - Click and drag across segments to select a range. Drag is the only way to select a range (there is no "click start, then click end").
  - A single click on a segment selects that one step (and replaces the selection).
  - **Ctrl-click** (Cmd-click on Mac) adds a step to the selection, or removes it if it is already selected, so non-adjacent steps can be selected.
  - **Ctrl-drag** adds a range to the existing selection. A plain drag replaces it.
- Selection is shown **only on the LED bar** (white segments). The step cells themselves are not colored or highlighted.
- A CLEAR button removes the selection. Switching tracks clears the selection.

### Scope rule (applies to EVERY step operation)

- Every operation on steps (shift, randomize, smooth, copy, paste, and any future step operation) on any lane, including probability, works on **the selected steps if there is a selection**, otherwise on **all steps of the current track's pattern** (within its length).
- "No selection" is equivalent to "all steps".
- The Selection box shows one line, "LENGTH: 7, STEPS: 4": length = first to last step in scope, steps = how many. With nothing selected it describes all steps; while COPIED is lit it describes the clipboard. No other explanatory text is shown.
- Operations on a non-adjacent selection act on exactly those steps (Smooth averages only those steps).

## 9. Step operations

All destructive operations are undoable. Multi-level undo, at least 32 levels, covering every step edit and operation. One gesture is one undo entry (for example a whole knob drag, one Randomize press, one paste).

### 9.1 Copy / Paste

- Operations always act on the currently selected track. Copy/paste is the way to move steps between tracks and patterns.
- **COPY** copies the steps in scope (the selection, or all steps within the length if none), but **only the lanes armed in LANES** (the same chips as Randomize and Smooth; in CC mode that is ON and CC). The lanes are fixed at the moment of copying. With no lane armed, COPY does nothing.
- After COPY the button lights up and reads **COPIED**, and the selection is gone: while COPIED is lit there is no selection at all. A click on an LED only picks the **landing step** (where the first copied step goes); dragging and Ctrl-click do not select.
- Once a landing step is picked, a **ghost preview** on the LEDs shows where the steps will land. Copied steps **keep their spacing**: copying steps 1, 3 and 6 and landing at 9 writes steps 9, 11 and 14; the steps in the gaps are left unchanged.
- **PASTE** (dimmed until a landing step is picked) writes the copied lanes there. COPIED stays lit, so the user can pick another landing step, switch track or pattern, and paste again.
- Clicking **COPIED** empties the clipboard and turns it back into COPY. **CLEAR** deselects, and when COPIED is lit also empties the clipboard and removes the ghost preview.
- A paste that runs past the destination track's length is **cut off** at the last step. It never wraps around to step 1. Paste can be undone.
- While COPIED is lit, Randomize, Smooth and Shift act on all steps (there is no selection).

### 9.2 Randomize

- Lane selection with toggle chips (the LANES group, shared with Smooth): ALL, ON, NOTE, OCT, VEL, GATE, PROB (note mode), or ALL, ON, CC (CC mode).
- Each lane has a min/max range. Random values stay inside it.
- NOTE randomization is scale-aware when the track's scale is on.
- Applies to the steps in scope (see 8), including off steps.

### 9.3 Smooth (replaces Mutate, which is removed)

- A SMOOTH button pulls the values of the steps in scope one step closer to their average.
- For each selected lane (same LANES chips as Randomize):
  1. Calculate the average of that lane's values over the steps in scope, **rounded to the nearest whole number**.
  2. Every value below the average goes up by 1, every value above it goes down by 1. Values equal to the average do not move.
- Example: CC values 0, 64, 127 give an average of 63.67, rounded to 64. One press gives 1, 64, 126.
- The average is recalculated on every press.
- Units of "one step" per lane: NOTE 1 scale step (1 semitone when the scale is off), OCT 1 octave, VEL 1, GATE 1%, PROB 1%, CC 1.
- NOTE and octave are treated as one combined pitch for NOTE smoothing, so notes converge across octave boundaries.
- The ON lane is ignored by Smooth (on/off has no average).
- Off steps are **excluded** from Smooth: they are not counted in the average and their values are not changed. (Randomize, by contrast, does include off steps.)
- Smooth ignores the Randomize min/max ranges (it can only move values toward the average, never outside the current spread).
- Press once for one step. **Press and hold** repeats (after a short delay), so large spreads can be flattened quickly. One press or one hold is one undo entry.

### 9.4 Shift (replaces per-track transpose)

A relative shift of all steps in scope.

- A lane selector picks what the shift changes:
  - **NOTE**: ±1 per detent, in scale steps when the scale is on (semitones when it is off). Crossing the top or bottom of an octave rolls into the next or previous octave and continues.
  - **OCT**: ±1 octave per detent.
  - **VEL**: ±1 per detent.
  - **GATE**: ±1% per detent.
  - **PROB**: ±1% per detent.
  - **CC** (CC mode): ±1 CC value per detent.
- Controlled by an **endless knob**. While turning, it shows the running offset (for example "+3"). It returns to 0 when released, because it is a one-time action, not a stored setting.
- **Capping**: each step moves relative to its own value. A step that reaches its limit stays at the limit while the others keep moving.
- **Gesture snapshot**: during one drag, every step is calculated from its value at the moment the knob was grabbed. Turning back within the same drag restores the original shape exactly. Steps update live while dragging (so the change can be heard), and the whole drag is one undo entry. After release, undo is the way back.
- Shift lives in the operations panel (not the parameter row) because it follows the scope rule like Randomize and Smooth.

## 10. CC output

- One CC per track (number set per track), on the track's MIDI channel.
- Off steps send no CC.
- Smoothing on/off (Settings). When on, the CC glides from one active step's value to the next instead of jumping. The glide passes over off steps. Smoothing time is configurable.

## 11. Settings dialog (board E)

Opened with the SETTINGS button. 600 x 720 dialog, same style as the main window. Sections, top to bottom:

- **Appearance**: Theme, DARK / LIGHT (neutral).
- **Pitch** (green): Octave range as two knobs, LOWEST and HIGHEST (the highest is always above the lowest). Middle C is C3 or C4.
- **User scale** (green): 12 note toggles with a count ("7 OF 12 NOTES"). The toggles are relative to the root (the first toggle is the root). The root is always included. A ROOT row above the toggles sets the root right here, so the user does not need to go back and forth with the main window. It is the same global root as in the SCALE menu: changing it in either place changes both. The toggle labels follow the root (root D shows D, D#, E, ...). Selected in the main window as "USER".
- **CC output** (cyan): Smooth between steps ON/OFF, and Smoothing time as a knob in percent of a step (1-100%). The time is dimmed when smoothing is off.
- **Pattern change** (neutral): From incoming CC ON/OFF, with MIDI CH and CC number knobs (dimmed when off). Quantize changes to OFF, 1 BEAT, 1 BAR or PATTERN END. OFF means no quantize: the pattern changes the instant it is picked (see 7.3).
- Footer: RESET DEFAULTS and DONE.

## 12. UI

### Look

- Modern, flat, "digital". NOT skeuomorphic: no fake hardware, textures, shadows or 3D knobs.
- Dark theme. Fonts: IBM Plex Sans (labels) and IBM Plex Mono (values).
- Colors: background #0E1114, panels #12161B / #161A20, borders #222830 / #2A313B, text #EEF1F5 / #C5CBD4 / #8A94A3.
- Mode accents: note mode green #4FD98A, CC mode cyan #5CC8FF. The accent colors the mode switch, lit step buttons, pitch arcs, playhead LED and main buttons.
- Lane colors: velocity green #4FD98A (same as note mode), gate purple #B48CFF, probability amber #FFB547, octave light grey #C5CBD4.
- Color tells which part a control affects:
  - **Track-wide** controls (LENGTH, DIVISION, DIRECTION, SWING, MIDI CH) use a fixed neutral #DDE2E8 and never change color with the mode. Selected division/direction options are neutral-filled with dark text.
  - **Note part** controls (NOTE OUT, SCALE) are always green.
  - **CC part** controls (CC OUT, CC number) are always cyan.
- Two themes, DARK (default) and LIGHT. The choice lives in the Settings dialog, not in the main window. All colors are theme tokens, so both themes share one layout.
- LIGHT theme palette (board F), built on a light grey, not white: background #D3D7DD, panels #DCE0E5, tiles #E3E6EA, alternate step groups #DADDE2, borders #C2C7CF / #B9BFC8 / #AEB5BF, text #11151A / #2C333D / #525B68, neutral (track-wide) #2A313B, selection LED #11151A. Accents are darkened for contrast on grey: note green #12864A, CC cyan #0874AE, gate purple #6539C2, probability amber #A86600.
- Selection: white #EEF1F5 on the LED bar.
- Base window size 1200 x 720.
- **Resizable**: the window scales as a whole (everything grows or shrinks together, proportions fixed), from 75% (900 x 540) to 200% (2400 x 1440), by dragging the corner. The size is saved with the project and the editor reopens at that size.

### Layout (top to bottom)

1. **Header**: plugin name, TRACK dropdown, NOTES / CC mode switch, PATTERN quick picker (arrows and popover, see 7.4; the popover links to the full patterns view, 7.1), host transport status (play state and BPM, display only, since the host controls transport), UNDO, SETTINGS button (labeled, not an icon-only button).
2. **Track parameter row** (see 12.2).
3. **Step grid**: 2 rows of 16 steps, each row with its LED bar above it.
4. **Operations panel**: Selection (with COPY, PASTE, CLEAR), Lanes, Ranges, RANDOMIZE and SMOOTH buttons, Shift.

### 12.1 Step cell

- **Top**: one button with the step number inside, used as the on/off toggle. On: filled with the mode accent, number in a dark contrasting color. Off: outline only.
- **Note mode**: pitch knob, octave knob below it, then a vertical slider. A global "SLIDER" switch (top right of the parameter row) makes all sliders show velocity, gate or probability. The slider takes that lane's color.
- **CC mode**: one larger knob showing the step's CC value. Cells keep the same height in both modes so the layout does not jump.
- Off steps: controls dimmed.
- Knobs are flat arc rings (270 degrees) with the value as text in the middle.

### 12.2 Track parameter row controls

- **Numeric values** (LENGTH, SWING, MIDI CH, CC number): small arc knobs (40 px), styled like the octave knob, value shown inside. No extra unit text beside them except "steps" for LENGTH.
- No explanatory or help text in the main window (no hints under Selection or Shift).
- **DIVISION**: click opens a popover grid with the straight values on one row and triplets on a second row. Mouse wheel on the tile steps through values.
- **DIRECTION**: three inline icon buttons (forward, backward, ping-pong). No menu.
- **SCALE**: two parts. A scale-name button (for example "C MIN") and a separate ON/OFF toggle for the current track. Clicking the name opens a popover, styled like the DIVISION popover, with a "GLOBAL" tag (the choice applies to every track that has SCALE on), a ROOT row (C to B) and a grid of the 14 scales plus USER. Picking a scale closes the popover. The mouse wheel over the name steps through the scales. Only one popover is open at a time.
- **NOTE OUT** and **CC OUT**: on/off toggles. The CC OUT tile also holds the CC number knob.
- To avoid accidents, part-specific controls are only shown in their own mode: SCALE and NOTE OUT only in note mode, CC OUT (with the CC number) only in CC mode. Track-wide controls are shown in both.
- The NOTES and CC mode buttons in the header each carry an LED showing whether that output is on for the selected track (lit in green / cyan when on, dark grey when off). The LEDs are visible in both modes, so the hidden output's state is always known. The selected mode button is shown with a tinted background and accent outline, so the LED stays readable.
- **SLIDER**: VEL / GATE / PROB switch (note mode only).

### 12.3 Knob and slider interaction (all knobs, including the per-step knobs, and the step sliders)

- Drag up / down to change the value. Mouse wheel changes it by one step.
- Shift + drag for fine control.
- Double-click to type a value.
- Right-click (or Alt + click) to reset to default.

### 12.4 Operations panel (bottom, left to right)

1. **Selection**: the "LENGTH: 7, STEPS: 4" line, with COPY (COPIED when lit), PASTE and CLEAR (9.1).
2. **Lanes**: toggle chips shared by Randomize and Smooth (see 9.2, 9.3).
3. **Ranges**: min/max bar per lane, in that lane's color. Ranges of lanes that are not selected are dimmed.
4. **RANDOMIZE** (filled, mode accent) and **SMOOTH** (outlined, mode accent).
5. **SHIFT ALL**: endless knob plus lane chips (NOTE, OCT, VEL, GATE, PROB in note mode; CC in CC mode).

### 12.5 Mockups

- The interactive mockups live in the "Step Sequencer Mockups" design canvas: boards C (main window, note and CC mode), D (patterns view and chain), E (settings dialog), F (light theme) and G (macros). The old exploration boards A and B were deleted.
- The plugin name is still a placeholder ("[NAME]") in the mockups.

## 13. State and presets

- The full state (all patterns, track settings and global settings) is saved with the DAW project.
- Preset save/load to files is planned. Details are open.

### 13.1 Macros (host automation)

The host sees 128 parameters, "Macro 1" to "Macro 128", grouped in 4 banks of 32. They are the only automatable parameters. Each macro is mapped to one target in the plugin, so only what is needed is exposed. Mapping a DAW knob or controller to a macro is done in the DAW; VIMA has no MIDI learn of its own.

Targets:

| Type | Targets | Track | Pattern | Step |
|---|---|---|---|---|
| GLOBAL | Pattern (switches follow the quantize setting), Chain On | - | - | - |
| TRACK | Notes Out, CC Out | SHOWN or 1-8 | - | - |
| TRACK | Length, Division, Direction, Swing, Scale On | SHOWN or 1-8 | CURRENT or a fixed pattern | - |
| STEP | On/Off, Note, Octave, Velocity, Gate, Probability, CC value | SHOWN or 1-8 | CURRENT or a fixed pattern | 1-32 |

- **SHOWN** = the track shown in the main window (saved with the project), for use with controllers: the same macros always control the track on screen. **CURRENT** = the playing pattern.
- **Range**: each mapping has MIN and MAX (default: the full range). The whole macro travel covers MIN to MAX. MIN above MAX inverts the direction.
- **Note** follows the scale like the step's note knob: when the target track's scale is on, the macro moves only through in-scale notes between MIN and MAX (C minor over C-B = 7 positions spread over the whole travel); with the scale off, through all 12. MIN / MAX are set as note names. **Octave** covers the octave range from Settings.
- **Two-way**: a macro move from the host changes the target on the next audio block. When the target changes inside VIMA (editing, pattern switch, chain, SHOWN track switch, remapping), the macro moves to match, as a short gesture so hosts can record it. A target value outside MIN / MAX is shown at the nearest end and is not overwritten by that.
- The host shows the target value as the macro's value text ("96", "1/16", "B01 · 05"); unmapped macros show "-".
- Host changes are not added to undo. Mappings are saved with the project.

### 13.2 Macros page

Opened with the MACROS button in the header (between UNDO and SETTINGS); STEPS goes back. Mockup: board G.

- **Left**: bank buttons (1-32, 33-64, 65-96, 97-128), "N OF 32 MAPPED", LEARN. A grid of 32 macro cells: number, color dot, a knob with the target's value (drag or wheel to move the macro), target name and where it points ("SHOWN · CUR", "T04 · B01·06", "GLOBAL").
- **Right**: the mapping editor for the selected macro: TARGET (NONE / GLOBAL / TRACK / STEP), PARAMETER, TRACK (SHOWN, 1-8), PATTERN (CURRENT / FIXED with a pattern knob), STEP (1-32), RANGE (MIN / MAX), CLEAR MAPPING and the current value.
- **LEARN**: with LEARN on, go back to STEPS (or the patterns view) and click a control: the selected macro is mapped to it (SHOWN track, CURRENT pattern, that step) and the next macro is selected, so a row of steps can be mapped quickly. While learning, clicks map instead of editing, an amber frame marks the view, the control under the mouse is highlighted, and the header button reads "LEARN M005". Learnable: step on/off, note, octave, the slider (its current lane), CC value, LENGTH, DIVISION, DIRECTION, SWING, SCALE on, NOTE OUT, CC OUT, pattern slots (Pattern) and CHAIN on/off.
- Colors follow the lanes: step on, note, octave and velocity green, gate purple, probability amber, CC cyan, track and global targets neutral.

## 14. Build and repository

- Repository on GitHub under the user's account (TheoGal). Repo name follows the final plugin name.
- GitHub Actions builds the plugin on every push: Windows VST3 (primary) and macOS VST3 (for testers). Build artifacts are attached to the workflow run, and to a GitHub Release when a version tag is pushed.
- JUCE version pinned in CMake.
- First milestone: a minimal test plugin that outputs MIDI on several channels, so the Live routing questions in section 2 can be tested before the full build.

## 15. Open questions

1. Live: VST3 MIDI output and channel separation (section 2).
2. Approve the proposed patterns view and chain design (7.1, 7.2).
3. Preset file details (13).
4. Probability and CC: should a step that fails its probability roll also skip its CC, or does CC always send? (Current assumption: probability affects notes only.)
5. Note randomize range: is the NOTE min/max a pitch-class range (for example C to A#) or a full note range across octaves (for example C2 to C4)?
6. Approve the redesigned Settings dialog (11).

## 16. Decision log

- 2026-10-20: 16 tracks, assignable MIDI channel per track, independent note and CC outputs on the same channel.
- 2026-10-20: Instrument plugin (not MIDI effect), VST3, built by GitHub Actions.
- 2026-10-20: Follow host transport only. Swing per track. Mutate is one-shot and destructive.
- 2026-10-21: Main window is the hybrid layout (board C). Track dropdown, no names, no sidebar.
- 2026-10-21: LED bar is playhead and selection. Selection only by drag (single click = one step). Selection shown only on the LEDs.
- 2026-10-21: Per-track transpose replaced by SHIFT ALL with lane selector, capping and gesture snapshot.
- 2026-10-21: Scope rule: every step operation acts on the selection, or all steps if none.
- 2026-10-21: Probability lane added. Colors: note mode and velocity green, gate purple, probability amber, CC cyan.
- 2026-10-21: Cross-track copy/paste: COPY, then PASTE arms, click start LED on any track. Overflow is cut off.
- 2026-10-21: No help text in the main window. Theme switching lives in Settings.
- 2026-10-21: The design is its own; no reference to Zaquencer behavior.
- 2026-10-21: Track-wide parameters use a fixed neutral color; note-part controls green, CC-part controls cyan.
- 2026-10-21: Part-specific controls only shown in their own mode. Output on/off LEDs on the NOTES / CC mode buttons.
- 2026-10-21: Mutate removed. Replaced by Smooth: each press moves values one step toward their rounded average.
- 2026-10-21: Scale and root are picked from a popover on the SCALE tile in the main window. The user scale is still edited in Settings.
- 2026-10-21: Pattern quantize has an OFF option: instant switch at the click, same playhead position. Patterns can be picked in the main header (arrows and popover).
- 2026-10-21: Queued pattern change shown by blinking the current pattern number.
- 2026-10-21: Pattern thumbnails: bar height = density, color = output (green, cyan, split for both, grey for silent).
- 2026-10-21: Name: VIMA. Repository TheoGal/vima-vst.
- 2026-10-21: Added: window resizing (75%-200%, uniform scaling), chain reordering by dragging, Ctrl-click / Ctrl-drag selection of non-adjacent steps. Pasting keeps the spacing of copied steps.
- 2026-10-22: Copy/paste redesign (9.1): COPY follows the armed lanes and lights up as COPIED; no selection while COPIED, an LED click picks the landing step with a ghost preview, PASTE writes and stays ready for more; COPIED or CLEAR empties the clipboard. Selection readout replaced by "LENGTH: n, STEPS: n".
- 2026-10-21: Note macros are scale dependent, like the note knobs. The SHOWN track is saved with the project. No right-click mapping (right-click resets controls).
- 2026-10-21: Host automation through 128 mappable macros (13.1, 13.2) instead of fixed parameters. Step targets can use a fixed track and pattern, or SHOWN / CURRENT for controllers. No direct MIDI learn in VIMA. Scale and root are not targets.
- 2026-10-21: Reduced to 8 tracks and 64 patterns (4 banks of 16). Pattern change by CC now reaches every pattern.
- 2026-10-21: Build decision: musical track settings are stored per pattern; routing (channel, outputs, CC number) is global per track. Undo covers step and pattern edits, not routing changes.
