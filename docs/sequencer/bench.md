# The bench protocol

The `bench` build is the sequencer plus a serial test surface. It exists because
there is no debugger on this board: the protocol is how a change gets checked
against the real hardware instead of only against the compiler.

It compiles only with `-DBENCH`, which is what `[env:bench]` sets. The production
firmware contains none of it — that is why the production image is silent.

## Framing

- USART0 at **115200**, the port of the CH340.
- One command per line, terminated by `\n`; `\r` is ignored.
- **Maximum line length is 24 characters** (`kLineMax`); anything longer is
  truncated, not rejected.
- One character per command, except `t` and `d`, which take arguments.
- Output is plain text, one or more lines per command; the host does the parsing.

## Commands

| command | arguments | prints | does |
|---|---|---|---|
| `?` | — | `?s t p o r h u d R C K D` | command list |
| `s` | — | `S <transport> <ticks> <bpm> <step> <gates>` | current state |
| `t<n>` | decimal count | `T <n>` | queue *n* deterministic ticks |
| `p` | — | `p` | transport start |
| `o` | — | `o` | transport stop |
| `r` | — | `r` | stop, then start |
| `h` | — | `h` | hold the tick timer |
| `u` | — | `u` | release the tick timer |
| `d<addr>,<len>` | hex address, decimal length | hex bytes, 16 per line, then `.` | dump FRAM |
| `R` | — | `R st=<status>` then `R PASS DE AD BE EF` | FRAM write/read self-test |
| `C` | — | `C st=<old> -> <new>` | clear the FRAM status register |
| `K` | — | `K <before> <after>` | toggle a step through the UI path |
| `D` | — | `D 0:2 1:3 ...` | division table, in ticks |

Details that matter:

- **`t<n>` is the point of the whole thing.** It stops the real timer and injects
  one tick per loop iteration, so a test does not depend on wall-clock time and
  two runs are identical. When the queue empties, the timer restarts unless it
  was held with `h`.
- **`s` fields** are: transport (0/1), master tick count, tempo in tenths of BPM,
  the current step of track 0, and the gate output mask.
- **`d`** caps the length at 64 bytes and prints 16 per line. The address is
  parsed as hex, the length as decimal: `d0e66,32`.
- **`R`** writes `DE AD BE EF` to `0x7000` and reads it back. It prints the status
  register first, because a chip left write-protected by previous firmware is the
  usual cause of a failure.
- **`K`** is hard-coded to track 0, step 1. It exists to exercise the
  application's own `writeStep` path rather than writing FRAM directly.

## A repeatable test

```
s          S 0 136 1247 0 0
t24        T 24
s          S 1 284 1247 8 0
R          R st=128
           R PASS DE AD BE EF
```

Reading it: the transport starts stopped with 136 ticks; 24 injected ticks move
the count to 284 and light gate 3 (mask 8) at step 7; the FRAM self-test passes
with status 128 (`WPEN` set, which is harmless because every write issues
`WREN`).

## What it is good for

| question | command |
|---|---|
| is the clock advancing? | `t<n>` then `s` |
| which step is playing? | `s` |
| did a gate fire? | `s`, gate mask |
| did my edit persist? | `d<address>,<len>` |
| is the FRAM chip unhappy? | `R`, then `C` |
| what does this division byte mean? | `D` |

## What it is not

- Not a binary protocol: it is text for a human with a terminal.
- Not remote control: it cannot change patterns or presets, only inspect and
  inject ticks.
- Not present in production. If `s` prints nothing, you flashed the silent
  image — see [firmware.md](../firmware.md#the-bench-firmware).
