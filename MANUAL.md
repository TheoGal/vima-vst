# VIMA User Manual

**[Screenshot]** The full window in the dark theme, note mode, with a busy pattern. *(main-dark-notes.png)*

## 1. Vima - the VST step sequencer to kill all VST step sequencers

**VIMA** (from the Greek word "βήμα", which means **step**) is an **8-track MIDI step sequencer** that runs as a plugin (VST3) inside your DAW. It makes no sound of its own: it plays notes and CC (controller) messages into the instruments and effects you already use. Its focused on being easy to use while still providing extensive capabilities in an unclutttered UI.

Each of its 8 tracks has up to 32 steps, its own length, timing and direction, and can play notes, send a CC value, or both at once. You can store 64 patterns, chain them into a song, switch between them live, and automate almost everything from your DAW.

The heart of VIMA is what I like to call **guided randomization**. Randomness is a great way to find ideas you would never have played yourself, but pure chance is rarely musical. In VIMA you steer it:

- you choose **which steps** change (the selection),
- you choose **which values** change (note, octave, velocity, gate, probability, CC),
- you choose **the limits** they move within (the ranges),
- you can shape the parameters: apply pre-defined shapes smooth them, draw a curve over them, etc.

Editing is quick and direct. Nothing asks "are you sure?" when you randomize: you try, you listen, and if you do not like it, UNDO is one click away.

---
## 2. Who am I

