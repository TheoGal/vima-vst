# VIMA - Design Spec

Name: **VIMA** (from Greek "βήμα", step). Repository: TheoGal/vima-vst.

Status: **in testing**. Every push is built by GitHub Actions; testers use the CI builds. This file is the single source of truth: sections 1-13 describe the current design as built, section 14 holds the build details needed for further development, section 15 holds open questions and section 16 the history of decisions. It is also the main source for the PDF manual.

Last updated: 2026-10-23 (checked against the code at that date).

---

## 1. Identity

- An **8-track MIDI step sequencer** plugin with its own design. It is not a clone of Zaquencer or any other product, and does not follow their behavior.
- Strong focus on randomization and quick, destructive "happy accident" editing, with undo as the safety net.

## 2. Plugin format and hosting

- Built with JUCE and CMake (details in 14).
- Format: **VST3** only. Windows (x64) is the primary target. macOS (universal: Apple Silicon and Intel) builds are produced by CI for testers; the developer has no Mac. There is no Audio Unit version: an AU cannot send MIDI out of a plugin.
- The host sees VIMA as an **instrument (synth) plugin**, not a MIDI effect.
- It produces no audio. It outputs MIDI only (notes and CC). A silent stereo output bus exists only so hosts accept it as an instrument.
- Incoming MIDI is used only for pattern change by CC (6, 7.3); it is not passed through.
- In the DAW, other tracks receive VIMA's MIDI output (for example in Ableton Live: "MIDI From" = the VIMA track, the lower chooser = VIMA, monitor "In").

### 2.1 Ableton Live and MIDI channels (tested and confirmed)

- **Confirmed by testing (Live 12, Windows): Ableton Live strips the MIDI channel from MIDI data internally.** All 8 tracks of one VIMA instance arrive mixed together on the destination track, whatever their MIDI channels. So in Live there is **no native way to use a VST step sequencer with multiple MIDI channels** (one instance feeding several instruments by channel). Someone who wants that in Live would have to build it with **Max for Live**.
- How VIMA is used in Live: **one VIMA instance per destination track** (each instance still has 8 tracks, for example several drum voices or layers going to one instrument). Accepted as is.
- Hosts that keep MIDI channels (for example Bitwig, Reaper) can route one instance to several destinations by channel.

## 3. Transport and clock

- Follows the host: plays when the host plays, stops when the host stops, follows the host tempo, time signature and song position.
- **Step positions come from the song position**: step 1 of a track falls on song position 0 and on every multiple of the track's length (length x division) after it. Locating, looping and jumping in the host land on the matching step.
- **Start / stop button** (the play / stop icon at the left of the BPM box in the header):
  - **Stop while the host plays**: VIMA stops and is **held**. A held VIMA ignores the host stopping and starting again; it stays stopped until its own play button is pressed. The stop icon is **amber** while held.
  - **Play while the host plays**: VIMA starts again in sync with the host and the hold is released; from then on it follows the host.
  - **Play while the host is stopped**: VIMA runs on its own from step 1 at the host's tempo (this also releases a hold). Stopping it with the button, or the host starting, ends that. A stop made while the host is stopped does not hold: the next time the host plays, VIMA follows.
  - The hold is not saved: it is released when the project is reopened.
- The icon shows whether VIMA is playing (play icon, mode accent), stopped (grey stop icon) or held (amber stop icon). The BPM shown is the host's.
- On stop, on a jump and on an instant pattern switch, every sounding note gets its note-off.

## 4. Tracks

- 8 tracks, shown as "01" to "08". No track names.
- Selected with the **TRACK dropdown** in the header. Switching tracks clears the step selection.
- Every track always has both a **note part** and a **CC part**. They share the track's 32 steps, length, division, direction and swing, but each has **its own on/off per step** (5). The NOTES / CC switch in the header only changes which part is shown and edited (including which on/off the step buttons, the ON param button, Randomize and Copy use). It does not change what the track outputs; that is set by the NOTE OUT and CC OUT toggles.
- An independent CC rhythm (different length or division from the notes) is made with a second track on the same MIDI channel.

Where track settings live:

- **Per track, per pattern** (stored in each pattern, so patterns can differ): length, division, direction, swing, scale on/off, and all steps.
- **Per track, global** (the same in every pattern, because they describe where the track is routed): MIDI channel, notes output on/off, CC output on/off, CC number.

Per track parameters:

| Parameter | Range / values | Stored | Notes |
|---|---|---|---|
| MIDI channel | 1-16 | global | Freely assignable. Track N is not tied to channel N. |
| Notes output (NOTE OUT) | on / off | global | Independent of CC output. |
| CC output (CC OUT) | on / off | global | Independent of notes output. Sends on the same MIDI channel as the notes. |
| CC number (CC NUM) | 0-127 | global | One CC per track. |
| Length | 4-32 steps | per pattern | |
| Division | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 and triplets 1/2T, 1/4T, 1/8T, 1/16T, 1/32T | per pattern | |
| Direction | Forward, Backward, Ping-pong | per pattern | Ping-pong plays the end steps once (1 2 3 4 3 2 1 2 ...). |
| Swing | 0-100% | per pattern | 0% = straight. Every second step (counted from song position 0) is delayed by swing x half a step: 100% delays it by half a step. |
| Scale | on / off | per pattern | Use or bypass the global scale (6). |

### 4.1 Init values (= defaults everywhere)

A new instance, NEW in the FILE menu, and a right-click / Alt + click on any control use these values. All 64 patterns start like this.

