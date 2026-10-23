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

### Ableton Live MIDI routing (tested)

- Live accepts MIDI output from VIMA, but strips the channel: all tracks of one instance arrive together on the destination track. In Live, use one VIMA instance per destination track (each instance still has 8 tracks). Other hosts that keep channels can route one instance to several destinations.

## 3. Transport and clock

- Follows the host: plays when the host plays, stops when the host stops, follows host tempo and song position.
- **Start / stop button** (the play / stop icon in the HOST box of the header, next to the BPM):
  - **Stop while the host plays**: VIMA stops and is **held**. A held VIMA ignores the host stopping and starting again; it stays stopped until its own play button is pressed. The stop icon is **amber** while held.
  - **Play while the host plays**: VIMA starts again in sync with the host and the hold is released; from then on it follows the host.
  - **Play while the host is stopped**: VIMA runs on its own from step 1 at the host's tempo (this also releases a hold). Stopping it with the button, or the host starting, ends that. A stop made while the host is stopped does not hold: the next time the host plays, VIMA follows.
  - The hold is not saved: it is released when the project is reopened.
- The icon shows whether VIMA is playing (play icon), stopped (grey stop icon) or held (amber stop icon).

## 4. Tracks

- 8 tracks, shown as "01" to "08". No track names.
- Selected with a **dropdown** in the header. No sidebar.
- Every track always has both a note part and a CC part, sharing the track's steps, length, division, direction and swing, but each with **its own on/off per step** (5). The NOTES / CC switch in the header only changes which part is shown and edited (including which on/off the step buttons and the ON lane button use). It does not change what the track outputs; that is set by the NOTE OUT and CC OUT toggles.

Where track settings live (decided during the build):

- **Per track, per pattern** (stored in each pattern, so patterns can differ): length, division, direction, swing, scale on/off, and all steps.
- **Per track, global** (the same in every pattern, because they describe where the track is routed): MIDI channel, notes output on/off, CC output on/off, CC number.

Per track parameters:

| Parameter | Range / values | Notes |
|---|---|---|
| MIDI channel | 1-16 | Freely assignable per track. Track N is NOT tied to channel N. |
| Notes output | on / off | Independent of CC output. |
| CC output | on / off | Independent of notes output. Sends on the same MIDI channel as the notes. |
| CC number | 0-127 | User-defined, one CC per track. |
| Length | 4-32 steps | |
| Division | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 and triplets 1/2T, 1/4T, 1/8T, 1/16T, 1/32T | |
| Direction | Forward, Backward, Ping-pong | |
| Swing | 0-100% | Per track. 0% = straight. Delays every second step; the exact curve is tuned during the build. |
| Scale | on / off | Per-track switch to use or bypass the global scale. |

### 4.1 Init values (= defaults everywhere)

A new instance, NEW in the FILE menu, CLEAR in the patterns view and a right-click / Alt + click on any control all use these values. All 64 patterns start like this.

