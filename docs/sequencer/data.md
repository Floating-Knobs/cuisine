# Data and storage

Where the sequencer keeps what it knows, and what happens when you save.

Everything below is computed from
[`include/structDefinition.h`](../../include/structDefinition.h) and
[`src/structDefinition.cpp`](../../src/structDefinition.cpp). If this page and
those files disagree, the files are right.

## Three places data lives

| where | what | written |
|---|---|---|
| FRAM | the live bank, per-track quick-save buffers, operators, transpose | field by field, as you edit |
| SPI flash | 16 archived banks | explicitly, one whole bank at a time |
| RAM | a 153 B pattern copy, edit state, live input | lost on power-off |

There is no EEPROM. FRAM is the working memory: it survives power loss, needs no
erase, and a write costs the same as a read.

## The FRAM map

32,768 B, and 6.6 KB of it is free.

| region | start | size | holds |
|---|---|---|---|
| per-track pattern buffer | `0x0000` | 1,836 B | 12 × 153 B — each track's quick-save slot |
| track copy area | `0x072C` | 1,850 B | one whole `trackStruct` |
| live bank | `0x0E66` | 22,268 B | 12 tracks (12 patterns + preset each) and the bank preset |
| operator settings | `0x6562` | 96 B | 12 × 8 B: gate and CV operator per track |
| transpose | `0x65C2` | 25 B | one metadata byte, then 12 × 2 B |
| free | `0x65DB` | 6,693 B | unused |

The address functions compose these from `sizeof` and `offsetof`, so the map
moves if a struct moves — which is exactly what the `static_assert`s prevent.

`PATTERN_BUFFER_SIZE` is what separates the first area from the bank:
`153 × 12 + 1,850 = 3,686`.

## The flash map

Sixteen banks. Bank `b` starts at `b << 15`, i.e. every bank owns a **32 KB
block**, so the 16 banks occupy 512 KB of the 1 MB chip.

A bank is **a copy of the whole FRAM image**, not a compact structure. That is
blunt and wastes space — half the flash is unused, and the free FRAM area is
archived too — but it makes save and load a straight memcpy with no format
decisions.

## The structs

| struct | size | notes |
|---|---|---|
| `patternStruct` | 153 B | 64 steps: `step[4]` masks, `accent[4]`, `duration[64]`, `value[64]`, plus first/last, division, mute, gate length |
| `presetStruct` | 14 B | type, outputs, clock and reset sources, play mode, CC |
| `trackStruct` | 1,850 B | 12 patterns + preset |
| `bankPresetStruct` | 68 B | tempo, scale, chord, mute groups, progression |
| `bankStruct` | 22,268 B | 12 tracks + bank preset |

Each size and the important offsets are pinned:

```cpp
static_assert(sizeof(patternStruct) == 153, "saved-data layout changed: patternStruct");
LAYOUT_OFFSET(patternStruct, step, 0);
```

The assertions exist because the data is the user's: patterns outlive firmware
versions.

## Save paths

**Quick save — SHIFT+COPY.** The current pattern is copied into that track's
slot in the first FRAM area (`savePattern(number, quicksave = true)`). **Quick
load — SHIFT+PASTE** brings it back. Neither touches flash.

**Copy / paste — double-tap COPY, then a step key.** `copyPattern()` reads 153
bytes into a RAM copy; `pastePattern()` writes it to the destination. RAM, so it
is gone after a power cycle.

**Bank save.** `_transferBank(bank, true)`: erase the 32 KB block, then copy all
32 KB of FRAM into flash in 256 B chunks — one flash page per chunk, which is
why that number is not arbitrary. The erase blocks the bus for tens of
milliseconds, so the app does this while the panel is idle, never inside
`poll()`.

**Bank load.** The same loop in reverse, then the tracks are re-initialised from
the new FRAM contents.

## Migration rules

There is **no version byte anywhere.** A bank saved by an older firmware is read
with the current struct layout, whatever that is.

Consequences:

- adding a field to a saved struct means updating the assertions, and the build
  refuses to produce a firmware whose layout disagrees with them;
- moving a field means old banks read garbage in that field — the assertions
  cannot catch it, only the person reading
  [conventions.md](../conventions.md#changing-saved-data);
- the accepted answers are a new address (leave the old data alone), a version
  byte (add one, knowingly), or a documented break.

## What is not stored

The page you were on, override state, live input buffers, arpeggiator state, the
clock phase. Power-cycling loses all of it by design — "it forgot where I was"
is not a save bug.
