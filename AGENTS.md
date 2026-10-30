# Working in this repository

Two layers, kept separate on purpose: `lib/cuisine/` is the hardware SDK, and
`src/` + `include/` is the sequencer built on it. Documentation is in `docs/` —
read the page that covers the area before changing it.

## Commands

```
pio run -e ATmega4809              production firmware
pio run -e bench                   same application + serial test protocol
pio run -e <env> -t upload --upload-port COMx
pio device list                    the port changes when the board is replugged
```

On Windows `pio` lives in `%USERPROFILE%\.platformio\penv\Scripts`. The
production image prints nothing on serial by design — silence is not a failed
flash.

## Verify, do not assume

A change is done when it **builds and was checked on the board**. "It compiles"
is not verified. The bench build exists for this:

```
pio run -e bench -t upload --upload-port COMx
s        status: transport, tick count, BPM, current step, gate mask
p        start the transport
t48      inject 48 deterministic ticks (one per loop, timer stopped)
s        the tick count, step and gate mask must have moved
o        stop
R        FRAM write/read self-test: R PASS DE AD BE EF
```

State in the commit what you flashed and what you observed.

## Hard rules

- `lib/cuisine/` never includes a header from `src/` or `include/`.
- No heap, no virtual functions, no lambdas with captures, no `std::function`.
- Do not reformat code you did not change. Mixed 2/4-space indentation is
  deliberate; a whitespace diff hides the actual change.
- Saved-data layout is user data. The `static_assert`s in
  `include/structDefinition.h` pin it; changing one needs an answer for data
  already in FRAM, not just an updated assert.
- No external library dependencies. Timers, drivers and MIDI parsing are in the
  library; adding `lib_deps` needs a licence check.
- The flash ceiling is **48,640 B**, not the 49,152 PlatformIO prints. The bench
  build sits within a few hundred bytes of it — check sizes after any change.

## Where the truth lives

| question | file |
|---|---|
| build, flash, bench in practice | `docs/firmware.md` |
| every bench command | `docs/sequencer/bench.md` |
| pins, SPI bus, measured speeds | `docs/hardware.md` |
| code rules and style | `docs/conventions.md` |
| SDK API, module by module | `lib/cuisine/docs/api.md` |
| loop order and invariants | `docs/sequencer/architecture.md` |
| FRAM map, save paths, migration | `docs/sequencer/data.md` |
| pages, keys, gestures, LEDs | `docs/sequencer/ui.md` |

## Branches

`master` is the working history. `release` is a single squashed commit used for
publishing, tagged `sdk-v1.0.0`, rebuilt from `master` whenever the tree changes
(delete the branch, `git checkout --orphan release`, commit, move the tag).
Never commit on `release`, and never `git push --tags` — that would drag the
pre-clean history along.

## Do not

- rename `VARIATION_NUMBER`, `variationStruct` or other legacy identifiers for
  consistency; the documentation carries the word mapping instead
- delete a limitation from the docs instead of fixing it
- change the layout of a saved struct without saying what happens to existing
  banks
