# Panel and pages

How the panel is wired into the firmware: which key reaches which page, what the
LEDs mean, and where the pot is pointed.

**Provenance.** Every row is traced to the source and cites it. Nothing here has
been pressed on the device yet — where the code is ambiguous, the row says so
rather than picking a reading.

## The surface

| | |
|---|---|
| 16 step keys | also the keyboard for NOTE tracks |
| 14 command keys | SHIFT, LOOP, DIV_MULT, CLEAR, PASTE, COPY, TRACK, STEP_EDIT, NOTE_SELECT, EUCLID, PATTERN, PAGE, REC, PLAY |
| arm switch | "override": live play and temporary changes without leaving the current page |
| endless pot | edits the current page's value |
| 16 LEDs | steps, keys, values and page indicators share them, depending on the page |
| 7 transport LEDs | PLAY, REC, OVR, PAGE 1..4 |

The command keys are bit masks, and a **combination is how a second function is
reached**: `TRACK_PLAY = SHIFT | TRACK`, `SCALE = SHIFT | NOTE_SELECT`,
`STEP_MUTE = SHIFT | STEP_EDIT`, `BANK = SHIFT | PATTERN`, `RESET = SHIFT | PLAY`,
`JUMP = SHIFT | LOOP`, `CLOCK_SHIFT = SHIFT | DIV_MULT`
(`include/interface.h:14-56`).

## Three kinds of press

The handler distinguishes them before anything else happens
(`src/interface.cpp:2212-2300`):

| gesture | where | what it does |
|---|---|---|
| **press** | `keys::commandPressed` | the key's primary action |
| **SHIFT + key** | held modifier | the secondary function listed above |
| **double tap** | same key twice within 250 ms | a shortcut, usually jumping to a page |

Double-tap shortcuts, from `interface.cpp:2232-2241`:

| double tap | goes to |
|---|---|
| SHIFT | BPM page |
| TRACK | TRACK SETTINGS |
| STEP_EDIT | TRACK SETTINGS 2 |
| PATTERN | MASTER SETTINGS |
| COPY / PASTE | copy or paste the current pattern |
| PAGE + SHIFT | copy the pattern to another slot |

A **held gesture** exists, but only one: SHIFT+CLEAR held for 500 ms
(`SAVE_TIME`) arms a quick clear, drawing the progress on the LEDs, and the
clear fires when CLEAR is released (`interface.cpp:2067-2084`). Saving is
instant, not held: SHIFT+COPY. The shorter 125 ms threshold (`POP_UP_TIME`)
drives the pot and step pop-ups.

## Direct actions

`interface.cpp:2155-2188`:

| key | action |
|---|---|
| PLAY | start or stop the transport, and send MIDI start/stop with it |
| REC | toggle record mode; releasing it clears the record mutes |
| PAGE | next page of the current view |
| PAGE + SHIFT | change the pattern's page count (1..4) |
| arm switch | enter/leave override: arp and live buffers are reset on release, and the armed mute state is restored |
| SHIFT + COPY | quick save the current pattern |
| SHIFT + PASTE | quick load the saved pattern |
| SHIFT + CLEAR | quick clear, but only after a 500 ms hold; fires on release |

These come from `interface.cpp:2049-2084` and `2155-2188`. COPY, PASTE and CLEAR
on their own are context-dependent: they act on the page you are on.

## Pages

A page is a command-key value, optionally OR-ed with one of two flags that tell
the pot whether it may be borrowed (`interface.h:26-38`):

| page | value | edits |
|---|---|---|
| STEP_EDIT | `128` | which steps are active |
| STEP_MUTE | `SHIFT|STEP_EDIT` | which steps are muted |
| NOTE_SELECT / KEYBOARD_PAGE | `256` | the note of a step, from the keyboard |
| SCALE | `SHIFT|NOTE_SELECT` | scale selection |
| EUCLID | `512` | Euclidean rhythm parameters |
| ACCENT | `EUCLID+1` | accent per step |
| PATTERN | `1024` | pattern selection |
| BANK | `SHIFT|PATTERN` | bank selection |
| PLAY / RESET | `8192` / `SHIFT|PLAY` | transport, and master reset |
| MODE_SET | `16|DONT_BYPASS` | play mode of the track |
| GATE_LENGTH | `32|BYPASS` | gate length |
| CHORD | `64|DONT_BYPASS` | chord |
| BPM | `128|BYPASS` | tempo |
| MASTER_RESET | `256|BYPASS` | reset source |
| TRACK_SETTINGS | `512|BYPASS` | track output, channel, type |
| TRACK_SETTINGS_2 | `1024|BYPASS` | second page of the above |
| MASTER_SETTINGS | `2048|BYPASS` | master clock settings |
| PATTERN_PAGE | `2048` | which page of the pattern is being edited |

**Open question, not resolved here:** `NOTE_SELECT`, `KEYBOARD_PAGE` and
`SCALE-1` are all `256`, and `PATTERN_PAGE`, `PAGE` and `MASTER_SETTINGS` all
resolve to `2048`. Some of that is deliberate reuse of a bit position; some may
be leftovers. Resolving it needs the device, not more reading.

## The pot

The pot edits the value of the current page. The two page flags exist because
some pages want the pot permanently (`DONT_BYPASS_OVERRIDE_POT`) while others
hand it to the override mechanism when the arm switch is down
(`BYPASS_OVERRIDE_POT`).

The practical consequence, and the thing that surprises people: **turning the
pot can stop affecting the page you are looking at**, because the override or a
pop-up has taken it. If a pot edit does nothing, check the arm switch and the
page flags before suspecting the encoder.

## LED language

| signal | meaning |
|---|---|
| colour | state — red, green, or yellow for "both" |
| dim | secondary state: the same information at lower priority |
| blink | needs attention; `blink()`, `blinkLow()` and `shortBlink()` are phases shared by everything that blinks |
| transport LEDs | PLAY (transport running), REC (recording), OVR (override armed, blinking when the mute is armed), PAGE 1..4 |
| page LEDs | position in the current page, with the current pattern shown separately (`interface.cpp:2204-2209`) |

## Recording

REC toggles record mode and remembers the step it started from; releasing REC
clears the record mutes (`interface.cpp:2166-2172`). The arm switch is separate:
it is the live-override gesture, not the record gesture.

What a recorded note does depends on the track type — a NOTE track stores pitch,
a DRUM track reduces it to a trigger with a fixed note. That branch is in the
track and event layers, not here; see
[README.md](README.md#the-three-track-types).
