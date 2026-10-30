# Firmware: build, flash, test

How to build this repository, put it on the board, and find out what it is doing
once it is there. There is no debugger: everything below is the substitute.

## Environments

`platformio.ini` defines two that are in use:

| environment | what it builds | when |
|---|---|---|
| `ATmega4809` | the sequencer, silent | production, everything you ship |
| `bench` | the same application plus a serial test protocol | testing and debugging |

The production image prints nothing on serial. That is deliberate — the USART is
free for the application — and it is why the bench build exists.

Two more environments are leftovers from the initial board bring-up:
`fuses_bootloader` and `custom_fuses`. They were used once, to flash the
bootloader and set fuses over `jtag2updi`. They are kept for reference and are
not part of any build.

## Build

```
pio run -e ATmega4809          # production
pio run -e bench               # test firmware
```

A cold build takes 10–15 s, an incremental one 2–5 s.

### Reading the numbers

PlatformIO prints a percentage against 49,152 B. **That number is wrong for this
board.** A 512-byte bootloader occupies `0x0000`–`0x01FF`, so the application
links at `0x200` and the real ceiling is **48,640 B**.

| build | flash | of 48,640 | RAM | of 6,144 |
|---|---|---|---|---|
| production | ~47 KB | ~96 % | 3,280 B | 53.4 % |
| bench | ~48.5 KB | ~99 % | 3,456 B | 56.2 % |

The build prints the exact bytes and they move by a few dozen with each change,
so treat these as magnitudes, not constants.

The bench build is close enough to the ceiling that a new string or a verbose
`Serial.print` can push it over. When it does, the build fails with
`address 0xc0xx out of range` — avrdude's message, not a friendly one — and the
fix is to shorten a string or drop a format argument, not to disable warnings.

## Flash

```
pio run -e ATmega4809 -t upload --upload-port COM5
```

`-t upload` rebuilds first, so there is no separate build step. A flash takes
about 17 s: write, then verify on the way back. The board resets into its
bootloader over DTR, so the port must be the CH340.

**Finding the port.** It changes when the board is replugged, and it is not
always the one you used last:

```
pio device list
```

Everything in this project has been flashed through a CH340
(`VID_1A86&PID_7523`), signature `0x1e9651`.

## The bench firmware

```
pio run -e bench -t upload --upload-port COM5
```

Same application, plus a one-character-per-command protocol on USART0 at
115200. The port is at the default `monitor_speed = 115200` for this build.

| command | does |
|---|---|
| `?` | print the command list |
| `s` | status: transport, tick count, BPM, current step, gate mask |
| `t<n>` | inject `n` deterministic ticks, one per loop, timer off |
| `p` | transport start |
| `o` | transport stop |
| `r` | stop, then start |
| `h` / `u` | hold / release the tick timer |
| `d<addr>,<len>` | dump FRAM, hex address and length |
| `R` | FRAM write/read self-test at `0x7000` |
| `C` | clear the FRAM status register (unprotect) |
| `K` | toggle a step through the UI path |
| `D` | print the division table and the ticks each division produces |

### A worked session

```
s   -> S 0 136 1247 0 0     stopped, 136 ticks, 124.7 BPM, step 0
p   -> p                    transport started
t24 -> T 24                 inject 24 ticks
s   -> S 1 284 1247 8 0     running, ticks advanced, gate mask 8 (step 7)
o   -> o                    stopped
```

Deterministic ticks are the important part: `t<n>` stops the timer and advances
the sequencer one tick per loop iteration, so a test does not depend on real
time and every run is identical.

## Debugging without a debugger

No ICE on this board. What is left, in order of usefulness:

1. **The bench protocol.** It can tell you the transport state, the tick count,
   the current step, which gates are on, and dump any FRAM range — enough to
   separate "the input never arrived" from "the output never fired".
2. **Memory dumps.** `d<preset address>,<len>` shows what was actually saved,
   which is usually faster than reading the save code again.
3. **LEDs as a signal.** A temporary `cuisine::led::set(0, cuisine::led::Red)`
   inside a branch you are unsure about is one line and visible immediately.
4. **Size as a regression signal.** Build after every change; a jump you cannot
   explain is often a structure that stopped being inlined.

## Traps

- **The CH340 wedges.** Symptoms: `cannot set com-state`, "Device Descriptor
  Request Failed" (code 43). Only a physical replug fixes it; restarting the
  device in Windows needs elevation and usually does not help.
- **`upload_protocol` must be `arduino`.** The default is `jtag2updi`, which
  fails with `unable to open programmer`. Any new environment needs the line
  `upload_protocol = arduino`.
- **A header change means a full rebuild.** Expect ~8 s, not 2 s.
- **The production image is silent.** If you flash it and open a terminal, you
  will see nothing — that is not a crash. Flash `bench` to talk to it.