I have been a MIDI freak for a long time. Under the name Softcore I wrote electronic music and built a lot of software little helpers (M4L, Lemur templates): Microtonic Lemurized (2013), was a Lemur template which turned Sonic Charge's Microtonic into a touch instrument with tilt-to-morph, a running step light that followed the host's clock, and randomize scripts with per-parameter exclusion; its developer, Magnus Lidström, called it "absolutely amazing" (https://soniccharge.com/forum/topic/383-microtonic-lemuriized). I was one of the first to buy Zaquencer, and Bome MIDI Translator glued my setups together for years.

So, even though I've tipped my toes into coding I am not a C++ developer.
VIMA was vibe-coded:  I designed it, decision by decision, and an AI (Claude, by Anthropic) wrote the code, which I tested in real sessions and sent back with notes until it behaved the way I wanted. Every choice you will find in Vima was made on purpose. Throughout this manual I'll be explaining various design choices and the reasoning behind them.

---

## 3. Installing and setting up

- **Windows:** copy the `Vima.vst3` folder to `C:\Program Files\Common Files\VST3\`.
- **macOS:** copy `Vima.vst3` to `/Library/Audio/Plug-Ins/VST3/`. Test builds are not notarized by Apple, so the first time you may need to remove the quarantine flag (see INSTALL-macOS.txt in the download).

VIMA is a VST3 plugin only. There is no Audio Unit version, because an AU plugin cannot send MIDI out.

### Getting MIDI out of VIMA

Your DAW lists VIMA as an **instrument**. Put it on its own track, then route that track's MIDI output to the instruments you want to play.

>**design choice:** Vima is on purpose designed to be recognised by DAWs as an **"instrument"** as this is the most compatible format to be able to get MIDI data out of a VST plugin and "midi plugins" are not recognised as such by any current DAWs. 

**Ableton Live:** on the instrument track, set "MIDI From" to the VIMA track, choose VIMA in the lower chooser, and set monitoring to "In".
Other DAWs have a similar "MIDI input from another track" option.

**Note for Ableton Live users.** Live strips the MIDI channel from MIDI data internally. All 8 tracks of one VIMA arrive mixed together on the receiving track, whatever their MIDI channels. So in Live, use **one VIMA per instrument track**: its 8 tracks can still play several drum voices or layers of that one instrument. (Hosts such as Bitwig and Reaper keep the channels, so there one VIMA can feed several instruments by channel. In Live, that would need a Max for Live device.)

**[Screenshot]** Ableton Live with VIMA on one track and an instrument track receiving "MIDI From: VIMA".

---

## 4. The main window

The main window (the **step view**) shows one track of one pattern at a time. 

The window can be resized by dragging its bottom right corner, from 75% to 200%. The size is remembered with your project.

> **[Screenshot]** The full step view with the three areas labeled. *(main-dark-notes.png)*

The step sequencer consists of two rows of 16 steps, each row with a thin **LED bar** above it which serves as a "playhead" - its the sequencer's "running light".

> **[Screenshot]** Close-up of a few step cells in note mode, with one step off and the last steps beyond the track length.

There are two modes available: **Note Mode** and **CC Mode** which share the same length, time division and direction. 

- In **note mode** the steps show note, octave and one slider (velocity, gate or probability).
- In **CC mode** each step shows one tall slider for its CC value.

Each of the modes can be set to output MIDI messages or not - so a seqeunce (a track) can output only notes, only CC values or both.

### Steps in detail

**[screenshot]** two step cells, one note mode, one cc mode

**note mode:**

- **The step button**: click to switch the step's note on or off. Lit = on, grey = off. **Drag across several step buttons to paint**: start on an off step and every step you pass turns on; start on an on step and every step you pass turns off.
- **The note knob**: Select what note to play. With the scale on, it only lands on notes in the scale.
- **The octave knob**.
- **The slider**: velocity, gate or probability, as chosen with SLIDER, in that value's color. **Click and drag across several sliders to draw** a shape in one move.

**CC mode:** the cell shows the step button (CC on / off for that step) and one tall cyan slider for the CC value.

Steps whose shown part is off are dimmed. Steps beyond the track length are faded out and are not played.

**What a step holds.**

| Value | Range | What it does |
|---|---|---|
| Note on / off | | Whether the step triggers a note. |
| CC on / off | | Whether the step sends its CC value. Independent of the note on / off. |
| Note | C to B | The note name. With the scale on, only notes in the scale. |
| Octave | C-2 to 8 (with middle C = C3) | The whole MIDI range. |
| Velocity | 1-127 | How hard the note is played. |
| Gate | 1-400% | How long the note lasts, in steps: 100% is one step, 50% half a step, 400% four steps (legato and long notes). |
| Probability | 1-100% | The chance that the note plays each time the step comes round. Notes only, never CC. |
| CC value | 0-127 | The value sent when the step's CC is on. |

>**design choice** You might have noticed that velocity and probability ranges do not include a 0 value. A Midi note on message with velocity 0 is actually a Note off and a 0 probability is actually "never" hence the design choices here.

**Mouse tips for knobs and sliders:**

- Drag up / down to change a value. Hold **Shift** for fine control.
- The **mouse wheel** changes one value per notch.
- **Double-click** to type a value. Notes can be typed by name (C, D#, A#).
- **Right-click** (or Alt + click) resets to the default.

### Tracks and patterns

You can create up to 8 completely individual sequences by using Vima's **tracks**. Furthermore you can "progress" sequences or chain them into longer sequences by using **patterns**. You can think of it like this: a track can be set (via its Midi channel output) to control a specific instrument in your DAW while patterns can be used to provide variations of what each instrument plays and build long sequences. 

#### Tracks

VIMA has **8 tracks**, shown as 01 to 08. Each one has its own midi channel assigned and its **note** and **cc** modes. Its related parameters can be controlled at the top of the plugin 

**[screenshot]** the track selection frame

- **TRACK**: pick which of the 8 tracks you see and edit.<br />
- **NOTES / CC**: switch between note mode and CC mode (4.3). Each button has a small light showing whether that output (notes or CC) is on for this track.<br />
- **MIDI CH** (1-16): the channel the track plays on. Any track can use any channel, and several tracks can share one.<br />
**In note mode:** 
- **NOTE OUT** (on / off): whether the track's notes are sent.
- **SLIDER**: which value the step sliders show: VELocity, GATE or PROBability.<br />
**In CC mode:**
- **CC OUT** (on / off): whether the track's CC values are sent. Independent of NOTE OUT.
- **CC NUM** (0-127): which controller the track's CC part sends. One CC per track, on the track's MIDI channel.

>**design choice**A new VIMA starts with track 1 sending notes on channel 1 (so you can quickly check if midi routing in your DAW is set correctly and an instrument is receiving notes), CC mode off and CC numbers 102-109 assigned to all 8 tracks (numbers that are unassigned in the MIDI standard). So loading Vima won't accidentally mess up your carefully designed synth sound.

#### Patterns

**[screenshot]** the pattern selection frame

VIMA holds **64 patterns**, numbered 01 to 64 and shown in four pages of 16. A pattern contains all 8 tracks: their steps and these per-track settings:

- **LENGTH** (4-32 steps): where the track loops. Steps beyond the length are dimmed and not played. Different lengths on different tracks give you polyrhythms for free.
- **DIVISION**: how long one step is: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, and the triplets 1/2T to 1/32T.
- **DIRECTION**: forward, backward or ping-pong. Ping-pong plays the end steps once.
- **SWING** (0-100%): delays every second step. 0% is straight; 100% delays it by half a step.
- **USE SCALE** (on / off): whether this track follows the global scale in this pattern.

>A new VIMA starts with **pattern 1** ready to play (16 steps on, every step an F above middle C at velocity 100) and **patterns 2-64 empty**, so you can see at a glance which patterns you have actually used.

>**design choice:** As you might have grasped by the above, **Note Mode** and **CC mode** of a track in a pattern share the same length, time division and sequencer direction. This is on purpose to make sequenced patterns easy to create and maintain. Want a CC rhythm that runs at a different length or speed than the notes? Use a second track on the same MIDI channel with only its CC output on.

>**design choice:** Controls are visually grouped and color coded according to their scope
>- **White / neutral**: settings for the whole track, pattern or the whole instance (length, division, direction, swing, MIDI channel, scale).
- **Green**: the note part (note steps, notes, octaves, velocity, NOTE OUT, USE SCALE).
- **Cyan**: the CC part (CC steps, CC values, CC NUM, CC OUT).
- **Purple**: gate. **Amber**: probability.

### 4.2 The top band

The top band groups controls by what they belong to, inside rounded frames.

> **[Screenshot]** Close-up of the top band, note mode. *(crop of main-dark-notes.png)*

**The track frame** (left) holds everything about the current track that is the same in every pattern:



**The pattern frame** (middle) holds everything that belongs to this track **in this pattern**:

- **PATTERN**: the pattern you are looking at. The ‹ and › arrows step to the previous / next pattern; clicking the number opens a quick picker with all 64 patterns and a button to open the patterns view (section 7).
- **CHAIN**, next to it: switches chain playback on and off (7.4). It lights up while the chain is on, so you can always see whether the patterns are changing by themselves.
- **LENGTH**, **DIVISION**, **DIRECTION**, **SWING**, and in note mode **USE SCALE**.

**On the right:**

- **The start / stop button and BPM** (the BPM shown is your DAW's tempo):
  - Normally VIMA simply follows the DAW.
  - Press **stop while the DAW plays**: VIMA stops and stays stopped (the icon turns **amber**: "held") until you press its play button again, even if the DAW stops and starts in between. Handy for muting the whole sequencer without touching the DAW.
  - Press **play while the DAW is stopped**: VIMA runs on its own from step 1 at the DAW's tempo, to audition ideas without starting the song.
- **FILE** (folder icon): presets, single patterns and macro sets (section 10).
- **UNDO**: one step back, up to 64 steps.
- **MACROS**: the Macros page (section 9).
- **SETTINGS** (gear icon): the Settings dialog (section 11).
- **SCALE** (on the right, under the buttons): the global scale and root, for example "C MIN". Click it to pick a root and one of 13 scales or your own USER scale. The mouse wheel over it steps through the scales.

> **[Screenshot]** The scale popover open. *(popover-scale.png)*

**Why is SCALE global?** Most electronic music stays in one key and one scale. A global scale means you set it once and every track agrees. Tracks that should ignore it (drums, effects) just switch USE SCALE off. USE SCALE is stored per pattern, so a track can follow the scale in one pattern and ignore it in another.

### 4.3 Note mode and CC mode



The step buttons, the PARAMS buttons, RANDOMIZE, COPY and the other tools always work on the part you are looking at.

> **[Screenshot]** The same track in note mode and in CC mode, side by side. *(main-dark-notes.png, main-dark-cc.png)*



### 4.5 The LED bar: playhead and selection

The LED bar above each row shows the **playhead** (the playing step lights up in green or cyan) and is also where you **select steps**:

- **Click** one LED to select that step. **Drag** to select a run of steps.
- **Ctrl + click** (Cmd + click on Mac) adds or removes a single step, so you can select steps that are not next to each other. **Ctrl + drag** adds a run.
- Selected steps show as **white** LEDs.
- CLEAR (in the operations panel) removes the selection. Switching tracks clears it too.

**The scope rule.** Every step operation in VIMA (copy, randomize, smooth, shift, curves...) works on **the selected steps, or on all steps of the track if nothing is selected**. The SELECTION box in the operations panel always tells you what that is, for example "LENGTH: 7, STEPS: 4".

> **[Screenshot]** A row with a non-adjacent selection (white LEDs) and the SELECTION box.

---

## 5. Step operations

The operations panel at the bottom holds the tools that change many steps at once.

> **[Screenshot]** The operations panel in note mode, labeled: SELECTION, PARAMS, RANGES, RANDOMIZE / SMOOTH / curves, SHIFT ALL.

Two things decide what every tool changes:

- **Which steps**: the selection (or all steps), see 4.5.
- **Which values**: the **PARAMS** buttons.

### 5.1 PARAMS: choosing which values change

In note mode: **ALL, ON, NOTE, OCT, VEL, GATE, PROB**. In CC mode just **ON** and **CC** (with only two, there is no ALL: both lit is everything). ON means the step's note on / off in note mode, its CC on / off in CC mode.

- Click **ALL** to arm everything (note mode).
- While ALL is lit, clicking a single param arms **only that one**.
- Otherwise the buttons toggle one by one.
- Something is always armed: you cannot switch the last one off.
- Note mode and CC mode remember their own choice.

PARAMS are shared by COPY, RANDOMIZE, SMOOTH and the curves.

### 5.2 RANGES: setting the limits

One min / max bar for each value, in its color: NOTE, OCT, VEL, GATE, PROB in note mode, CC in CC mode. Drag the handles to set the limits random values and curves stay within. Ranges of values that are not armed are dimmed.

The **NOTE** range follows the scale: with the scale on, it shows only the notes of the scale. It is a range of note names (it applies to every octave), so "D to G" means only D, E, F, G in any octave (in C minor: D, D#, F, G).

### 5.3 RANDOMIZE

Gives the armed values of the steps in scope new random values, within their ranges.

- **ON**: each step is switched on with a 70% chance.
- **NOTE**: random notes from the NOTE range, in the scale when the track's scale is on.
- Randomize includes steps that are off (they get new values too, ready for when you switch them on).

Each press is one undo step, so you can press, listen, undo, press again.

**Guided randomization in practice:** select steps 9-16, arm only VEL, set the VEL range to 60-110, press RANDOMIZE. Only the velocities of the second half change, and only within a musical range.

### 5.4 SMOOTH

Pulls the armed values one step closer to their average. Press once for a small nudge; **press and hold** to keep smoothing until the values meet. Great for taming a wild randomize result without losing its character.

- Off steps are left out (not counted and not changed).
- Notes are smoothed in scale steps, across octave boundaries.
- Smooth never goes outside the values you already have.

### 5.5 Curves, rotate and invert

Under RANDOMIZE and SMOOTH:

- **Shapes**: **SINE**, **TRIANGLE**, **RAMP** and **SQUARE** draw one cycle of that shape over the steps in scope, between each armed value's range limits. Want a ramp down? RAMP, then INV.
- **‹ ›** (rotate): moves the armed values one step left or right, the end wrapping round to the start. Arm only ON to shift the rhythm under fixed notes, or only NOTE to move the melody over a fixed rhythm.
- **INV** (invert): flips the armed values around the middle of their range. With a CC range of 0-100, 30 becomes 70.

Each press is one undo step.

> **[Screenshot]** A row of CC values before and after SINE.

### 5.6 SHIFT ALL

An endless knob that moves one value of all steps in scope up or down together, keeping the shape: NOTE (in scale steps), OCT, VEL, GATE or PROB in note mode, picked with the buttons next to the knob. In CC mode it always moves the CC value, so the knob stands alone.

- While you turn, the knob shows the running offset ("+3"); it springs back to 0 when you let go.
- A step that hits its limit stays there while the others keep moving, and turning back in the same move brings the original shape back exactly.
- You hear the change live while turning; the whole move is one undo step.

### 5.7 COPY and PASTE (steps)

Copy is how you move steps between tracks and patterns.

1. Select steps (or none for all), arm the PARAMS you want to copy, press **COPY**. It lights up as **COPIED**.
2. Click an LED to pick the **landing step**: a ghost preview shows where the steps will land. Copied steps keep their spacing.
3. Press **PASTE**. COPIED stays lit, so you can switch track or pattern and paste again.
4. Click **COPIED** or **CLEAR** to finish.

Only the armed params are copied. So you can copy just the rhythm (ON), just the notes (NOTE), just the velocities, or everything (ALL). A paste that runs past the end of the track is cut off; it never wraps round.

### 5.8 UNDO

UNDO (in the top band) steps back through your edits, up to 64 steps. It covers everything stored in patterns: step edits, track settings per pattern, copy / paste, randomize, smooth, curves, shift and pattern pastes. One gesture is one undo step (a whole knob move, a whole slider drawing, a whole hold of SMOOTH).

Routing (channel, outputs, CC number), global settings, the chain and macro mappings are not part of undo.

---



---

## 7. The patterns view

The patterns view is where you see all your patterns, copy them around and build a song with the chain. Open it from the PATTERN picker in the top band ("OPEN PATTERNS VIEW"); the green **STEPS** button takes you back.

> **[Screenshot]** The patterns view with several stored patterns, one playing, and a chain. *(patterns-view.png)*

### 7.1 The pattern slots

The four page buttons show patterns 1-16, 17-32, 33-48 and 49-64. A small dot marks the page with the playing pattern.

Each slot shows:

- the pattern number and a **play button**,
- its state: **PLAYING**, **NEXT** (waiting to switch), **STORED** or **EMPTY**,
- a **thumbnail** of the pattern (7.2),
- a thin line at the bottom if the pattern is used in the chain.

At a glance: the **playing** slot is the brightest, **stored** slots are normal, **empty** slots sink back. The **selected** slot has a thick white outline.

- **Click a slot** to select it. This does **not** play it.
- **Click its play button** to select it and switch to it (following the quantize setting, 7.5). A waiting pattern's play button blinks until the switch happens.

### 7.2 Reading the thumbnails

Each thumbnail is a tiny picture of the pattern: one row per track, 01 at the top.

- A **green** line is the track's notes, a **cyan** line its CC. Each lit stripe is a step that is on.
- The line is as long as the track: 32 steps fill the width, 16 half of it.
- A part whose output is **off** is shown dimmed: it holds steps, but you will not hear it.
- A part with no steps on is not shown at all.

> **[Screenshot]** One slot enlarged, with the note and CC lines of a few tracks pointed out.

### 7.3 Copying patterns and tracks

The bar under the slots copies whole patterns or single tracks:

- **COPY** copies the selected pattern; **COPY TRACK** copies one track of it (the one in the TRACK box). The button lights up as **COPIED** / **TRACK COPIED**.
- While it is lit, **clicking a slot picks where to paste** (it gets the white outline).
- **PASTE** writes there. The button stays lit, so you can paste into more patterns.
- A track copy is pasted into the track in the TRACK box, so you can copy track 1 of one pattern into track 3 of another.
- **CLEAR**, or clicking the lit button, finishes. CLEAR never erases a pattern.

**Right-click a slot** for a quicker way: **COPY PATTERN**, **COPY TRACK**, **PASTE**, plus **LOAD PATTERN...** and **SAVE PATTERN AS...** (section 10). A PASTE from this menu is a one-off: it pastes into the slot you right-clicked, then clears the clipboard, and the PASTE button flashes green to confirm.

> **[Screenshot]** The slot right-click menu, and the patterns view with COPIED lit and a paste destination outlined. *(patterns-view-copied.png)*

### 7.4 The chain

The chain on the right is a song list: patterns played one after the other.

- **+ ADD** adds the selected pattern at the end. The same pattern can appear many times.
- Each entry shows the pattern, its length in bars and a **repeat** count (- / +, up to 99). The playing entry has a lit LED and a progress bar.
- **Drag** an entry by its grip to reorder; **x** removes it; **CLEAR** empties the chain.
- **CHAIN ON / OFF** starts and stops chain playback (the same as the CHAIN button next to the PATTERN box in the main window). **LOOP ON** goes back to the first entry at the end; **LOOP OFF** plays the chain once and then keeps repeating the last pattern.
- Long chains scroll with the mouse wheel or the scroll bar.

When the DAW starts, the chain always starts from its first entry. (It does not follow the song position like the steps do. This was a deliberate choice: it keeps editing patterns from the DAW simple.) Picking a pattern by hand while the chain plays works as usual; the chain then carries on with its next entry.

> **[Screenshot]** A long chain with the scroll bar. *(patterns-view-long-chain.png)*

### 7.5 When does a pattern switch?

The **Quantize** setting (Settings dialog) decides:

- **OFF**: instantly, at the moment you pick it. Every track carries on at the right step, so the groove never stumbles.
- **1 BEAT** / **1 BAR**: at the next beat or bar of your DAW's time signature.
- **PATTERN END**: when the playing pattern reaches its end.

While a switch is waiting, the PATTERN box blinks. Picking another pattern before the switch replaces the waiting one. When the DAW is stopped, patterns switch at once.

Patterns can also be switched from outside: by a **CC message** from a controller or another track (Settings, 11), or by the **Pattern macro** from DAW automation (9).

---

## 8. Playback details worth knowing

- **Steps follow the song position.** Step 1 of every track falls on bar 1, beat 1 of your song and on every repeat of the track's length after it. Start the DAW anywhere and every track is on the right step.
- **Swing** counts from the song position too, so swung tracks stay locked together.
- **Long gates** (over 100%) let notes overlap into the next steps. If the same note plays again on the same channel while the old one is still held, the old one ends just before the new one starts, so nothing gets stuck.
- **Stopping**, jumping in the timeline and instant pattern switches end every sounding note.
- **CC smoothing** (Settings) glides each track's CC smoothly from one CC step to the next instead of jumping.

---

## 9. Macros: automating VIMA from your DAW

### 9.1 Why macros?

A DAW can only automate parameters that a plugin "publishes", and it needs a fixed list of them. VIMA has 8 tracks x 64 patterns x 32 steps x 8 values per step: well over 100,000 things you might want to move. No DAW can show a list like that, and nobody could find anything in it.

So VIMA publishes **128 macros** instead. Each macro is an empty slot you point at exactly the thing you want to automate. Your DAW sees only those 128, and each one is **named after what it controls**, for example "Vel St05 Tr03 Pat23" (velocity of step 5, track 3, pattern 23) or "Swing Tr03 Pat23". Unused macros are simply "Macro 1", "Macro 2" and so on.

Every macro points at **one fixed thing**: one track, one pattern, one step. It does not follow what is on screen. That is what makes automation reliable: what you record is exactly what plays back.

Macros work both ways: turn a VIMA knob that is mapped, and the macro moves too, so you can record your moves in the DAW.

What can be mapped:

- **Global:** Pattern (which pattern plays), Chain On.
- **Track:** Notes Out, CC Out; and per pattern: Length, Division, Direction, Swing, Scale On.
- **Step:** Note On (note on / off), Note, Octave, Velocity, Gate, Probability, CC On (CC on / off), CC Value.

The **Pattern** macro is special: turning it picks the next pattern, and the macro shows that pattern straight away, while the actual switch waits for the quantize point in the background.

A **Note** macro follows the scale like the note knob: in C minor it moves through the 7 notes of the scale.

### 9.2 The Macros page

Open it with **MACROS** in the top band; **STEPS** goes back.

> **[Screenshot]** The Macros page with some mapped macros and the editor on the right. *(macros.png)*

- **Left:** 4 pages of 32 macros (1-32 ... 97-128), the total count ("40 OF 128 MAPPED") and **LEARN**. Each macro cell shows its number, a knob with the current value, what it controls ("STEP 05 VEL") and where ("T04 · 06" = track 4, pattern 06).
- **Right:** the editor for the selected macro. Choose **TARGET** (GLOBAL / TRACK / STEP), **PARAMETER**, and the **TRACK**, **PATTERN** and **STEP** it points at. **CLEAR MAPPING** empties it.

### 9.3 LEARN: mapping by clicking

The quickest way to map:

1. Press **LEARN**. The selected macro (or the next free one) starts listening; the header button reads "LEARN M005" in amber.
2. Go to the step view or the patterns view and **click the control** you want to automate: a step's velocity, a track's swing, a pattern slot...
3. The macro is mapped to that control (on the track and pattern you are looking at), and LEARN moves on to the next free macro. Keep clicking to map more.
4. Press LEARN again to stop.

While learning, every mapped control shows an amber outline with its macro number. A control can only have one macro: clicking a mapped one does nothing. **Right-click** a mapped control to free it.

> **[Screenshot]** The step view while learning, with amber outlines and macro numbers. *(main-learning.png)*

Then, in your DAW, automate or map hardware knobs to the macros as you would with any plugin parameter.

---

## 10. Saving: presets, patterns and macro sets

### 10.1 Your DAW project saves everything

Every VIMA in your project saves its **complete** state with the project: all patterns, the chain, routing, scale, settings, macro mappings and window size. Each VIMA instance keeps its own: two VIMAs in the same project can have completely different settings. You never need to save a preset just to keep your work.

The files below are for **reusing** material across projects.

### 10.2 The FILE menu

Click **FILE** (the folder icon) for one menu with three sections.

> **[Screenshot]** The FILE menu open, showing the PRESET, PATTERN and MACROS sections.

**PRESET**: the music of the whole instance.

- Holds all 64 patterns, the chain, the track routing, the scale, root and user scale, the PARAMS and RANGES, and the current pattern.
- Does **not** hold the macro mappings, the settings found only in the Settings dialog (CC smoothing, pattern change by CC, quantize), the theme, middle C or the window size. Loading a preset leaves all of those as they are.
- **NEW** starts over (pattern 1 ready, the rest empty) and asks first. **LOAD...**, **SAVE** (overwrites the current preset; greyed out if it has never been saved) and **SAVE AS...**
- The menu header shows the current preset's name, or "UNSAVED".
- Loading a preset or NEW clears the undo history.
- Files end in **.vima** and are kept in Documents/VIMA/Presets by default.

**PATTERN**: one single pattern.

- Works on the **selected pattern** (the one in the PATTERN box, or the selected slot in the patterns view).
- Holds the pattern's 8 tracks: their steps and their length, division, direction, swing and use scale. No routing, no scale: a loaded pattern plays in the current scale.
- **NEW** resets the pattern (16 steps on, ready to edit) and asks first. **LOAD...** asks before replacing a pattern that has steps. **SAVE AS...** saves it. Both NEW and LOAD can be undone.
- Files end in **.vimapattern**, in Documents/VIMA/Patterns.
- Also available by right-clicking a slot in the patterns view.

**MACROS**: all 128 macro mappings, as a set.

- Handy for a mapping you use with a hardware controller, or a standard set of automation targets.
- **NEW** clears all mappings and asks first. **LOAD...** asks before replacing existing mappings. **SAVE** and **SAVE AS...** work like presets.
- Loading a macro set never touches your music, and loading a preset never touches your mappings.
- Files end in **.vimamacros**, in Documents/VIMA/Macros.

Your DAW's own preset system (for example Live's .adv or .vstpreset files) also works and stores everything, mappings included.

---

## 11. The Settings dialog

Open it with **SETTINGS** (the gear icon). Click **DONE** or anywhere outside it to close.

> **[Screenshot]** The Settings dialog, dark and light. *(settings.png, settings-light.png)*

- **APPEARANCE: Theme**, DARK or LIGHT. This is **your** preference: it applies to every VIMA in every project, and switching it in one VIMA switches all open ones.
- **PITCH: Middle C is** C3, C4 or C5. This only changes how octaves are **numbered** on screen (MIDI note 60 is always middle C), so VIMA can match your DAW: C3 for Live, Bitwig, Logic, Cubase, Studio One, Pro Tools and Reason; C4 for Reaper and Tracktion; C5 for FL Studio, Cakewalk and Renoise. Like the theme, it is your preference for every VIMA.
- **USER SCALE**: build your own scale. Choose the **root** and switch the 12 notes on or off (the root is always on). Then pick **USER** in the SCALE popover. The root here is the same as the one in the SCALE popover.
- **CC OUTPUT: Smooth between steps**: when on, each track's CC glides from one CC step's value to the next instead of jumping.
- **PATTERN CHANGE**:
  - **From incoming CC**: switch patterns with a CC message, on the MIDI channel and CC number you set. Value 0 picks pattern 01, value 63 picks pattern 64. Great for switching patterns from a hardware controller or from another track in your DAW.
  - **Quantize changes to**: OFF, 1 BEAT, 1 BAR or PATTERN END (see 7.5).
- **RESET DEFAULTS** puts everything in this dialog back to its starting value. The theme and middle C are reset for every VIMA; the scale you selected is kept.

Except for the theme and middle C, these settings belong to each VIMA in your project and are saved with the project, but not in presets: a preset is about the music, these are about how the instance is set up.

---

## Appendix A: Starting values

| Setting | Starting value |
|---|---|
| Tracks | track 1 notes on, tracks 2-8 silent; MIDI channels 1-8; CC output off; CC numbers 102-109 |
| Patterns | pattern 1: 16 steps on; patterns 2-64 empty |
| Steps | F above middle C, velocity 100, gate 100%, probability 100%, CC value 64 |
| Track settings | length 16, 1/16, forward, swing 0%, use scale on |
| Scale | C minor |
| PARAMS | ON and NOTE in note mode, CC in CC mode |
| RANGES | full range of every value |
| Chain | empty, off, loop on |
| Quantize | 1 BAR |
| CC smoothing | off |
| Pattern change by CC | off (channel 16, CC 20) |
| Middle C | C3 |
| Theme | dark |

## Appendix B: Mouse quick reference

| Where | What you do | What happens |
|---|---|---|
| Knobs | drag up / down | change the value (Shift = fine) |
| Knobs and sliders | mouse wheel | one value per notch |
| Knobs and sliders | double-click | type a value |
| Knobs and sliders | right-click or Alt + click | reset to default |
| Step sliders | click and drag across steps | draw values |
| Step buttons | click / drag across | switch on or off / paint |
| LED bar | click / drag | select one step / a run |
| LED bar | Ctrl + click / Ctrl + drag | add or remove steps / add a run |
| SMOOTH | press and hold | keep smoothing |
| SHIFT ALL | drag / wheel | shift values (springs back to 0) |
| DIVISION, SCALE | mouse wheel | next / previous value |
| Pattern slot | click / play button | select / select and switch |
| Pattern slot | right-click | copy, paste, load, save menu |
| Chain entry | drag the grip | reorder |
| While LEARN is on | click / right-click a control | map it / free it |

On a Mac, Alt is Option, and Ctrl + click for selecting steps is Cmd + click.
