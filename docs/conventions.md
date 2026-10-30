# Conventions

How the code is written here, and the constraints that explain why it looks the
way it does.

## The two rules that matter

1. **`lib/cuisine` never includes an application header.** No `track.h`, no
   `interface.h`, nothing from `src/` or `include/`. The SDK must build alone.
2. **Nothing allocates.** No `new`, no `malloc`, no `std::vector`, no
   `String`. Every byte of RAM is accounted for at compile time.

Everything below is a consequence of those two, or of the 48 KB of flash.

## Language

The toolchain compiles **gnu++17 with `-fno-exceptions`**, but the code stays
inside a C++11-compatible subset so it also builds on older cores. Beyond that,
a deliberately small set of features:

- **no virtual functions** — a vtable costs RAM per object and a call through a
  pointer costs flash;
- **no exceptions** — the core compiles with `-fno-exceptions`, and this code
  never relies on unwinding;
- **no `std::function`, no lambdas with captures** — callbacks are plain
  function pointers (`MidiNoteHandler`, `void(*)()`);
- **no heap** — `new` would compile, and there is nowhere to get the memory
  from after the first hour.

The SDK's public API is namespaced free functions forwarding to static classes
in the header. That keeps the call zero-cost: the compiler inlines the forwarder
and the underlying call compiles exactly as if it had been written directly.

**Parenthesise macros used inside expressions.** `#define F_CPU_PERIOD F_CPU /
1000000` is harmless as `period *= F_CPU_PERIOD`, and silently wrong as
`(600000000 / x) * F_CPU_PERIOD`: the compiler sees `* F_CPU / 1000000`, multiplies
by 20,000,000 first, and overflows 32 bits. That cost an afternoon measuring a
timer whose arithmetic read correctly.

## Memory

Static storage, sized at compile time. The stack lives inside the same 6 KB as
everything else, so large frames are not free:

- the biggest frame in this tree is **275 B**;
- there was a version with two 1 KB bank buffers on the stack — 1,042 B of
  frame, on a 6 KB device. It is now one 256 B buffer that matches the flash
  page size, so the flash is written the same way with a quarter of the stack;
- when a function needs to touch a big structure, chunk it through a static
  buffer rather than copying it through the stack.

The pass that produced these rules took static RAM from 4,336 to 3,280 B and
flash from 48,682 to 46,928 B, and the biggest stack frame from 1,042 to 275 B.

**Bound indices that come from data.** A step number, a byte read from FRAM, a
pot value: clamp before subscripting. `euclid.getPattern(255)` used to read past
its array and light random LEDs — an array bug that looked like an LED bug.

**Size counters to the range they must reach.** A `uint8_t` holds 0..255 and
never becomes 256:

```c
for (uint8_t i = 0; i < 256; i++)    // infinite: 255 + 1 wraps to 0
for (uint8_t i = 0; i <= 255; i++)   // identical infinite loop, other spelling
```

The comparison is done as `int`, so `i <= 255` is always true, and the increment
wraps. The compiler may warn that the comparison is always true; the build does
not fail. This one cost a debugging session on the memory test — the symptom was
silence on serial, which looked like a bad upload — and is why its counters are
`uint16_t`.

`for (uint8_t i = 0; i < 255; i++)` does terminate, but runs 255 times instead of
256: a silent off-by-one. If the count matters, widen the counter.

## Flash

`-Os` with LTO and `--gc-sections`. The ceiling is **48,640 B**, not 49,152 (see
[firmware.md](firmware.md#reading-the-numbers)), and the application sits at
96.5 % of it.

- **measure after every change**; a jump you cannot explain is usually a
  function that stopped being inlined or a struct that stopped fitting a
  register;
- **strings are the cheapest win**: `F("...")` keeps them in flash, and
  shortening a message is often the whole difference;
- **avoid templates that instantiate per type** unless the call is trivial; the
  storage layer has two (`get`/`put`) and they are the exception, not the rule.

## Layout and naming

| thing | convention |
|---|---|
| SDK public surface | `cuisine::` namespace — `clock`, `keys`, `cv`, `led`, ... |
| SDK implementation | static classes in the header, `XxxClass` legacy names |
| application | `interfaceClass`, `trackClass`, `trackClock`, legacy names kept |
| constants | `kSomething` in the SDK; `SCREAMING_CASE` macros survive in the app |
| files | one class per pair, `.h` in `include/`, `.cpp` in `src/` |
| licence | `SPDX-License-Identifier` and the copyright line, first two lines of every `.cpp` and `.h` |

The application keeps its historical names on purpose. Renaming 6,000 lines to
chase a convention would produce a diff nobody can review, and the names are not
wrong — they are merely old.

## Style

**Formatting is inherited, not imposed.** Match the file you are editing: the
tree uses both 2- and 4-space indentation, and both brace styles, depending on
when a file was written. That inconsistency is cosmetic and not worth a mass
reformat.

What is not negotiable: **do not reformat as a side effect.** A whitespace diff
hides the three lines that actually matter, and this repository has more
load-bearing comments than average.

## Comments

Say **why**, not what. The comments that matter in this codebase are the ones
recording a surprise:

- `WREN` before every FRAM write, despite the datasheet calling it optional;
- the HOLD pin, without which transfers pause mid-frame;
- the division byte encoding, and that byte 5 is a 1/16;
- `reset()` must run *inside* `update()`, after the flag swap, or the reset is
  swallowed.

A comment that restates the code is noise. A comment that stops the next person
from "fixing" a deliberate choice is the most valuable line in the file.

## Changing saved data

Every struct stored in FRAM or flash has `sizeof` and `offsetof` assertions in
`include/structDefinition.h`:

```cpp
static_assert(sizeof(patternStruct) == 153, "saved-data layout changed: patternStruct");
LAYOUT_OFFSET(patternStruct, step, 0);
```

Adding a field means:

1. updating the assertions — the build will tell you if you forget;
2. answering what happens to data saved by the **previous** firmware. There is
   no migration framework. The accepted answers are a new address, a version
   byte, or a deliberate break documented in the commit.

Bank and preset formats are the user's data. Treat a layout change as a
breaking change even when the compiler is happy.

## How "verified" is used here

A change is verified when it **builds and was checked on the board**, not when
it compiles and looks right.

In practice that means: build both environments, flash the one under test, and
observe the behaviour — the bench protocol for state, the panel for anything
visual, FRAM dumps for persistence. Commits describe what was flashed and what
was seen, so the next person knows which claims rest on evidence.

Nothing in this repository has unit tests. The hardware is the test rig, and a
claim that was not checked against it is a guess worth labelling as one.