| Setting | Init value |
|---|---|
| Notes output | on for track 1, off for tracks 2-8 |
| CC output | off on all tracks |
| MIDI channel | track number (1-8) |
| CC number | 102 + track index: tracks 1-8 = CC 102-109 (undefined CCs in the MIDI spec) |
| Length / division / direction / swing / scale | 16 / 1/16 / forward / 0% / on |
| Steps | all 32 on (note on and CC on), note F (the middle of the note knob's travel), octave of middle C (so F above middle C, MIDI 65), velocity 100, gate 100%, probability 100%, CC value 64 |
| Current pattern | 01 |
| Global scale / root / user scale | minor / C / natural minor shape |
| Middle C | C3 |
| CC smoothing | off |
| Pattern change by CC | off, channel 16, CC 20 |
| Quantize | 1 BAR |
| Chain | off, loop on, empty |
| PARAMS | NOTE armed in note mode, CC armed in CC mode (ALL off) |
| RANGES | full range of every param (NOTE C-B, OCT = all octaves, C-2 to 8 with middle C = C3, VEL 1-127, GATE 1-400%, PROB 0-100%, CC 0-127) |
| SHIFT ALL param / SLIDER / view | NOTE / VEL / NOTES |
| Macros | none mapped |
| Theme | dark |

Because every step starts on, every pattern holds content from the start (the patterns view shows them as STORED, 7.1).

## 5. Steps

Each track has 32 steps. Steps beyond the track length are shown dimmed and are not played.

Per step values:

| Value | Range | Notes |
|---|---|---|
| Note on / off | on / off | Whether the step plays its note. An off step still takes its time slot; it is not skipped. |
| CC on / off | on / off | Whether the step sends its CC value, **independent of the note on/off**: a step can send CC without a note, or a note without CC. |
| Note | C to B (pitch class) | Separate control from octave. With the track's scale on, only in-scale notes. |
| Octave | the whole MIDI range: C-2 to 8 (with middle C = C3) | Notes above MIDI 127 (G8) are not played. |
| Velocity | 1-127 | Never 0: in MIDI a Note On with velocity 0 is a Note Off. Every operation clamps velocity to 1-127. |
| Gate | 1-400% | Percentage of one step at the track's division: 100% = one step (for 1/16, a 1/16 note), 50% = half a step, up to 400% = four steps (legato and long notes). When a note is still held and the same pitch plays again on the same channel, the old note ends just before the new one. |
| Probability | 0-100% | Chance that an "on" step actually plays its note each time it is reached. 100% = always. **Strictly a note-mode parameter**: it is not shown in CC mode and never affects CC, which is always sent when the step's CC is on. |
| CC value | 0-127 | Sent when the track's CC output and the step's CC are on (10). |

Pitch display and scale behavior:

- **The scale is a master setting for everything note related**: when a track's scale is on, every note control, display and operation follows it (note knob and its arc, NOTE range, Randomize, Smooth, Shift, note macros, playback).

- The step's note knob and octave knob always show the note that will actually sound (scale applied, middle C setting applied to the octave number).
- When the track's scale is on, the note knob can only land on notes in the scale, and its arc follows the scale: the in-scale notes are spread evenly over the whole arc, whatever their number (7 in C minor, 3 in a 3-note user scale). The lowest in-scale note (from C up) is fully left, the highest fully right (A# in C minor). With the scale off, the arc covers all 12 notes, C to B.

There is no step skip, no chords per step and no drum mode.

## 6. Global settings

- **Scale**: 13 built-in scales plus 1 user scale, with a root note: Major, Minor, Harmonic minor, Melodic minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major pentatonic, Minor pentatonic, Blues, Whole tone, plus USER. There is no Chromatic scale: a track with its scale OFF plays all 12 notes. The scale and root apply to every track whose SCALE is on.
- The scale is **selected in the main window** (SCALE tile popover, 12.2), never in the Settings dialog. The root can be set in the SCALE popover and in the Settings user scale editor (the same global value).
- **User scale**: 12 note toggles. It is **edited only in the Settings dialog**, and **selected** (as "USER") from the main window.
- **Middle C** (display only): C3, C4 or C5 (11).
- **CC smoothing** on/off (10).
- **Pattern change from incoming CC**: on/off, MIDI channel and CC number (7.3).
- **Pattern change quantize**: OFF, 1 BEAT, 1 BAR or PATTERN END (7.3).

## 7. Patterns

- 64 patterns, numbered **1-64 everywhere** and shown as two digits ("05", "21"). The screens show them in 4 pages of 16: 1 - 16, 17 - 32, 33 - 48, 49 - 64.
- A pattern holds all 8 tracks (their per-pattern settings and steps).
- Patterns and single tracks can be copied to other patterns (7.1.2).
- Patterns can be chained (7.2).
- Patterns are picked from the header (7.4), the patterns view (7.1), the chain (7.2), incoming CC (7.3) and the Pattern macro (13.1).

### 7.1 Patterns view

- Opened with the "OPEN PATTERNS VIEW" button in the header's pattern popover (7.4). It replaces the step view; the green **STEPS** button in the header goes back.
- Global screen, so it uses the neutral color. Green and cyan only appear in the pattern thumbnails.
- **Pages**: a row of 4 buttons, 1 - 16, 17 - 32, 33 - 48, 49 - 64. A small dot marks the page holding the playing or queued pattern.
- **Pattern grid**: 4 x 4 slots for the current page. Each slot shows the pattern's own number (page 17 - 32 shows 17 to 32), a small **play button at the top center**, a state on the right (PLAYING with a lit LED and a thick outline, NEXT with a dashed outline, STORED, or EMPTY when no track has any step on), and a thumbnail of the 8 tracks (7.1.1). A thin line at the bottom marks patterns used in the chain.
- **Clicking a slot** anywhere except its play button only **selects** it (for COPY, COPY TRACK and + ADD to the chain). It does not play it.
- **The play button** selects the slot **and** queues it as the next pattern (7.3).
- **Selected bar** (bottom): SELECTED and the selected pattern's number, COPY, the TRACK dropdown, COPY TRACK, PASTE and CLEAR. On the right, an info text shows what is copied: "PATTERN 05" for a pattern, "PATTERN 05 - TRACK 02" for a track, nothing when the clipboard is empty.

#### 7.1.1 Pattern thumbnail

- 8 bars, one per track, track 01 on the left to track 08 on the right.
- **Height = density**: the number of steps that are on (within the length), divided by the track's length.
- **Which part and color = what the track outputs**:
  - Notes output on only: the note part's density, green.
  - CC output on only: the CC part's density, cyan.
  - Both on: two thin bars side by side, notes (green) on the left and CC (cyan) on the right, each at its own density.
  - Both outputs off: dim grey, at the busier part's density (content exists but is silent).
  - No steps on: a thin dark baseline.

#### 7.1.2 Copy / paste of patterns and tracks

Works like step copy / paste (9.1):

- **COPY** copies the whole selected pattern; **COPY TRACK** copies the selected pattern's track chosen in the TRACK dropdown. The button lights up and reads **COPIED** or **TRACK COPIED**. There is one clipboard: copying again replaces it.
- A copy takes **everything**: all steps (note and CC parts) and all settings stored in the pattern. Armed params do not apply here. Routing (channel, outputs, CC number) is not part of a pattern and is not copied.
- While a copy button is lit, a click on a slot only picks the **destination pattern**, shown with a dashed outline (the ghost preview). The selection does not change. The slot play buttons still play.
- For a track copy, the **destination track** is the one in the TRACK dropdown when PASTE is pressed, so track 1 can be pasted into track 2 of the same or another pattern.
- **PASTE** is dimmed until a destination is picked. It writes there (one undo step), and the button stays lit for more pastes.
- Clicking the lit button, or **CLEAR**, cancels: the clipboard is emptied and the ghost preview removed. **CLEAR never erases a pattern.** There is no "erase pattern" function; NEW in the FILE menu resets everything.

### 7.2 Chain

- An ordered list (song list) of up to 64 entries, shown on the right of the patterns view.
- Each entry: playing LED, position number, pattern number, length in bars (one play of the pattern times its repeats, in 4/4 bars) with a progress bar while playing, a repeat count (- / +, 1 to 99) and a remove button (x).
- **+ ADD 05** appends the selected pattern. **CLEAR** (under the chain) empties the chain. Entries can repeat the same pattern. The footer shows the number of entries and the total length.
- **CHAIN ON/OFF** turns chain playback on. **LOOP ON/OFF**: at the end of the chain, LOOP ON goes back to the first entry; LOOP OFF plays the chain once, then keeps playing (repeating) its last pattern until the transport stops or another pattern is picked.
- One play of a pattern lasts as long as its longest track (length x division).
- The chain **does not follow the song position**: when the host starts, the chain always starts from its first entry, wherever the host's playhead is. A jump while playing (locate or host loop) restarts the current entry from there. Steps inside a pattern still follow the song position (3), so timing stays in sync. (Decided to keep it this way: a song-position chain would make editing patterns from the DAW awkward.)
- Switching the chain on while playing queues its first entry (quantize applies).
- A pattern picked by hand while the chain plays (play button, header, CC, macro) switches as usual and then plays for the length of the current chain entry; after that the chain goes on with its next entry.
- **Reordering**: drag an entry up or down the list (a grip on its left shows it can be dragged). A line shows where it will land. Playback keeps following the entry that is playing, wherever it moves. Removing an entry also keeps playback on the right entry.

### 7.3 Pattern switching rules

- **Quantize OFF**: the switch happens the instant the pattern is picked (or the moment the pattern-change CC arrives), not at the next step. Each track continues at the step the song position gives (3), so timing stays in sync. Notes still sounding from the old pattern get their note-offs at the switch.
- **Quantize 1 BEAT / 1 BAR**: the picked pattern is queued ("NEXT") and switches at the next beat or bar of the host's time signature.
- **Quantize PATTERN END**: switches at the next multiple of the playing pattern's length, counted from song position 0.
- With quantize on, notes from the old pattern play out their gate.
- Picking another pattern before the switch replaces the queued one.
- While the transport is stopped, a picked pattern switches immediately.
- **Pattern change from incoming CC** (Settings, 11): when on, a CC message with the set number on the set channel picks a pattern: value 0 = pattern 01 ... value 63 = pattern 64. Values 64-127 are ignored.

### 7.4 Quick pattern picker (main window header)

- The PATTERN box in the header has **‹ and › arrows** for the previous / next pattern (crossing pages, wrapping from 64 to 01 and back). Each click queues that pattern (quantize applies).
- Clicking the pattern number opens a **popover** (styled like the DIVISION and SCALE popovers): a PAGE row (1 - 16 to 49 - 64; a dot marks the page with the playing or queued pattern), the 16 slots of that page with the patterns' own numbers (playing = outlined with a lit LED, queued = dashed outline, empty = dim), and an "OPEN PATTERNS VIEW" button.
- Picking a slot queues it (quantize applies) and closes the popover.
- While a pattern is queued, the PATTERN box keeps showing the current pattern and **blinks** until the switch happens. It does not show the next pattern's number. The same applies in the patterns view and on the Macros page.

## 8. Selection and the scope rule

### 8.1 The LED bar

- Above each row of 16 steps is an LED bar, one segment per step.
- It shows the **playhead**: the segment of the playing step lights in the mode accent (green in note mode, cyan in CC mode) while VIMA plays. The step cells do not change as it passes.
- Segments beyond the track length are dark and cannot be selected.
- It is also the **selection tool**:
  - Click and drag across segments to select a range. A single click selects that one step (and replaces the selection).
  - **Ctrl-click** (Cmd-click on Mac) adds a step to the selection, or removes it if it is already selected, so non-adjacent steps can be selected.
  - **Ctrl-drag** adds a range to the existing selection. A plain drag replaces it.
- Selection is shown **only on the LED bar** (white segments). The step cells themselves are not highlighted.
- CLEAR (operations panel) removes the selection. Switching tracks clears it.

### 8.2 Scope rule (applies to EVERY step operation)

- Every operation on steps (copy, randomize, smooth, shift, and any future step operation) works on **the selected steps if there is a selection**, otherwise on **all steps of the current track in the current pattern** (within its length).
- The Selection box shows one line, "LENGTH: 7, STEPS: 4": length = first to last step in scope, steps = how many. With nothing selected it describes all steps; while COPIED is lit it describes the clipboard.
- Operations on a non-adjacent selection act on exactly those steps (Smooth averages only those steps).

## 9. Step operations

Undo: the UNDO button in the header steps back, up to 64 levels. Undo covers everything stored in patterns: step edits, per-pattern track settings, copy / paste, randomize, smooth, shift and pattern pastes. One gesture is one undo entry (a whole knob drag, a slider drawing, one Randomize press, one Smooth press or hold, one paste). Routing (channel, outputs, CC number), global settings, armed params and ranges, the chain and macro mappings are not undone. Loading a preset or NEW clears the undo history. Host automation (macros) is not added to undo.

### 9.1 Copy / Paste (steps)

- Copy / paste is the way to move steps between tracks and patterns.
- **COPY** copies the steps in scope (the selection, or all steps within the length if none), but **only the params armed in PARAMS** (the same buttons as Randomize and Smooth; with ALL, every param of the mode). The params are fixed at the moment of copying.
- After COPY the button lights up and reads **COPIED**, and the selection is gone: while COPIED is lit there is no selection at all. A click on an LED only picks the **landing step** (where the first copied step goes); dragging and Ctrl-click do not select.
- Once a landing step is picked, a **ghost preview** on the LEDs shows where the steps will land. Copied steps **keep their spacing**: copying steps 1, 3 and 6 and landing at 9 writes steps 9, 11 and 14; the steps in the gaps are left unchanged.
- **PASTE** (dimmed until a landing step is picked) writes the copied params there. COPIED stays lit, so the user can pick another landing step, switch track or pattern, and paste again.
- Clicking **COPIED** empties the clipboard and turns it back into COPY. **CLEAR** deselects, and when COPIED is lit also empties the clipboard and removes the ghost preview.
- A paste that runs past the destination track's length is **cut off** at the last step. It never wraps around to step 1.
- While COPIED is lit, Randomize, Smooth and Shift act on all steps (there is no selection).

### 9.2 Randomize

- Which values change is chosen with the **PARAMS** buttons (shared with Smooth and Copy): ALL, ON, NOTE, OCT, VEL, GATE, PROB in note mode (ON = note on/off), or ALL, ON, CC in CC mode (ON = CC on/off). The LED bar chooses which steps; the PARAMS buttons choose which values of those steps.
- **Arming params**:
  - Clicking **ALL** lights only ALL: every param of the mode is armed, but the single buttons stay unlit.
  - While ALL is lit, clicking a param turns ALL off and arms **only that param**.
  - Outside ALL, the param buttons toggle one by one.
  - **Something is always armed**: clicking the only lit button (ALL, or the last armed param) does nothing.
  - Note mode and CC mode each keep their own ALL and params. Init: NOTE in note mode, CC in CC mode.
- Each param except ON has a min / max range (RANGES, 12.6). Random values stay inside it.
- **ON**: each step is switched on with a 70% chance.
- **NOTE**: a random note from the NOTE range, which is a pitch-class range (the same for every octave). With the track's scale on, only in-scale notes within the range (the NOTE bar then shows only in-scale notes, 12.6); if none is in range, any note.
- Applies to the steps in scope (8.2), including off steps.

### 9.3 Smooth

- A SMOOTH button pulls the values of the steps in scope one step closer to their average.
- For each armed param (same PARAMS buttons as Randomize):
  1. Calculate the average of that param's values over the steps in scope, **rounded to the nearest whole number**.
  2. Every value below the average goes up by 1, every value above it goes down by 1. Values equal to the average do not move.
- Example: CC values 0, 64, 127 give an average of 63.67, rounded to 64. One press gives 1, 64, 126.
- The average is recalculated on every press.
- Units of "one step" per param: NOTE 1 scale step (1 semitone when the scale is off), OCT 1 octave, VEL 1, GATE 1%, PROB 1%, CC 1.
- NOTE and octave are treated as one combined pitch for NOTE smoothing, so notes converge across octave boundaries.
- The ON param is ignored by Smooth (on/off has no average).
- Off steps are **excluded** from Smooth: they are not counted in the average and their values are not changed. (Randomize, by contrast, includes off steps.)
- Smooth ignores the Randomize ranges (it only moves values toward the average, never outside the current spread).
- Press once for one step. **Press and hold** repeats (after 0.4 s, about 14 times per second). One press or one hold is one undo entry.

### 9.4 Shift (SHIFT ALL)

A relative shift of all steps in scope.

- A param selector (its own buttons, separate from PARAMS) picks what the shift changes:
  - **NOTE**: ±1 per detent, in scale steps when the track's scale is on (semitones when it is off). Crossing the top or bottom of an octave rolls into the next or previous octave.
  - **OCT**: ±1 octave per detent.
  - **VEL**: ±1 per detent.
  - **GATE**: ±1% per detent.
  - **PROB**: ±1% per detent.
  - **CC** (CC mode, the only param there): ±1 CC value per detent.
- Controlled by an **endless knob**. While turning, it shows the running offset (for example "+3"). It returns to 0 when released, because it is a one-time action, not a stored setting.
- **Capping**: each step moves relative to its own value. A step that reaches its limit stays at the limit while the others keep moving.
- **Gesture snapshot**: during one drag, every step is calculated from its value at the moment the knob was grabbed. Turning back within the same drag restores the original shape exactly. Steps update live while dragging (so the change can be heard), and the whole drag is one undo entry.

## 10. CC output

- One CC per track (CC NUM), on the track's MIDI channel, when the track's CC OUT is on.
- Without smoothing: each step with its CC on sends its CC value at the step's time. Steps with the CC off send nothing.
- **Smoothing** (Settings, on / off): the CC glides from one CC-on step's value to the next, over the whole time between them, passing over CC-off steps. Messages are sent about every 32 samples, only when the value changes. There is no smoothing time setting.
- Probability does not affect CC (5).

## 11. Settings dialog

Opened with the SETTINGS button. A 600 x 700 dialog over the main window, same style. Sections, top to bottom:

- **APPEARANCE**: Theme, DARK / LIGHT.
- **PITCH**: "Middle C is" (how octaves are numbered on screen; MIDI note 60 is always middle C): **C3 (LOWEST C-2)**, **C4 (LOWEST C-1)** or **C5 (LOWEST C0)**. C3 matches Live, Bitwig, Logic, Cubase, Studio One, Pro Tools and Reason; C4 matches Reaper, Tracktion and scientific pitch notation; C5 matches FL Studio, Cakewalk and Renoise. Default C3. There is no octave range setting: octaves always cover the whole MIDI range.
- **USER SCALE**: a ROOT row (C to B; the same global root as in the SCALE popover: changing it in either place changes both) and a NOTES row of 12 toggles with a count ("7 OF 12 NOTES"). The toggles are relative to the root (the first toggle is the root, which is always included); their labels follow the root (root D shows D, D#, E, ...). Selected in the main window as "USER".
- **CC OUTPUT**: Smooth between steps ON / OFF (10).
- **PATTERN CHANGE**: From incoming CC ON / OFF, with MIDI CH and CC knobs (dimmed when off). Quantize changes to OFF, 1 BEAT, 1 BAR or PATTERN END (7.3).
- Footer: **RESET DEFAULTS** (resets the settings in this dialog to their init values: theme, middle C, root, user scale, CC smoothing, pattern change by CC and quantize; the selected scale is not changed) and **DONE**. A click outside the dialog also closes it.

## 12. UI

### 12.1 Look

- Modern, flat, "digital". Not skeuomorphic: no fake hardware, textures, shadows or 3D knobs.
- Fonts: IBM Plex Sans (labels) and IBM Plex Mono (values), embedded in the plugin.
- Two themes, **DARK** (default) and **LIGHT**, chosen in Settings. All colors are theme tokens, so both themes share one layout.
- DARK palette: background #0E1114, panels #12161B / #161A20, borders #222830 / #2A313B, text #EEF1F5 / #C5CBD4 / #8A94A3.
- LIGHT palette, built on a light grey, not white: background #D3D7DD, panels #DCE0E5, tiles #E3E6EA, alternate step groups #DADDE2, borders #C2C7CF / #B9BFC8 / #AEB5BF, text #11151A / #2C333D / #525B68, neutral #2A313B, selection LED #11151A. Accents are darkened for contrast: note green #12864A, CC cyan #0874AE, gate purple #6539C2, probability amber #A86600.
- Mode accents (dark theme): note mode green #4FD98A, CC mode cyan #5CC8FF. The accent colors the mode switch, lit step buttons, note arcs, the playhead LED, RANDOMIZE and SMOOTH.
- Param colors: velocity green (same as note mode), gate purple #B48CFF, probability amber #FFB547, CC cyan, octave light grey #C5CBD4.
- Color tells which part a control affects:
  - **Track-wide and global** controls (LENGTH, DIVISION, DIRECTION, SWING, MIDI CH, the patterns view, the Macros page) use a fixed neutral (#DDE2E8 dark) and never change color with the mode.
  - **Note part** controls (SCALE, NOTE OUT) are green.
  - **CC part** controls (CC NUM, CC OUT) are cyan.
- Amber also marks LEARN (13.2) and a held transport (3).
- Selection: white #EEF1F5 on the LED bar (dark theme).
- No explanatory or help text in the main window.
- Base window size 1200 x 720. **Resizable**: the window scales as a whole (proportions fixed), from 75% (900 x 540) to 200% (2400 x 1440), by dragging the corner. The size is saved with the project.

### 12.2 Layout (top to bottom)

1. **Header**, left to right: VIMA; then in the step view the TRACK dropdown and the NOTES / CC mode switch, or in the patterns view and on the Macros page the green **STEPS** button (back to the step view; the Macros page also shows the title MACROS); then on the right: the PATTERN box (7.4), the BPM box with the start / stop button (3), FILE (folder icon, 13.3), UNDO, MACROS (13.2) and SETTINGS (11).
2. **Track parameter row** (12.4).
3. **Step grid**: 2 rows of 16 steps, each row with its LED bar above it. Steps are grouped in fours by alternating tile shades.
4. **Operations panel** (12.6).

The NOTES and CC mode buttons each carry an LED showing whether that output is on for the selected track (lit green / cyan when on, dark grey when off), visible in both modes. The selected mode button has a tinted background and an accent outline.

### 12.3 Step cell

- **Top**: one button with the step number inside, used as the on/off toggle of the shown part (note on/off in note mode, CC on/off in CC mode). On: filled with the mode accent, number in a dark contrasting color. Off (within the length): filled grey. Beyond the length: outline only, and the whole cell dimmed.
- **Note mode**: note knob, octave knob below it, then a vertical slider with its value under it. The **SLIDER** switch in the parameter row makes all sliders show velocity, gate or probability. The slider takes that param's color, and so does the value under it.
- **CC mode**: one tall cyan slider for the step's CC value (0-127), filling the cell below the step button, with the value under it in cyan.
- When the shown part of a step is off, its controls and value are dimmed.
- Knobs are flat arc rings (270 degrees) with the value as text in the middle. Cells keep the same height in both modes so the layout does not jump.

### 12.4 Track parameter row

- **Layout**: on the left the track-wide controls: first **MIDI CH** (the same in every pattern), then a small extra gap, then the settings stored per pattern: LENGTH, DIVISION, DIRECTION, SWING. On the right, aligned to the right edge, the mode-specific controls with the output ON/OFF always last: note mode SCALE, SLIDER, NOTE OUT; CC mode CC NUM, CC OUT.
- Part-specific controls are only shown in their own mode, to avoid accidents. Track-wide controls are shown in both.
- **Numeric values** (MIDI CH, LENGTH, SWING, CC NUM): small arc knobs with the value inside. LENGTH also shows "steps".
- **DIVISION**: click opens a popover grid with the straight values on one row and the triplets on a second row. The mouse wheel on the tile steps through the values.
- **DIRECTION**: three inline icon buttons (forward, backward, ping-pong).
- **SCALE**: a scale-name button (for example "C MIN") and a separate ON/OFF toggle for the current track (stored per pattern). Clicking the name opens a popover with a "GLOBAL" tag (the choice applies to every track with SCALE on), a ROOT row (C to B) and a grid of the 13 scales plus USER. Picking a scale closes the popover. The mouse wheel over the name steps through the scales.
- **SLIDER**: VEL / GATE / PROB (note mode only; it changes the view, not the music).
- **NOTE OUT** / **CC OUT**: on/off toggles.
- Only one popover is open at a time.

### 12.5 Mouse interaction reference

On macOS, Alt is Option, Ctrl-click for selection is Cmd-click, and Ctrl-click also counts as a right-click.

**Knobs and step sliders** (MIDI CH, LENGTH, SWING, CC NUM; per-step NOTE and OCTAVE knobs; the VEL / GATE / PROB / CC step sliders; the knobs in Settings and in the macro mapping editor)
- **Knobs, drag up / down**: change the value, relative to where the drag started. **Shift + drag**: fine control (3 times slower).
- **Step sliders, click**: the value jumps to the click position. **Click and drag across neighbouring sliders** to draw values over several steps of the same row in one move (one undo step).
- **Mouse wheel**: one value per notch (the NOTE knob: one in-scale note when the scale is on).
- **Double-click**: type a value. Enter applies it; Esc or clicking elsewhere cancels. Notes can be typed as names (C, D#, A#).
- **Right-click or Alt + click**: reset to the default.
- Defaults are the init values (4.1): MIDI CH = the track number, LENGTH 16, SWING 0%, CC NUM = 102 + track index, NOTE F, OCTAVE = the octave of middle C, VELOCITY 100, GATE 100%, PROBABILITY 100%, CC value 64. Settings and macro-editor knobs reset to their own defaults.

**Buttons** (on/off and choice buttons, PARAMS buttons, tabs)
- **Click**: the action happens when the mouse button is released over the button; releasing elsewhere cancels.
- **Right-click**: does nothing (except on mapped controls while LEARN is on, 13.2).
- **Mouse wheel** on the DIVISION tile: next / previous division (wheel up = shorter steps). On the scale name: next / previous scale.
- **SMOOTH**: click for one smoothing step; **press and hold** to repeat (9.3).

**LED bar**
- **Click**: select one step. **Drag**: select a run of steps.
- **Ctrl + click**: add or remove one step. **Ctrl + drag**: add a run.
- While **COPIED** is lit: a click only picks the landing step of the paste (no selection, no drag).
- Clicks beyond the track length do nothing.

**Step button** (step number): click toggles the shown part of the step on / off.

**SHIFT ALL knob**: drag up / down to shift the chosen param (Shift + drag = finer); **mouse wheel** shifts by one per notch. The knob springs back to 0 on release; the whole drag is one undo step. No double-click or reset.

**RANGES bars**: click or drag; the nearest handle (min or max) follows the mouse. The handles cannot cross. No wheel, double-click or reset.

**Header**
- TRACK: click for the track list. NOTES / CC: click to switch.
- PATTERN: ‹ › queue the previous / next pattern; click the number for the pattern picker.
- Start / stop button: see 3.
- FILE: presets menu (13.3). UNDO: one step back (9). MACROS: Macros page; reads "LEARN M005" in amber while learning. SETTINGS: settings dialog.

**Patterns view**
- Pattern slot: click to select it (or, while COPIED / TRACK COPIED is lit, to pick the paste destination). The play button at the top center selects it and queues it.
- Chain row: drag the row (grip on the left) to reorder; - / + change the repeats; x removes the entry.

**Macros page**
- Macro cell: click to select. **Drag up / down** moves the macro (Shift + drag = finer), **mouse wheel** moves it one value (one in-scale note for a note target). Unmapped macros cannot be moved. No double-click or reset.
- LEARN: see 13.2.

**Popovers** (DIVISION, SCALE, pattern picker): click an option to choose it; click outside to close.

**Keyboard**: no shortcuts, except typing into a knob after a double-click.

### 12.6 Operations panel (bottom, left to right)

1. **SELECTION**: the "LENGTH: 7, STEPS: 4" line, with COPY (COPIED when lit), PASTE and CLEAR (9.1).
2. **PARAMS**: buttons shared by Randomize, Smooth and Copy: ALL, ON, NOTE, OCT, VEL, GATE, PROB in note mode; ALL, ON, CC in CC mode. ALL is its own state and something is always armed (9.2).
3. **RANGES**: a min / max bar per param, in that param's color (OCT in grey): NOTE, OCT, VEL, GATE, PROB in note mode; CC in CC mode. Ranges are global (shared by all tracks). **Ranges of unarmed params are dimmed** (none with ALL on). The **NOTE bar follows the scale**: when the current track's scale is on, it shows only the in-scale notes, spread evenly over the bar (like the note knob arc, 5), so C minor runs from C to A#; with the scale off, C to B. The range is stored as notes (a pitch-class range), so it keeps its meaning when the scale changes.
4. **RANDOMIZE** (filled, mode accent) and **SMOOTH** (outlined, mode accent).
5. **SHIFT ALL**: endless knob plus its own param buttons (NOTE, OCT, VEL, GATE, PROB in note mode; CC only in CC mode).

### 12.7 Mockups

- The early interactive mockups live in the "Step Sequencer Mockups" design canvas: boards C (main window), D (patterns view and chain), E (settings), F (light theme) and G (macros). They are **behind the plugin**: the built plugin and this spec are the reference, not the mockups.

## 13. State, macros and presets

- The full state is saved with the DAW project: all patterns, routing, global settings, chain, armed params and ranges, macro mappings and the current pattern, plus per-instance view settings: the window size, the track shown in the editor and the current preset file.
- Not saved: the NOTES / CC view, the selection, clipboards, undo history, LEARN and the transport hold.

### 13.1 Macros (host automation)

The host sees 128 parameters, "Macro 1" to "Macro 128", in 4 groups of 32 (Macros 1-32 ... 97-128). They are the **only automatable parameters**. Each macro can be mapped to one target in the plugin, so only what is needed is exposed. Mapping a DAW knob or controller to a macro is done in the DAW; VIMA has no MIDI learn of its own.

Targets:

| Type | Targets | Track | Pattern | Step |
|---|---|---|---|---|
| GLOBAL | Pattern (switches follow the quantize setting), Chain On | - | - | - |
| TRACK | Notes Out, CC Out | 1-8 | - | - |
| TRACK | Length, Division, Direction, Swing, Scale On | 1-8 | 01 to 64 | - |
| STEP | Note Step, Note, Octave, Velocity, Gate, Probability, CC Step, CC Value | 1-8 | 01 to 64 | 1-32 |

- **Every mapping is fixed**: it always points at one track, one pattern (for settings stored per pattern) and one step. It never follows the screen or the playing pattern, so DAW automation records and plays back exactly. (Mapping that follows the track or pattern on screen, for hardware controllers, is set aside for now.)
- **Full range**: every macro always covers its target's whole range over its whole travel. There is no min / max per mapping.
- **Note Step** and **CC Step** switch the step's note part and CC part on or off (5).
- **Note** follows the scale like the step's note knob: when the target track's scale is on (in the mapped pattern), the macro moves only through the in-scale notes (C minor = 7 positions, C fully down to A# fully up, spread over the whole travel); with the scale off, through all 12. **Octave** covers the whole MIDI range.
- **Two-way**: a macro move from the host changes the target on the next audio block. When the target changes inside VIMA (editing it, undo, loading a preset, remapping, a pattern switch or chain step for the Pattern target), the macro moves to match, as a short gesture so hosts can record it.
- The host shows the target value as the macro's value text ("96", "1/16", "05"); unmapped macros show "-".
- Host changes are not added to undo.

### 13.2 Macros page

Opened with the MACROS button in the header; STEPS goes back.

- **Left**: page buttons (1 - 32, 33 - 64, 65 - 96, 97 - 128), "N OF 32 MAPPED", SAVE and LOAD (13.3), LEARN. A grid of 32 macro cells: macro number ("M005"), a color dot when mapped, a knob with the target's value (drag or wheel to move the macro), the target name ("STEP 05 VEL") and where it points ("T04 · 06" = track 4, pattern 06; "T02" for routing targets; "GLOBAL").
- **Right**: the mapping editor for the selected macro ("M017", with its host name "MACRO 17"):
  - **TARGET**: GLOBAL / TRACK / STEP (none lit for an unmapped macro).
  - **PARAMETER**: the targets of that type. Step parameters in this order: NOTE STEP, NOTE, OCTAVE, VELOCITY, GATE, PROB, CC STEP, CC VALUE. A new mapping starts on the track and pattern on screen.
  - **TRACK** (1-8), **PATTERN** (a knob, 01 to 64, for targets stored per pattern), **STEP** (a knob, 1-32, for step targets).
  - Footer: **CLEAR MAPPING** (unmaps the macro) and the current value ("NOW 16").
- **LEARN**: press LEARN; the selected macro listens (if it is already mapped, the next free one does). Go back to STEPS (or the patterns view) and click a control: the listening macro is mapped to it (the track and pattern on screen at that moment, and that step, all fixed), and LEARN moves on to the **next free macro** (wrapping around; it never overwrites a mapping). When no macro is free, LEARN switches itself off. The header button reads "LEARN M005" in amber. A banner on the Macros page explains it while listening.
- While learning: clicks map instead of editing; an amber frame marks the view; the control under the mouse is highlighted; **every mapped control keeps an amber outline with its macro number** (for example "M005"), following the track and pattern on screen.
- **A mapped control cannot be mapped again**: clicking it does nothing (the outline flashes). **Right-click** a mapped control to free it (unmap its macro).
- Learnable: step on/off (note or CC, following the mode), note, octave, the step slider (its current param: velocity, gate, probability, or CC value in CC mode), LENGTH, DIVISION, DIRECTION, SWING, SCALE on, NOTE OUT, CC OUT, pattern slots (Pattern) and CHAIN ON.
- **No duplicates in the editor either**: a change in the mapping editor that would point a second macro at a control that already has one is refused, and the footer shows "ALREADY ON M003".
- Outside LEARN, controls show **no indicator** when they are mapped to a macro.
- Colors follow the params: note step, note, octave and velocity green, gate purple, probability amber, CC step and CC value cyan, track and global targets neutral.

### 13.3 Presets and macro mapping sets

- **Presets** hold the music: all 64 patterns, the chain, track routing and the musical settings (scale, root, user scale, middle C, CC smoothing, pattern change by CC, quantize, armed params and ranges), plus the current pattern. They do not hold macro mappings, the theme or the window size; those stay as they are when a preset loads.
- **FILE** button (folder icon) in the header, between the BPM box and UNDO, opens a menu: the current preset's name ("PRESET: SONG", or "PRESET: UNSAVED"), NEW (asks first; resets the music to the init values, keeps mappings and theme), LOAD..., SAVE (overwrites the current preset file; dimmed when there is none), SAVE AS...
- LOAD and SAVE AS use the system's file dialogs, starting in **Documents/VIMA/Presets**. Files can be saved anywhere. Extension **.vima** (XML).
- Loading a preset or NEW clears undo. The current preset file is remembered with the DAW project.
- **Macro mapping sets** hold all 128 mappings. SAVE and LOAD buttons on the Macros page, with file dialogs starting in **Documents/VIMA/Macros**. Extension **.vimamacros** (XML). Loading a set replaces all mappings and leaves the music alone.
- DAW preset formats (Live .adv, .vstpreset) also work and store the whole state, mappings included.

## 14. Build and development notes

- **No backwards compatibility while VIMA is in testing.** Projects, presets and mapping sets saved by earlier test builds may load wrongly or not at all; no conversion code is written for them.

### 14.1 Toolchain

- CMake 3.22+, C++17, **JUCE 8.0.8** (downloaded by CMake FetchContent, or a local checkout with `-DJUCE_DIR=/path/to/JUCE`).
- Plugin settings (CMakeLists.txt): product "Vima", company "TheoGal", bundle ID `gr.cnnbl.vima`, manufacturer code `Thgl`, plugin code `Vima`, VST3 only, IS_SYNTH, MIDI in and MIDI out, VST3 categories "Instrument Generator". **Do not change the codes or the bundle ID**: hosts would see a different plugin.
- Windows builds use the static MSVC runtime, so the .vst3 works without the VC++ redistributable.
- The IBM Plex fonts are embedded as binary data (Resources/fonts, SIL Open Font License).
- License: AGPL-3.0.

### 14.2 CI (GitHub Actions, .github/workflows/build.yml)

- Runs on every push to main, on pull requests and on demand. A tag like `v1.0.0` also publishes both zips to a GitHub Release.
- **tests** (Linux): builds and runs the test harness under xvfb; uploads the rendered screenshots as "Vima-test-screenshots".
- **windows**: builds the x64 VST3, runs pluginval (strictness 10; does not fail the build), uploads the folder **Vima-Windows-x64** (the download is a single zip; uploading a zip gave a zip inside a zip).
- **macos**: builds a universal VST3 (macOS 10.13+), signs it ad-hoc (needed for Apple Silicon; not notarized), runs pluginval, and uploads **Vima-macOS-universal** containing `Vima-macOS-plugin.zip` plus INSTALL-macOS.txt (the inner zip keeps file permissions, which GitHub's artifact zip drops; its own name avoids a clash when unzipping).
- Install: Windows `C:\Program Files\Common Files\VST3\`; macOS `/Library/Audio/Plug-Ins/VST3/`, then `xattr -dr com.apple.quarantine` on the bundle.

### 14.3 Tests

- `tests/Tests.cpp`, built with `-DVIMA_BUILD_TESTS=ON` as the console app `VimaTests`. Run: `VimaTests <output folder>` (under `xvfb-run -a` on Linux).
- It checks the model, engine (timing, gate, swing, CC, smoothing, transport hold with a simulated host), operations, macros, presets, and drives the real editor with simulated mouse clicks (step buttons, sliders, copy / paste, patterns view, LEARN, Macros page). It also renders screenshots of every screen into the output folder. About 200 checks; all must pass.

### 14.4 Source map (Source/)

- **Model.h/.cpp**: the data model (Step, TrackPattern, Pattern, TrackRouting, Globals, Chain, MacroMap, Project), constants, scales, divisions, serialization.
- **Engine.h/.cpp**: audio-thread sequencer: host position to steps, notes, CC, smoothing, pattern switching, chain.
- **Operations.h/.cpp**: step edits, scope, randomize, smooth, shift, copy / paste, undo history.
- **Macros.h/.cpp**: macro targets: info, reading and writing values, value text. **MacroParams.h/.cpp**: the 128 host parameters and two-way sync.
- **PluginProcessor**: state, locking, transport button, presets, host state. **PluginEditor**: the window, scaling, LEARN overlay.
- **Context.h**: shared UI state (UiState) and helpers for all components.
- UI components: **Header**, **ParamRow**, **StepGrid** (step cells, LED bars, sliders), **OpsPanel**, **PatternsView** (slots, chain), **MacrosView**, **SettingsDialog**, **Popovers**, **Widgets** (Pill buttons, Knob), **Theme** (colors, fonts, drawing helpers).

### 14.5 Threading

- The Project is shared between the message thread (UI) and the audio thread, guarded by one `juce::SpinLock`, held only briefly.
- Read with `proc.read (fn)`, change with `proc.edit (undoPattern, fn)` / `ctx.edit (withUndo, fn)`. **Never nest them**: the SpinLock is not recursive and nesting deadlocks. Compute values outside the lambdas.
- Every edit bumps `revision`; the UI refreshes from it. A 20 Hz message-thread timer syncs macro values to the host.
- Message thread to audio thread: `engine.queuedPattern` (atomic). Audio thread to UI: play positions and chain progress (atomics).

### 14.6 Engine details

- Stateless timing: for each track, step number n = floor(song position / step length); the step index is n modulo the length (direction applied). Swing delays odd n.
- Notes are scheduled sample-accurately with pending note-offs; a same-pitch retrigger on the same channel releases the old note first. A jump is detected when the new block's position differs from the expected one by more than 0.02 quarter notes: all notes are released and CC state reset.
- While the host is stopped, VIMA's own run (3) counts its position from 0 at the host tempo.
- Pattern switches can happen mid-block; up to 8 switches per block are handled.

### 14.7 State format

- Project state (DAW): XML root `VIMA`, `version` 2, with `currentPattern`, a `Globals` child (scale, root, userScale, middleC, lightTheme, ccSmoothing, patternCC..., quantize, chainOn, chainLoop, chain as "pattern:repeats,...", armed params as 10 flags (note-mode and CC-mode params, ALL for each mode; stored as the `lanes` property), ranges as "min:max,..."), 8 `Routing` children, `Macro` children (only mapped ones: index, target, track, pattern, step) and `Pattern` children (only patterns that differ from the init pattern). Each pattern has 8 `Track` children with length, division, direction, swing, scaleOn and `steps`: 32 steps x 9 bytes, base64 (on, pitch class, octave + 2, velocity, gate low, gate high, probability, CC, CC on). The DAW state also stores uiScale, uiTrack and presetPath.
- Preset (.vima): the same `VIMA` tree with `content="preset"`, without `Macro` children and without the theme.
- Mapping set (.vimamacros): XML root `VIMAMACROS`, `version` 1, with `Macro` children.
- Host parameter IDs: `macro001` ... `macro128` (version 1), names "Macro 1" ... "Macro 128". **Changing the IDs breaks saved DAW automation.**

## 15. Open questions

### 15.1 Decided, not built yet

Nothing at the moment.

### 15.2 Open

Nothing at the moment.

## 16. Decision log

History of decisions, oldest first. Later entries override earlier ones; sections 1-14 describe the current design.

- 2026-10-20: Instrument plugin (not MIDI effect), VST3, built by GitHub Actions. Follow host transport. Swing per track.
- 2026-10-20: 16 tracks with assignable MIDI channels and independent note and CC outputs on the same channel. (Later reduced to 8 tracks.)
- 2026-10-21: Main window is the hybrid layout (board C). Track dropdown, no names, no sidebar.
- 2026-10-21: LED bar is playhead and selection. Selection shown only on the LEDs.
- 2026-10-21: Scope rule: every step operation acts on the selection, or all steps if none.
- 2026-10-21: Per-track transpose replaced by SHIFT ALL with a param selector, capping and gesture snapshot.
- 2026-10-21: Probability added per step and as a param. Colors: note mode and velocity green, gate purple, probability amber, CC cyan.
- 2026-10-21: Mutate removed, replaced by Smooth (each press moves values one step toward their rounded average).
- 2026-10-21: Track-wide parameters use a fixed neutral color; note-part controls green, CC-part controls cyan. Part-specific controls only shown in their own mode. Output LEDs on the NOTES / CC buttons.
- 2026-10-21: No help text in the main window. Theme switching lives in Settings.
- 2026-10-21: The design is its own; no reference to Zaquencer behavior.
- 2026-10-21: Scale and root are picked from a popover on the SCALE tile. The user scale is edited in Settings.
- 2026-10-21: Pattern quantize has an OFF option (instant switch, same playhead position). Patterns can be picked in the header (arrows and popover). A queued change blinks the current pattern number.
- 2026-10-21: Pattern thumbnails: bar height = density, color = output.
- 2026-10-21: Name: VIMA. Repository TheoGal/vima-vst.
- 2026-10-21: Window resizing (75%-200%, uniform scaling), chain reordering by dragging, Ctrl-click / Ctrl-drag selection, pasting keeps the spacing of copied steps.
- 2026-10-21: Reduced to 8 tracks and 64 patterns. Pattern change by CC reaches every pattern.
- 2026-10-21: Musical track settings are stored per pattern; routing (channel, outputs, CC number) is global per track. Undo covers pattern content, not routing.
- 2026-10-21: Host automation through 128 mappable macros instead of fixed parameters. No direct MIDI learn in VIMA. Scale and root are not macro targets. Note macros follow the scale like the note knobs.
- 2026-10-22: Macro mappings are always fixed (track, pattern, step); following the screen (SHOWN / CURRENT) was removed.
- 2026-10-22: No backwards compatibility with earlier test builds (14).
- 2026-10-22: LEARN shows mapped controls (outline + macro number), refuses remapping, right-click frees, moves to the next free macro; the editor refuses duplicates. No macro indicators outside LEARN.
- 2026-10-22: Note and CC parts have separate on/off per step (shared length, division, direction, swing). In CC mode the step buttons, the ON param, randomize, copy / paste and the CC Step macro use the CC on/off. Thumbnails show both parts.
- 2026-10-22: Parameter row: MIDI CH first, then the per-pattern settings; mode-specific controls on the right with the output ON/OFF last; CC number in its own tile (CC NUM).
- 2026-10-22: The STEPS button (patterns view, Macros page) is filled green.
- 2026-10-22: Octave range setting removed; octaves cover the whole MIDI range. Middle C setting C3 / C4 / C5 with the lowest C shown.
- 2026-10-22: Gate 1-400% of a step. Init values defined (4.1); right-click resets to them. Step sliders: click jumps, drag draws. Every knob supports the mouse wheel. No INIT button: NEW covers it.
- 2026-10-22: Mouse interaction reference written (12.5), as the base for the manual.
- 2026-10-22: Tested in Live 12 (Windows): Live strips the MIDI channel from a VST3's output; one VIMA instance per destination track in Live. Accepted.
- 2026-10-22: Presets (music only, .vima) from a FILE menu in the header; macro mapping sets (.vimamacros) on the Macros page; files in Documents/VIMA. The word HOST removed from the header.
- 2026-10-22: Start / stop button next to the BPM. Playhead on the LEDs only. Off steps within the length have a grey filled step button.
- 2026-10-22: Step copy / paste redesign (9.1): armed params, COPIED, landing step with ghost preview, PASTE stays ready; selection readout "LENGTH: n, STEPS: n".
- 2026-10-22: No CC smoothing time; the note knob arc spreads the in-scale notes; no CHROMATIC scale; macros always cover the full range; step macros NOTE STEP and CC STEP; pattern slots select on click and play from their play button; patterns view copy / paste works like step copy / paste and CLEAR never erases; patterns numbered 1-64 in pages (no banks); no NONE button in the macro editor.
- 2026-10-23: CC mode: the per-step CC value is a slider (like the note-mode sliders, with drawing) instead of a knob. Slider values are shown in their param's color.
- 2026-10-23: Transport button: a stop while the host plays holds VIMA (amber stop icon) through host stops and restarts, until VIMA's own play button; a stop while the host is stopped does not hold.
- 2026-10-23: The chain does not follow the song position (always starts from its first entry); kept on purpose.
- 2026-10-23: Confirmed: Ableton Live strips MIDI channels internally, so a VST step sequencer cannot drive several channels natively in Live; Max for Live would be the way to build that. VIMA stays one instance per destination track in Live.
- 2026-10-23: Spec checked against the code and rewritten to match the build.
- 2026-10-23: Probability is strictly a note-mode parameter (UI and function); it never affects CC. The scale is the master setting for every note-related control and operation. Queued: scale-aware NOTE range bar, dimmed ranges for unarmed lanes.
- 2026-10-23: "Lanes" renamed "params" everywhere (UI: PARAMS). ALL is its own state; something is always armed (CC mode starts with CC armed). Ranges of unarmed params are dimmed. The NOTE range bar follows the scale.