| Setting | Init value |
|---|---|
| Notes output | on for track 1, off for tracks 2-8 |
| CC output | off on all tracks |
| MIDI channel | track number (1-8) |
| CC number | 102 + track index: tracks 1-8 = CC 102-109 (undefined CCs in the MIDI spec) |
| Length / division / direction / swing / scale | 16 / 1/16 / forward / 0% / on |
| Steps | all 32 on (note on and CC on), note F (the middle of the note knob's travel), octave of middle C (so F above middle C, MIDI 65), velocity 100, gate 100%, probability 100%, CC value 64 |
| Current pattern | 01 |
| Global scale | C minor |
| Middle C | C3 |
| CC smoothing | off, time 100% |
| Pattern change by CC | off, channel 16, CC 20; quantize 1 BAR |
| Chain | off, loop on, empty |
| LANES | NOTE only (in CC mode: none) |
| RANGES | full range of every lane (NOTE C-B, OCT = all octaves (C-2 to G8 with middle C = C3), VEL 1-127, GATE 1-400%, PROB 0-100%, CC 0-127) |
| SHIFT ALL lane / SLIDER lane / view | NOTE / VEL / NOTES |
| Macros | none mapped |
| Theme | dark |


## 5. Steps

Each track has 32 steps. Steps beyond the track length are shown dimmed and are not played.

Per step values:

| Value | Range | Notes |
|---|---|---|
| Note on / off | on / off | Whether the step plays its note. An off step still takes its time slot; it is not skipped. |
| CC on / off | on / off | Whether the step sends its CC value, **independent of the note on/off**: a step can send CC without a note, or a note without CC. Length, division, direction and swing are shared by both parts. |
| Note | pitch class | Separate control from octave. |
| Octave | user-defined range (Settings) | |
| Velocity | 1-127 | Minimum is 1, never 0: in MIDI a Note On with velocity 0 is treated as a Note Off. Every operation (knob, Randomize, Smooth, Shift, ranges) clamps velocity to 1-127. |
| Gate | 1-400% | Percentage of one step at the track's division: 100% = one step (for 1/16, a 1/16 note), 50% = half a step (a 1/32 note), up to 400% = four steps (legato and long notes). No click-in points. When a long note is still held and the same pitch plays again, the old note ends just before the new one. Long notes end cleanly on stop, pattern change and jumps. |
| Probability | 0-100% | Chance that an "on" step actually plays each time it is reached. 100% = always. Applies to the note output (see open question about CC). |
| CC value | 0-127 | Used when the track's CC output is on. |

Pitch display and scale behavior:

- Every step always displays the note that will actually sound (for example "D#3").
- When the track's scale is on, the note knob can only land on notes in the scale, and its arc follows the scale: the in-scale notes are spread evenly over the whole arc, whatever their number (7 in C minor, 3 in a 3-note user scale). The lowest in-scale note (from C up) is fully left, the highest fully right (A# in C minor). With the scale off, the arc covers all 12 notes, C to B.

There is no step skip, no chords, no drum mode. Note mode only.

## 6. Global settings

- Scale: 13 built-in scales plus 1 user scale, with a root note: Major, Natural minor, Harmonic minor, Melodic minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major pentatonic, Minor pentatonic, Blues, Whole tone. There is no Chromatic scale: a track with its scale OFF plays all 12 notes.
- The scale is **selected in the main window** (SCALE tile popover, see 12.2), never in the Settings dialog. The root can be set in the SCALE popover and also in the Settings user scale editor (same global value).
- User scale: 12 note toggles. It is **edited only in the Settings dialog**, and can only be **selected** (as "USER") from the main window.
- Pattern change from incoming CC: MIDI channel and CC number configurable.
- Pattern change quantization: OFF (instant, at the moment of the click), 1 beat, 1 bar or end of pattern.

## 7. Patterns

- 64 patterns, numbered 1-64 everywhere and shown as two digits ("05", "21"). The screens show them in 4 pages of 16 (1 - 16, 17 - 32, 33 - 48, 49 - 64). Every pattern can be reached by one incoming CC value.
- A pattern holds all 8 tracks.
- Copy a whole pattern, or a single track, to another slot.
- Pattern chaining.

### 7.1 Patterns view (proposed, board D, pending approval)

- Opened with the "OPEN PATTERNS VIEW" button in the header's pattern popover (7.4). It replaces the step view; a "STEPS" button in the header goes back. While open, the PATTERN box is highlighted and shows the playing pattern, blinking while a change is queued.
- Global screen, so it uses the neutral color. Green and cyan only appear in the pattern thumbnails (note content and CC content).
- **Pages**: a row of 4 buttons, 1 - 16, 17 - 32, 33 - 48, 49 - 64. A small dot marks the page holding the playing or queued pattern.
- **Pattern grid**: 4 x 4 slots for the current page. Each slot shows the pattern's own number (page 17 - 32 shows 17 to 32), a small **play button at the top center**, a state (PLAYING with a lit LED, NEXT with a dashed outline, STORED, EMPTY), and an 8-bar thumbnail of the 8 tracks (see 7.1.1). A thin line at the bottom marks patterns used in the chain.
- **Clicking a slot** anywhere except its play button only **selects** it (for COPY and COPY TRACK). It does not play it.
- **The play button** selects the slot **and** queues it as the next pattern. The switch happens at the next quantize point (Settings: 1 beat, 1 bar or pattern end), or instantly when quantize is OFF. The header picker and arrows also play patterns directly (7.4).
- **Selected bar** (bottom): SELECTED and the selected pattern's number, COPY, the TRACK dropdown, COPY TRACK, PASTE and CLEAR. On the right, an info text shows what is copied: "PATTERN 05" for a pattern, "PATTERN 05 - TRACK 02" for a track, nothing when the clipboard is empty.

#### 7.1.2 Copy / paste of patterns and tracks

Works like step copy / paste (9.1):

- **COPY** copies the whole selected pattern; **COPY TRACK** copies the selected pattern's track chosen in the TRACK dropdown. Either button lights up and reads **COPIED** or **TRACK COPIED**. There is one clipboard: copying again replaces it.
- A copy takes **everything**: all steps (note and CC parts) and all settings stored in the pattern. Armed lanes do not apply here.
- While a copy button is lit, a click on a slot only picks the **destination pattern**, shown with a dashed outline (the ghost preview). The selection does not change. The slot play buttons still play.
- For a track copy, the **destination track** is the one in the TRACK dropdown when PASTE is pressed, so track 1 can be pasted into track 2 of the same or another pattern.
- **PASTE** is dimmed until a destination is picked. It writes there (one undo step), and the button stays lit for more pastes.
- Clicking the lit button, or **CLEAR**, cancels: the clipboard is emptied and the ghost preview removed. **CLEAR never erases a pattern.**

#### 7.1.1 Pattern thumbnail

- 8 bars, one per track, track 01 on the left to track 08 on the right.
- **Height = density**: the number of steps that are on, divided by the track's length. A busy track is a tall bar, a sparse one a short bar.
- **Which part and color = what the track outputs**:
  - Notes output on only: the note part's density, green.
  - CC output on only: the CC part's density, cyan.
  - Both on: two thin bars side by side, notes (green) on the left and CC (cyan) on the right, each at its own density.
  - Both outputs off: dim grey, at the busier part's density (content exists but is silent).
  - No steps on: a thin dark baseline.

### 7.2 Chain (proposed)

- An ordered list (song list) of up to 64 entries, shown on the right of the patterns view.
- Each entry: playing LED, position number, pattern (for example "23"), length in bars (one play of the pattern times its repeats) with a progress bar while playing, a repeat count (-/+, 1 to 99) and a remove button.
- **+ ADD** appends the selected pattern. CLEAR empties the chain. Entries can repeat the same pattern.
- **CHAIN ON/OFF** turns chain playback on. **LOOP ON/OFF**: at the end of the chain, LOOP ON goes back to the first entry; LOOP OFF plays the chain once, then keeps playing (repeating) its last pattern until the transport stops or another pattern is picked.
- The chain does not follow the song position: when the host starts, the chain always starts from its first entry, wherever the host's playhead is. A jump while playing (locate or host loop) restarts the current entry from there. Steps inside a pattern still follow the host position, so timing stays in sync. (Decided to keep it this way: a song-position chain would make editing patterns from the DAW awkward.)
- One play of a pattern lasts as long as its longest track (length x division).
- A manually queued pattern (a slot's play button) takes priority over the chain for the next change.
- **Reordering**: drag an entry up or down the list (a grip on its left shows it can be dragged). A line shows where it will land. Playback keeps following the entry that is playing, wherever it moves. Removing an entry also keeps playback on the right entry.

### 7.3 Pattern switching rules

- **Quantize OFF**: the switch happens the instant the user picks the pattern (or the moment the pattern-change CC arrives), not at the next step. The new pattern takes over at the same playhead position: each track continues from its current step index in the new pattern, so timing stays in sync. Notes still sounding from the old pattern get their note-offs at the switch.
- **Quantize ON**: the picked pattern is queued ("NEXT") and switches at the next quantize point. Picking another pattern before then replaces the queued one.
- Patterns can be picked from three places: the quick picker in the main header (7.4), the patterns view (7.1), and incoming CC (Settings).

### 7.4 Quick pattern picker (main window header)

- The PATTERN box in the main header has **‹ and › arrows** for the previous / next pattern (crossing pages, wrapping from the last pattern to the first).
- Clicking the pattern name opens a **popover** (styled like the DIVISION and SCALE popovers): a row of 4 page buttons, 1 - 16 to 49 - 64 (a dot marks the page with the playing or queued pattern), the 16 slots of the chosen page, numbered with the patterns' own numbers (playing = lit LED, queued = dashed outline, empty = dim), and an "OPEN PATTERNS VIEW" button for the full patterns view (chain, copy, paste).
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
- **COPY** copies the steps in scope (the selection, or all steps within the length if none), but **only the lanes armed in LANES** (the same lane buttons as Randomize and Smooth; in CC mode that is ON and CC). The lanes are fixed at the moment of copying. With no lane armed, COPY does nothing.
- After COPY the button lights up and reads **COPIED**, and the selection is gone: while COPIED is lit there is no selection at all. A click on an LED only picks the **landing step** (where the first copied step goes); dragging and Ctrl-click do not select.
- Once a landing step is picked, a **ghost preview** on the LEDs shows where the steps will land. Copied steps **keep their spacing**: copying steps 1, 3 and 6 and landing at 9 writes steps 9, 11 and 14; the steps in the gaps are left unchanged.
- **PASTE** (dimmed until a landing step is picked) writes the copied lanes there. COPIED stays lit, so the user can pick another landing step, switch track or pattern, and paste again.
- Clicking **COPIED** empties the clipboard and turns it back into COPY. **CLEAR** deselects, and when COPIED is lit also empties the clipboard and removes the ghost preview.
- A paste that runs past the destination track's length is **cut off** at the last step. It never wraps around to step 1. Paste can be undone.
- While COPIED is lit, Randomize, Smooth and Shift act on all steps (there is no selection).

### 9.2 Randomize

- Lane selection with the **lane buttons** (the LANES group, shared with Smooth and Copy): ALL, ON, NOTE, OCT, VEL, GATE, PROB in note mode (ON = note on/off), or ALL, ON, CC in CC mode (ON = CC on/off). The LED bar chooses which steps; the lane buttons choose which values of those steps.
- Each lane has a min/max range. Random values stay inside it.
- NOTE randomization is scale-aware when the track's scale is on.
- Applies to the steps in scope (see 8), including off steps.

### 9.3 Smooth (replaces Mutate, which is removed)

- A SMOOTH button pulls the values of the steps in scope one step closer to their average.
- For each selected lane (same lane buttons as Randomize):
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
- Smoothing on/off (Settings). When on, the CC glides from one CC-on step's value to the next instead of jumping, over the whole time between them. The glide passes over off steps. There is no smoothing time setting.

## 11. Settings dialog (board E)

Opened with the SETTINGS button. 600 x 720 dialog, same style as the main window. Sections, top to bottom:

- **Appearance**: Theme, DARK / LIGHT (neutral).
- **Pitch** (green): there is no octave range setting; octaves always cover the whole MIDI range (MIDI notes 0-127). Middle C is (how octaves are numbered on screen; MIDI note 60 is always middle C): **C3 (lowest C-2)**, **C4 (lowest C-1)** or **C5 (lowest C0)**, shown with the lowest C in brackets, in capitals like the rest of Settings: "C3 (LOWEST C-2)", "C4 (LOWEST C-1)", "C5 (LOWEST C0)". C3 matches Live, Bitwig, Logic, Cubase, Studio One, Pro Tools and Reason; C4 matches Reaper, Tracktion and scientific pitch notation; C5 matches FL Studio, Cakewalk and Renoise. Default C3.
- **User scale** (green): 12 note toggles with a count ("7 OF 12 NOTES"). The toggles are relative to the root (the first toggle is the root). The root is always included. A ROOT row above the toggles sets the root right here, so the user does not need to go back and forth with the main window. It is the same global root as in the SCALE menu: changing it in either place changes both. The toggle labels follow the root (root D shows D, D#, E, ...). Selected in the main window as "USER".
- **CC output** (cyan): Smooth between steps ON/OFF (10).
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

1. **Header**: plugin name, TRACK dropdown, NOTES / CC mode switch, PATTERN quick picker (arrows and popover, see 7.4; the popover links to the full patterns view, 7.1), HOST box (start / stop button with the play state, and the host BPM, see 3; no "HOST" word), FILE menu (13.3), UNDO, SETTINGS button (labeled, not an icon-only button).
2. **Track parameter row** (see 12.2).
3. **Step grid**: 2 rows of 16 steps, each row with its LED bar above it.
4. **Operations panel**: Selection (with COPY, PASTE, CLEAR), Lanes, Ranges, RANDOMIZE and SMOOTH buttons, Shift.

### 12.1 Step cell

- **Top**: one button with the step number inside, used as the on/off toggle. On: filled with the mode accent, number in a dark contrasting color. Off (within the length): filled grey. Beyond the length: outline only (and the whole cell dimmed).
- **Note mode**: pitch knob, octave knob below it, then a vertical slider. A global "SLIDER" switch (top right of the parameter row) makes all sliders show velocity, gate or probability. The slider takes that lane's color, and so does the value shown under it (dimmed when the step is off).
- **CC mode**: one tall cyan slider for the step's CC value (0-127), filling the cell below the step button, with the value under it in cyan (dimmed when the step's CC is off). It works like the note-mode sliders: click to jump, click and drag across neighbouring steps to draw. Cells keep the same height in both modes so the layout does not jump.
- Off steps: controls dimmed.
- The playhead is shown on the LED bar only; step cells do not change as it passes.
- Knobs are flat arc rings (270 degrees) with the value as text in the middle.

### 12.2 Track parameter row controls

- **Layout**: the track-wide controls on the left: first **MIDI CH** (the same in every pattern), then a small extra gap, then the settings stored per pattern: LENGTH, DIVISION, DIRECTION, SWING. The mode-specific controls are aligned to the right edge, with the output ON/OFF always last: note mode SCALE, SLIDER, NOTE OUT; CC mode CC NUM, CC OUT. The free space between the two groups separates them.
- **Numeric values** (LENGTH, SWING, MIDI CH, CC number): small arc knobs (40 px), styled like the octave knob, value shown inside. No extra unit text beside them except "steps" for LENGTH.
- No explanatory or help text in the main window (no hints under Selection or Shift).
- **DIVISION**: click opens a popover grid with the straight values on one row and triplets on a second row. Mouse wheel on the tile steps through values.
- **DIRECTION**: three inline icon buttons (forward, backward, ping-pong). No menu.
- **SCALE**: two parts. A scale-name button (for example "C MIN") and a separate ON/OFF toggle for the current track. Clicking the name opens a popover, styled like the DIVISION popover, with a "GLOBAL" tag (the choice applies to every track that has SCALE on), a ROOT row (C to B) and a grid of the 13 scales plus USER. Picking a scale closes the popover. The mouse wheel over the name steps through the scales. Only one popover is open at a time.
- **NOTE OUT** and **CC OUT**: on/off toggles, the last tile of the row. The CC number has its own tile, **CC NUM**, left of CC OUT.
- To avoid accidents, part-specific controls are only shown in their own mode: SCALE and NOTE OUT only in note mode, CC NUM and CC OUT only in CC mode. Track-wide controls are shown in both.
- The NOTES and CC mode buttons in the header each carry an LED showing whether that output is on for the selected track (lit in green / cyan when on, dark grey when off). The LEDs are visible in both modes, so the hidden output's state is always known. The selected mode button is shown with a tinted background and accent outline, so the LED stays readable.
- **SLIDER**: VEL / GATE / PROB switch (note mode only).

### 12.3 Mouse interaction reference

This section lists every mouse action, control by control. On macOS, Alt is Option, Ctrl-click for selection is Cmd-click, and Ctrl-click also counts as a right-click.

**Knobs and step sliders** (track LENGTH, SWING, MIDI CH, CC number; per-step NOTE, OCTAVE; the VEL / GATE / PROB step sliders and the CC step sliders; the knobs in Settings and in the macro mapping editor)
- **Knobs, drag up / down**: change the value, relative to where the drag started.
- **Step sliders, click**: the value jumps to the click position. **Click and drag across neighbouring sliders** to draw values over several steps in one move (one undo step).
- **Shift + drag**: fine control (3 times slower).
- **Mouse wheel**: one step per notch.
- **Double-click**: type a value. Enter applies it; Esc or clicking elsewhere cancels. Notes can be typed as names (C, D#, A#).
- **Right-click or Alt + click**: reset to the default.
- Defaults are the init values (4.1): LENGTH 16, SWING 0%, MIDI CH = the track number, CC number = 102 + track index, NOTE F, OCTAVE = the octave of middle C, VELOCITY 100, GATE 100%, PROBABILITY 100%, CC value 64. Settings and macro-editor knobs reset to their own defaults.
- The NOTE knob moves through in-scale notes only when the track's scale is on, and its arc then spreads those notes over the whole travel (5); the OCTAVE knob covers the whole MIDI range (C-2 to G8 with middle C = C3; notes above MIDI 127 are not played).

**Buttons** (all on/off and choice buttons, lane buttons and tabs)
- **Click**: act. The action happens when the mouse button is released over the button; releasing elsewhere cancels.
- **Right-click**: does nothing.
- **Mouse wheel** on the DIVISION value: next / previous division (wheel up = shorter steps). On the scale name in the SCALE tile: next / previous scale.
- **SMOOTH**: click for one smoothing step; **press and hold** to repeat (after 0.4 s, about 14 times per second). One press or one hold is one undo step.

**LED bar** (above each row of steps)
- **Click**: select one step. **Drag**: select a run of steps.
- **Ctrl + click**: add or remove one step. **Ctrl + drag**: add a run.
- While **COPIED** is lit: a click only picks the landing step of the paste (no selection, no drag).
- Clicks beyond the track length do nothing.

**Step on/off button** (step number): click toggles the step.

**SHIFT ALL knob**: drag up / down to shift the chosen lane (Shift + drag = finer); **mouse wheel** shifts by one per notch. The knob springs back to 0 on release; the whole drag is one undo step. No double-click or reset.

**RANGES bars** (operations panel): click or drag; the nearest handle (MIN or MAX) follows the mouse. The handles cannot cross. No wheel, double-click or reset.

**Header**
- TRACK: click for the track list. NOTES / CC: click to switch.
- PATTERN: ‹ › step to the previous / next pattern (queued, following quantize); click the name for the pattern picker.
- Start / stop button in the BPM box: see 3.
- FILE (folder icon): presets menu (13.3). UNDO: one step back. MACROS: Macros page; reads "LEARN M005" while learning. SETTINGS: settings dialog.

**Patterns view**
- Pattern slot: click to select it (or, while COPIED / TRACK COPIED is lit, to pick the paste destination). The play button at the top center selects it and queues it.
- Chain row: drag the row (grip on the left) to reorder; - / + change the repeats; x removes the entry.

**Macros page**
- Macro cell: click to select. **Drag up / down** moves the macro (Shift + drag = finer), **mouse wheel** moves it one value (one in-scale note for a note target). Unmapped macros cannot be moved. No double-click or reset.
- LEARN: see 13.2. While learning, a click on a control in the step or patterns view maps it instead of changing it.

**Popovers** (DIVISION, SCALE, pattern picker): click an option to choose it; click outside to close.

**Keyboard**: no shortcuts, except typing into a knob after a double-click.

### 12.4 Operations panel (bottom, left to right)

1. **Selection**: the "LENGTH: 7, STEPS: 4" line, with COPY (COPIED when lit), PASTE and CLEAR (9.1).
2. **Lanes**: lane buttons shared by Randomize, Smooth and Copy (see 9.1, 9.2, 9.3).
3. **Ranges**: min/max bar per lane, in that lane's color. Ranges of lanes that are not selected are dimmed.
4. **RANDOMIZE** (filled, mode accent) and **SMOOTH** (outlined, mode accent).
5. **SHIFT ALL**: endless knob plus lane buttons (NOTE, OCT, VEL, GATE, PROB in note mode; CC in CC mode).

### 12.5 Mockups

- The interactive mockups live in the "Step Sequencer Mockups" design canvas: boards C (main window, note and CC mode), D (patterns view and chain), E (settings dialog), F (light theme) and G (macros). The old exploration boards A and B were deleted.
- The plugin name is still a placeholder ("[NAME]") in the mockups.

## 13. State and presets

- The full state (all patterns, track settings and global settings) is saved with the DAW project.
- Presets and macro mapping sets are saved to files (13.3).

### 13.1 Macros (host automation)

The host sees 128 parameters, "Macro 1" to "Macro 128", grouped in 4 groups of 32 (Macros 1-32 ... 97-128). They are the only automatable parameters. Each macro is mapped to one target in the plugin, so only what is needed is exposed. Mapping a DAW knob or controller to a macro is done in the DAW; VIMA has no MIDI learn of its own.

Targets:

| Type | Targets | Track | Pattern | Step |
|---|---|---|---|---|
| GLOBAL | Pattern (switches follow the quantize setting), Chain On | - | - | - |
| TRACK | Notes Out, CC Out | 1-8 | - | - |
| TRACK | Length, Division, Direction, Swing, Scale On | 1-8 | 01 to 64 | - |
| STEP | Note Step, Note, Octave, Velocity, Gate, Probability, CC Step, CC Value | 1-8 | 01 to 64 | 1-32 |

- **Every mapping is fixed**: it always points at one track, one pattern (for settings stored per pattern) and one step. It never follows the screen or the playing pattern, so DAW automation records and plays back exactly. (Mapping that follows the track or pattern on screen, for hardware controllers, is set aside for now.)
- **Full range**: every macro always covers its target's whole range over its whole travel. There is no MIN / MAX.
- **Note Step** and **CC Step** switch the step's note part and CC part on or off (5).
- **Note** follows the scale like the step's note knob: when the target track's scale is on, the macro moves only through the in-scale notes (C minor = 7 positions, C fully down to A# fully up, spread over the whole travel); with the scale off, through all 12. **Octave** covers the whole MIDI range.
- **Two-way**: a macro move from the host changes the target on the next audio block. When the target changes inside VIMA (editing it, undo, loading a preset, remapping, a pattern switch or chain step for the Pattern target), the macro moves to match, as a short gesture so hosts can record it.
- The host shows the target value as the macro's value text ("96", "1/16", "05"); unmapped macros show "-".
- Host changes are not added to undo. Mappings are saved with the project.

### 13.2 Macros page

Opened with the MACROS button in the header (between UNDO and SETTINGS); STEPS goes back. Mockup: board G.

- **Left**: page buttons (1 - 32, 33 - 64, 65 - 96, 97 - 128), "N OF 32 MAPPED", LEARN. A grid of 32 macro cells: number, color dot, a knob with the target's value (drag or wheel to move the macro), target name and where it points ("T04 · 06", "T02" for routing targets, "GLOBAL").
- **Right**: the mapping editor for the selected macro: TARGET (GLOBAL / TRACK / STEP; none lit for an unmapped macro), PARAMETER (a new mapping starts on the track and pattern on screen; step parameters in this order: NOTE STEP, NOTE, OCTAVE, VELOCITY, GATE, PROB, CC STEP, CC VALUE), TRACK (1-8), PATTERN (a pattern knob, 01 to 64), STEP (1-32), CLEAR MAPPING (unmaps the macro) and the current value.
- **LEARN**: press LEARN; the selected macro listens (if it is already mapped, the next free one does). Go back to STEPS (or the patterns view) and click a control: the listening macro is mapped to it (the track and pattern on screen at that moment, and that step, all fixed), and LEARN moves on to the **next free macro** (wrapping around; it never overwrites a mapping). When no macro is free, LEARN switches itself off. The header button reads "LEARN M005".
- While learning: clicks map instead of editing; an amber frame marks the view; the control under the mouse is highlighted; **every mapped control keeps an amber outline with its macro number** (for example "M005"), following the track and pattern on screen.
- **A mapped control cannot be mapped again**: clicking it does nothing (the outline flashes). **Right-click** a mapped control to free it (unmap its macro).
- Learnable: step on/off, note, octave, the slider (its current lane), CC value, LENGTH, DIVISION, DIRECTION, SWING, SCALE on, NOTE OUT, CC OUT, pattern slots (Pattern) and CHAIN on/off.
- **No duplicates in the editor either**: a change in the mapping editor that would point a second macro at a control that already has one is refused, and the footer shows "ALREADY ON M003".
- Outside LEARN, controls show **no indicator** when they are mapped to a macro.
- Colors follow the lanes: step on, note, octave and velocity green, gate purple, probability amber, CC cyan, track and global targets neutral.

### 13.3 Presets and macro mapping sets

- **Presets** hold the music: all 64 patterns, the chain, track routing and the musical settings (scale, root, user scale, middle C, CC smoothing, pattern change by CC, quantize, lanes and ranges), plus the current pattern. They do not hold macro mappings, the theme or the window size; those stay as they are when a preset loads.
- **FILE** button (folder icon) in the header, between the HOST box and UNDO, opens a menu: the current preset's name (or UNSAVED), NEW (asks first; clears the music, keeps mappings), LOAD..., SAVE (overwrites the current preset file), SAVE AS...
- LOAD and SAVE AS use the system's file dialogs, starting in **Documents/VIMA/Presets**. Files can be saved anywhere. Extension **.vima** (XML).
- Loading a preset clears undo. The current preset file is remembered with the DAW project.
- **Macro mapping sets** hold all 128 mappings. SAVE and LOAD buttons on the Macros page (next to LEARN), with file dialogs starting in **Documents/VIMA/Macros**. Extension **.vimamacros**. Loading a set replaces all mappings and leaves the music alone.
- DAW preset formats (Live .adv, .vstpreset) still work and store the whole state, mappings included.

## 14. Build and repository

- **No backwards compatibility while VIMA is in testing.** Projects, presets and mapping sets saved by earlier test builds may load wrongly or not at all; no conversion code is written for them.

- Repository on GitHub under the user's account (TheoGal). Repo name follows the final plugin name.
- GitHub Actions builds the plugin on every push: Windows VST3 (primary) and macOS VST3 (for testers). Build artifacts are attached to the workflow run, and to a GitHub Release when a version tag is pushed.
- JUCE version pinned in CMake.
- First milestone: a minimal test plugin that outputs MIDI on several channels, so the Live routing questions in section 2 can be tested before the full build.

## 15. Open questions

### 15.1 Decided, not built yet

Nothing at the moment.

### 15.2 Open

1. Approve the proposed patterns view and chain design (7.1, 7.2).
2. Probability and CC: should a step that fails its probability roll also skip its CC, or does CC always send? (Current assumption: probability affects notes only.)
3. Note randomize range: is the NOTE min/max a pitch-class range (for example C to A#) or a full note range across octaves (for example C2 to C4)?
4. Approve the redesigned Settings dialog (11).

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
- 2026-10-22: No backwards compatibility with earlier test builds (14).
- 2026-10-22: Macro mappings are always fixed (track, pattern, step). SHOWN / CURRENT removed everywhere; hardware-controller mapping that follows the screen is set aside for now. The shown track is no longer saved for macros.
- 2026-10-22: MIDI CH moved first in the parameter row, slightly apart from the per-pattern settings.
- 2026-10-22: Parameter row: track-wide controls on the left; mode-specific controls on the right with the output ON/OFF last; CC number in its own tile (CC NUM).
- 2026-10-22: Note and CC parts have separate on/off per step (shared length, division, direction, swing; an independent CC lane is a second track on the same channel). In CC mode the step buttons, the ON lane button, randomize, copy/paste and the macro target "CC ON" use the CC on/off. Thumbnails show both parts side by side.
- 2026-10-22: The STEPS button (back to the step view, in the patterns view and the Macros page) is filled green so it stands out.
- 2026-10-22: LEARN shows mapped controls (outline + macro number), refuses remapping, right-click frees, moves to the next free macro; the editor refuses duplicates. Built together with fixed-only mappings and the removal of the old-project code.
- 2026-10-22: Octave range setting removed: octaves cover the whole MIDI range; Settings keeps only the middle C convention for pitch.
- 2026-10-22: Built: init values (4.1), gate up to 400%, step slider click / draw, wheel on SHIFT ALL, middle C C3 / C4 / C5. Old projects keep their saved content.
- 2026-10-22: Middle C setting gets C5 (FL Studio, Cakewalk, Renoise); each choice shows its lowest C in brackets.
- 2026-10-22: Gate range 1-400% (percentage of a step, no click-in points). Step sliders: click jumps, drag draws. Every knob supports the mouse wheel. No INIT button: NEW in the FILE menu covers it. (To be built together with the new init values.)
- 2026-10-22: Mouse interaction reference written (12.3), as the base for the manual.
- 2026-10-22: Tested in Live 12 (Windows): Live strips the MIDI channel from a VST3's output, so in Live one VIMA instance per destination track is used. Accepted.
- 2026-10-22: Presets (music only, .vima) from a FILE menu in the header with system file dialogs; macro mapping sets (.vimamacros) saved / loaded on the Macros page; files in Documents/VIMA. The word HOST was removed from the header.
- 2026-10-22: No macro-mapped indicators on controls. Output LEDs on the NOTES / CC buttons are plain dots, no glow ring.
- 2026-10-22: Start / stop button in the HOST box (stop while the host plays, or run on its own while the host is stopped). Playhead on the LEDs only. Off steps within the length have a grey filled on/off button.
- 2026-10-22: Copy/paste redesign (9.1): COPY follows the armed lanes and lights up as COPIED; no selection while COPIED, an LED click picks the landing step with a ghost preview, PASTE writes and stays ready for more; COPIED or CLEAR empties the clipboard. Selection readout replaced by "LENGTH: n, STEPS: n".
- 2026-10-22: Built: no CC smoothing time (smoothing glides over the whole gap); the note knob arc spreads the in-scale notes over the whole arc; no CHROMATIC scale (13 + USER); macros always cover the full range (no MIN / MAX); step macros NOTE STEP and CC STEP; pattern slots select on click and play from their play button; patterns view copy / paste works like step copy / paste, CLEAR never erases; patterns numbered 1-64 in pages (no banks); no NONE button in the macro editor.
- 2026-10-23: CC mode: the per-step CC value is a slider (like the note-mode sliders, with drawing) instead of a knob.
- 2026-10-23: Transport button: a stop while the host plays holds VIMA (amber stop icon) through host stops and restarts, until VIMA's own play button; a stop while the host is stopped does not hold.
- 2026-10-21: Note macros are scale dependent, like the note knobs. The SHOWN track is saved with the project. No right-click mapping (right-click resets controls).
- 2026-10-21: Host automation through 128 mappable macros (13.1, 13.2) instead of fixed parameters. Step targets can use a fixed track and pattern, or SHOWN / CURRENT for controllers. No direct MIDI learn in VIMA. Scale and root are not targets.
- 2026-10-21: Reduced to 8 tracks and 64 patterns. Pattern change by CC now reaches every pattern.
- 2026-10-21: Build decision: musical track settings are stored per pattern; routing (channel, outputs, CC number) is global per track. Undo covers step and pattern edits, not routing changes.
