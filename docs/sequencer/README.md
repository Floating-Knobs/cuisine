# The sequencer

Twelve tracks of step sequencing on one board, edited entirely from the panel.
Each track is an independent voice with its own clock, scale and output routing;
nothing needs a computer.

The instrument is made by Floating Knobs. See the
[product page](https://www.floatingknobs.com/product-page/cuisinette) and the
[video manual](https://www.youtube.com/watch?v=GlvFoVIrSeU) for the physical
controls and the intended workflow; this document is about how the firmware
implements it.

## The three track types

A track is not simply "a voice". Its **type** decides what a step means, how it
is edited, and what comes out. The type is part of the track preset, so it
survives a pattern change.

| type | a step stores | plays as |
|---|---|---|
| **NOTE** | a packed note: scale index + octave, plus accent and duration | CV pitch and gate, and a MIDI note. Supports the arpeggiator |
| **MOD** | one 8-bit value | CV as continuous modulation, re-read every cycle; the gate follows the step trigger |
| **DRUM** | a trigger (the value comes from the last gate) | a short fixed gate (`_gateDuration = 2`) and one fixed MIDI note |

Underneath, all three use the same storage: **one byte per step** — the packed
note for NOTE tracks, the modulation value otherwise — plus a duration and an
accent bit. That is why the step editor and the LEDs change meaning with the
type while the pattern structure does not.

## The data model

| level | count | lives in |
|---|---|---|
| bank | 16 | flash when archived, FRAM when live |
| track | 12 per bank | FRAM |
| pattern | 12 per track | FRAM |
| step | 16 per pattern × 4 pages = 64 | FRAM |

Each level carries its own settings:

| level | owns |
|---|---|
| bank | tempo, scale, chord, mute groups, chord progression |
| track | type, CV output, gate output, MIDI channel, clock source and input, play mode, reset source, transpose |
| pattern | length (`first`/`last`), division, mute, gate length, modified flag |

Every size and offset above is pinned by a `static_assert` in
[`include/structDefinition.h`](../../include/structDefinition.h) — see
[conventions.md](../conventions.md#changing-saved-data) before touching any of
them.

## A track

What one track owns, in the order that matters when something is wrong:

1. **type** — NOTE, MOD or DRUM (above);
2. **routing** — which CV channel, which gate output, which MIDI channel, which
   CC it listens to;
3. **clocking** — source (master, coupled, triggered, looper), offset, play mode
   (normal, gate, one-shot) and reset source. The division is **not** here: it
   belongs to the pattern, which is what lets one track switch feel between
   patterns;
4. **musical settings** — scale, arpeggiator, Euclidean rhythm, transpose;
5. **data** — twelve patterns, each 64 steps across four pages.

The panel and this documentation call them **pages**; the code still calls the
same thing a variation internally (`VARIATION_NUMBER`, `variationStruct`,
`index.page`). Same object, two names — the UI's word wins in prose.

## The signal flow

```
keys / MIDI / gate inputs
    |
    v
interface        what the user meant: pages, edits, transport, overrides
    |
    v
track            when it happens: per-track clock, pattern data, arpeggiator
    |
    v
eventManager     what comes out: gate/CV operators, note resolution, dispatch
    |
    v
CV outputs (4) · gate outputs (8) · MIDI out
```

The three stages fail in distinguishable ways, which is why the split is worth
knowing: no edit at all means `interface`; edits that never fire mean `track`
and its clock; notes that fire with the wrong pitch or on the wrong output mean
`eventManager` or the routing.

Twelve tracks share four CV and eight gate outputs, so routing is a real
decision, not a formality: two tracks on the same output will contend, and the
operator settings define how gate sources are combined (AND, OR, XOR, NAND) and
how CV sources are mixed (ADD, SUB, MULT, MIX).

## Panel and pages

Sixteen step keys, fourteen command keys, an arm switch, one endless pot, 16
LEDs and 7 transport LEDs. Every key combination, page and LED meaning is in
[ui.md](ui.md).

## Saving

Live edits go to FRAM as they happen — that is what the `modifiedFlag` tracks.
Archiving a bank to flash is explicit, because an erase blocks the bus for
milliseconds. Addresses, formats and the migration rules are in
[data.md](data.md).

## Where the truth lives

| question | file |
|---|---|
| what a track stores, and where | `include/structDefinition.h`, `src/track.cpp` |
| when a track fires | `src/trackClock.cpp`, `src/track.cpp` |
| what comes out of it | `src/eventManager.cpp` |
| what the keys and pages do | `src/interface.cpp` |
| how the blocks are implemented | `lib/cuisine/` |
