# Architecture

How the firmware is split, in what order one loop runs, and which habits are
load-bearing rather than stylistic.

## The layers

```
lib/cuisine      hardware: pin, ADC, SPI, timers. No knowledge of patterns
track            state and per-track timing: patterns, presets, clock, arpeggiator
eventManager     what comes out: operators, note resolution, dispatch to outputs
interface        workflow: pages, edits, transport, LED framing
main             wiring only
```

| file | lines | owns |
|---|---|---|
| `src/interface.cpp` | 2,825 | the panel: pages, key handling, transport, LED drawing |
| `src/eventManager.cpp` | 985 | resolving and dispatching what a track plays |
| `src/track.cpp` | 895 | pattern data, presets, per-track timing, save paths |
| `src/trackClock.cpp` | 74 | the per-track clock: source, division, offset, play mode |
| `src/structDefinition.cpp` | 62 | every FRAM address |
| `src/main.cpp` | 16 | `cuisine::begin()` and the loop |

`interface` owns the tracks; a track never calls back into the interface. That
one-directional rule is what keeps `track.cpp` testable by reading alone.

## One loop, in order

From [`interface.cpp:498-531`](../../src/interface.cpp):

| # | step | why it is here |
|---|---|---|
| 1 | tap tempo on the SHIFT edge | the input layer must not depend on the feature |
| 2 | `loadUpdate` / `saveUpdate` | deferred flash work, done outside the hot path |
| 3 | `_updateInput()` | **triggers are read here** |
| 4 | clock master update | one master tick per pass |
| 5 | MIDI in | drained before transport reacts to it |
| 6 | `_updateTransport()` | play/stop/reset edges |
| 7 | `keyboardClass::update()` | **old values latch only now** |
| 8 | clear command, pattern page | housekeeping |
| 9 | LED frame flush, then mask cleared | the pass starts from a blank frame |
| 10 | gate inputs | |
| 11 | latch play triggers, then each track updates | the musical work, once per pass |

## Why that order is load-bearing

**Triggers before the latch.** A key press sets a bit in `pendingRise`; the
latch at step 7 moves it into `rise` and clears it. If the latch moved earlier,
an edge would be consumed before anything read it, or read twice. The comment at
the top of `update()` says exactly this.

**The LED frame is draw-then-flush-then-clear.** Every stage sets LEDs as it
works; step 9 sends the frame once and clears the mask. That is why drawing is
sprinkled through the interface instead of collected in one function.

**Flash work is deferred.** A 32 KB block erase takes tens of milliseconds, so
it runs in `loadUpdate`/`saveUpdate` where the panel can be idle, never in the
middle of a trigger pass.

## Why this application does not call `cuisine::poll()`

The SDK's `poll()` does latch-first, then MIDI, then the clock. This
application needs triggers **before** the latch, and it draws LEDs between
stages. So it calls the SDK functions individually, in its own order.

That is not a workaround for a missing API — it is the reason `poll()` exists as
a convenience for new applications rather than a requirement. The cost is that
this app must keep the order correct by hand; see the invariants below.

## Invariants

Each of these has cost a bug, and none is enforced by the compiler.

1. **A reset must be taken inside `update()`, after the flag swap.** The
   `_oldFlag = _flag; _flag = false;` dance at the top of `trackClock::update()`
   is what turns a latched edge into a one-shot; a reset taken elsewhere is
   swallowed by the next swap.
2. **Coupled and triggered tracks advance in `track::update()`, not in
   `trackClock`.** `trackClock::update()` deliberately has empty branches for
   those; moving the advance there would double-step them.
3. **`interface` never computes a FRAM address.** It talks to track methods, and
   every address comes from `structDefinition.cpp`. It never calls
   `storage::fram` or `storage::flash` itself.
4. **`eventManager` reads tracks and writes outputs — and records.** During
   recording it writes the played note or value back into the track
   (`eventManager.cpp:508-537`). That is the only place it edits pattern data,
   and it is the record path, not a general licence.

## Where to add something

| change | file |
|---|---|
| a page, a key combination, an LED behaviour | `src/interface.cpp` |
| per-track data, timing, saving | `src/track.cpp`, `include/structDefinition.h` |
| what a track does with its clock | `src/trackClock.cpp` |
| a rule for what an output does | `src/eventManager.cpp` |
| a hardware block or a bus detail | `lib/cuisine/` |

If a change wants to cross one of those boundaries, that is the design telling
you something — the boundary that most often looks inconvenient is the one
keeping `interface` out of FRAM, and it is worth keeping.
