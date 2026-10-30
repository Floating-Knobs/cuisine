# cuisine

Hardware SDK for the Cuisine board: ATmega4809 at 20 MHz, 48 KB flash, 6 KB RAM.
A panel of 16 red/green LEDs and 30 keys, four CV outputs, eight gate outputs, four
gate inputs, one endless potentiometer, MIDI in and out, 32 KB of FRAM and 1 MB
of SPI flash.

It is a hardware layer. It knows the panel, the outputs, the clock and the
memory chips. It has no idea what a pattern, a track or a song is — those are
application decisions, and the application in this repository is an opinionated
example of making them.

## Two layers in this repository

| layer | where | what it is |
|---|---|---|
| **SDK** | `lib/cuisine/` | what this document covers |
| **sequencer** | `src/` | a complete sequencer built on it: 12 tracks, patterns, banks, scales, arpeggiator, Euclidean rhythms |

The sequencer is the reference implementation. It uses every block, and at
46.9 KB it is also the proof of what fits in the flash budget. When this
document runs out, read `src/`.

## What you get

| block | hardware | start with |
|---|---|---|
| panel keys | 16 step + 14 command + arm switch | `keys::pressed(i)` |
| endless pot | two wipers, read as a vector | `pot::read(value, 1024, min, max)` |
| CV out | 4 × 12-bit DAC | `cv::setNote(ch, midiNote)` |
| gate out | 8, one shift register | `gate_out::set(i, true)` |
| gate in | 4, interrupt-driven | `gate_in::rising(i)` |
| LEDs | 16 red/green + 7 transport | `led::set(i, led::Red)` |
| display | 4 digits drawn on the LED bar | `display::number(127)` |
| MIDI | UART1 at 31250 | `midi::onNoteOn(fn)` |
| clock | master timebase | `clock::start()` |
| storage | 32 KB FRAM + 1 MB flash | `cuisine::storage::fram.put(addr, v)` |

The full surface is in [docs/api.md](docs/api.md).

## Install

```ini
[env:myapp]
platform = atmelmegaavr
board = ATmega4809
framework = arduino
board_build.f_cpu = 20000000L
build_flags = -flto -ffunction-sections -fdata-sections -Wl,--gc-sections
lib_extra_dirs = ../path/to/sequencer/lib
```

**No dependencies.** The SDK brings its own drivers, its own timers and its own
MIDI parser; nothing is fetched at build time. A registry release would replace
the last line with `lib_deps = cuisine`.

## The smallest working app

```cpp
#include <cuisine.h>
using namespace cuisine;

static bool s_gate = false;

void setup() {
    begin();
    clock::setTempoTenths(1200);    // 120.0 BPM
    clock::start();
}

void loop() {
    poll();

    if (keys::commandPressed(board::kKeyPlay)) {   // PLAY toggles gate out 0
        s_gate = !s_gate;
        gate_out::set(0, s_gate);
        led::set(0, s_gate ? led::Green : led::Off);
        led::flush(8);                             // LEDs are flushed by the app
    }

    if (clock::tick()) {
        // one clock tick: advance your step here
    }
}
```

That compiles as printed with the `platformio.ini` above.

## The loop contract

`poll()` does four things, in this order:

1. latch key edges (the matrix itself is scanned by an ISR at 150 µs per slot)
2. drain the MIDI input
3. advance the master timebase
4. flush gates and CV outputs that changed

It never blocks. Call it every iteration, and do not starve it: about 1 ms is
comfortable, 5 ms is the practical ceiling before key latency and MIDI input
degrade.

**LEDs are not flushed by `poll()`.** Brightness is an application choice, so
`led::flush(brightness)` is yours to call once the frame is drawn.

The timer interrupt belongs to the SDK. For work at tick time, register a hook:

```cpp
void onTick();                    // runs in the timer ISR: short, no SPI
clock::onHardwareTick(onTick);
```

## Module map

| module | header | reference |
|---|---|---|
| core | `cuisine.h` | [begin, poll](docs/api.md#core) |
| board | `board.h` | [pins, key names](docs/api.md#board) |
| clock | `clock.h` | [master timebase](docs/api.md#clock) |
| keys | `keyboard.h` | [step and command keys](docs/api.md#keys) |
| pot | `pot.h` | [endless potentiometer](docs/api.md#pot) |
| gate in | `gateIn.h` | [four inputs](docs/api.md#gate-in) |
| gate out | `gateOut.h` | [eight outputs](docs/api.md#gate-out) |
| CV | `DAC.h` | [four outputs](docs/api.md#cv) |
| LEDs | `LED.h` | [sixteen red/green + seven](docs/api.md#leds) |
| display | `display.h` | [numbers and bars](docs/api.md#display) |
| MIDI | `midiMinimal.h` | [UART1](docs/api.md#midi) |
| storage | `storage.h` | [FRAM and flash](docs/api.md#storage) |

## Five things that will bite you

1. **FRAM writes need `WREN` (0x06) before every write on this part**, although
   the datasheet calls it optional. Reads work without it, which is exactly why
   the omission looks fine until nothing persists. The driver does this for you;
   know it anyway, because a custom driver is easy to write and wrong.
2. **`clock::ticks()` counts 48 ticks per quarter, on every source.** MIDI
   produces the same grid (24 PPQN × 2), so neither mode needs special handling.
   Measured: 1065 ticks in 10.36 s at 124.7 BPM.
3. **The division byte:** bit 0 picks the family (1 = straight, 0 = triplet),
   bits 7..1 are the octave offset. `ticksPerStep(5)` is **12 ticks — a 1/16**,
   which is the sequencer's default. Dotted divisions cannot be represented at
   all.
4. **There is no scheduler.** A slow `poll()` delays key edges, MIDI parsing and
   the clock's output flush together.
5. **Flash links at `0x200`, so 48,640 B are usable, not 49,152.** PlatformIO
   will happily print a percentage against a size you cannot use.

## What is not here

Deliberately, and each for a reason:

- **no pattern, track or bank model** — the shape of that data is the app's
  identity; the sequencer in `src/` shows one answer, not the answer;
- **no per-track clock** — source, division, offset and play mode are workflow;
  `src/trackClock.h` is a worked example;
- **no storage layout** — the SDK moves bytes, address maps belong to the app.

## Licence

GNU General Public License v3.0 or later. See [../../LICENSE](../../LICENSE).

Free to use, study, share and modify. If you distribute a product built on it,
the GPL requires you to publish your source under the same licence.
