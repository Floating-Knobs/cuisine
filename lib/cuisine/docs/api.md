# cuisine API reference

One section per module. Signatures are copied from the headers; anything not
listed here is internal and may change.

Conventions used throughout:

| quantity | unit |
|---|---|
| tempo | tenths of BPM — `1200` is 120.0 |
| clock ticks | 48 per quarter, on every source |
| division | packed byte, see [`clock::ticksPerStep`](#ticksperstep) |
| pot `turn` | fixed point, 1024 = one full turn across the range |
| CV | DAC counts 0..4095, about 1 V/oct |
| LED colour | `led::Off`, `led::Red`, `led::Green`, `led::Yellow` |

---

## core

`#include <cuisine.h>`

```cpp
void cuisine::begin();
void cuisine::poll();
```

`begin()` brings up every block: LEDs, key matrix, CV, gates, pot, gate inputs,
clock and MIDI. Call it once, first. It includes the keyboard scan settle (one
full matrix pass), so key edges are valid immediately after it returns — no
delay needed.

`poll()` advances the hardware by one slice: latches key edges, drains MIDI,
advances the master timebase, flushes gates and CV. Never blocks. Call once per
loop iteration.

It does **not** flush the LEDs; call [`led::flush`](#leds) when your frame is
drawn.

---

## board

`#include <board.h>`

Every pin in one place; no other header names hardware.

```cpp
namespace cuisine::board {
    constexpr uint8_t SSLedPin = 12;    // LED register latch
    constexpr uint8_t SSLedtransportPin = 2;
    constexpr uint8_t SSRegisterPin = 9;   // gate outputs
    constexpr uint8_t SSDAC0 = 10, SSDAC1 = 11;
    constexpr uint8_t SSFram = 13, SSFlash = 19;
    constexpr uint8_t holdFlashPin = 16, holdFramPin = 17;
    constexpr uint8_t POT_A_INPUT = PIN_PF2, POT_B_INPUT = PIN_PF3;
    constexpr uint8_t PWM_RED = PIN_PF4, PWM_GREEN = PIN_PF5;
    constexpr uint8_t keysPin = 22, commandPin = 23, armPin = 18, muxPort = 0;
    constexpr uint8_t GATE_A_INPUT = PIN_PD2;  // ..D
    constexpr uint8_t GATEnumber = 8, DAC_CHANNELS = 4, LEDnumber = 16;
    constexpr uint8_t kStepKeyCount = 16;

    enum CommandKey : uint16_t {
        kKeyShift, kKeyLoop, kKeyDivMult, kKeyClear, kKeyPaste, kKeyCopy,
        kKeyTrack, kKeyStepEdit, kKeyNote, kKeyEuclid, kKeyPattern,
        kKeyPage, kKeyRec, kKeyPlay
    };  // one bit each; combinations are key | kKeyShift
}
```

Command keys are bit masks, not indices: `keys::commandPressed(board::kKeyPlay | board::kKeyShift)`.

---

## clock

`#include <clock.h>`

The master timebase. One per device; there is no per-track clock in the SDK.

```cpp
namespace cuisine::clock {
    constexpr uint8_t kInternal = 0, kMidi = 1, kModular = 2;

    void     setSource(uint8_t source);
    uint8_t  source();

    void     setTempoTenths(uint16_t bpm);   // 1200 = 120.0
    uint16_t tempoTenths();

    void start();
    void stop();
    bool started();

    bool tick();        // a master tick happened during the last poll()
    uint16_t ticks();   // tick counter since power-up
    bool takeReset();   // a master reset occurred since the last call

    bool transportStarted();   // one-poll edges
    bool transportStopped();

    void poll();               // called by cuisine::poll(), not by the app
    void onHardwareTick(void (*handler)());

    uint16_t ticksPerStep(uint8_t division);
}
```

| source | tick rate | driven by |
|---|---|---|
| `kInternal` | 48 per quarter | the on-board timer, tempo from `setTempoTenths` |
| `kMidi` | 48 per quarter | incoming MIDI clock (24 PPQN × 2), with half-period extrapolation |
| `kModular` | gate in 0 | rising edges on gate input 0 |

The internal rate is **48 ticks per quarter**, measured on the board: at 124.7
BPM the tick counter advanced 1065 times in 10.36 s, which is 102.8 ticks/s, or
48.2 per quarter. MIDI produces the same grid (24 PPQN × 2), which is why both
sources behave identically.

`tick()` is true exactly on the polls where the master advanced. Test it in
`loop()` and do one step of work.

`onHardwareTick` registers work to run inside the timer ISR, after the tick is
latched. Keep it short and do not touch SPI there. One handler; a second call
replaces the first.

### ticksPerStep

```cpp
uint16_t cuisine::clock::ticksPerStep(uint8_t division);
```

Pure function, no state. The stored division byte: **bit 0 = family**
(1 = straight, 0 = triplet), **bits 7..1 = octave offset**.

| division | ticks | note value at 48 ticks/quarter |
|---|---|---|
| 0 | 2 | 1/96 |
| 1 | 3 | 1/64 |
| 2 | 4 | 1/48 |
| 3 | 6 | 1/32 |
| 4 | 8 | 1/24 |
| **5** | **12** | **1/16** |
| 6 | 16 | 1/12 |
| 7 | 24 | 1/8 |
| 8 | 32 | 1/6 |
| 9 | 48 | 1/4 |

The sequencer's default is byte **5, which is a 1/16** — the common case, and
deliberately not changed because every saved pattern depends on it.

Triplet values are `2 << n`, straight values `3 << n`. **Dotted divisions cannot
be represented**: there is no third family bit.

---

## keys

`#include <keyboard.h>`

```cpp
namespace cuisine::keys {
    uint16_t steps();            // level mask, 16 step keys
    uint16_t stepsPressed();     // pressed since the last poll
    uint16_t stepsReleased();

    bool down(uint8_t i);
    bool pressed(uint8_t i);
    bool released(uint8_t i);

    uint16_t commands();         // level mask, 14 command keys
    uint16_t commandsPressed();
    bool command(uint16_t mask);
    bool commandPressed(uint16_t mask);
    bool commandReleased(uint16_t mask);

    bool arm();                  // arm switch, level
}
```

Edges are latched by `cuisine::poll()`, so they are valid for exactly one loop
iteration. There is no `commandsReleased()` mask — release edges exist per
individual command, through `commandReleased(mask)`.

Key names are in [`board::CommandKey`](#board).

---

## pot

`#include <pot.h>`

One endless potentiometer, read as two wipers forming a vector. No end stop, no
push switch. The value lives in your code, not in the driver.

```cpp
namespace cuisine::pot {
    int  read(int value, int turn, int minValue, int maxValue);
    bool moving();       // inside the 125 ms movement window
    bool movingLatch();  // raw flag: set by the ADC interrupt
}
```

```cpp
static int s_length = 16;
s_length = pot::read(s_length, /*turn*/ 1024, 1, 16);
```

`turn` is fixed point: 1024 means one physical turn spans the whole range;
2048 is twice as fine. `read` applies the movement since the previous call, so
call it regularly — at least every 125 ms, or slow turns are dropped.

Use `moving()` to know whether to show the value on the LEDs.

---

## gate in

`#include <gateIn.h>`

```cpp
namespace cuisine::gate_in {
    bool high(uint8_t i);    // 0..3, level
    bool rising(uint8_t i);  // a rising edge since the last poll
}
```

Interrupt-driven; no polling required. Used by the clock in `kModular` mode and
available for anything else — reset inputs, footswitches, triggers.

---

## gate out

`#include <gateOut.h>`

```cpp
namespace cuisine::gate_out {
    void    set(uint8_t i, bool on);  // 0..7
    uint8_t mask();                   // all eight
    bool    get(uint8_t i);
}
```

Written through one shift register, flushed by `cuisine::poll()` only when it
changed. Index 0..7 is logical; `board.h` owns the physical wiring.

---

## cv

`#include <DAC.h>`

```cpp
namespace cuisine::cv {
    void setNote(uint8_t channel, uint8_t midiNote);  // 0..3, 1 V/oct
    void setRaw(uint8_t channel, uint16_t counts);    // 0..4095
}
```

`setNote` writes `note × 41.67` DAC counts, which the analog stage maps to
1 V/oct at 83.3 mV per semitone; note 0 is zero volts. `setRaw` writes counts
directly for anything that needs an exact voltage.

---

## LEDs

`#include <LED.h>`

```cpp
namespace cuisine::led {
    constexpr uint8_t Off = 0, Red = 1, Green = 2, Yellow = 3;

    void set(uint8_t i, uint8_t colour, bool dim = false);       // 0..15
    void setMask(uint16_t mask, uint8_t colour, bool dim = false);
    void overlay(uint16_t mask, uint8_t colour, bool dim = false);

    void transport(uint8_t i, bool on);   // 0..6: PLAY, REC, OVR, PAGE 1..4
    void page(uint8_t size, uint8_t active, uint8_t maxPage,
              uint8_t current, bool special = false);

    void flush(uint8_t brightness, bool force = false);  // 1..31

    bool blink(); bool blinkLow(); bool shortBlink();
}
```

Colours are 2-bit masks, so `Yellow == Red | Green`. `set` replaces one LED,
`overlay` adds to the existing state, `setMask` replaces a set of them.

`flush` is what actually sends the frame; call it once per drawn frame. Brightness
1..31; `force` resends even if nothing changed.

`blink()`, `blinkLow()` and `shortBlink()` return the shared phase booleans so
several indicators can blink together.

---

## display

`#include <display.h>`

The "display" is the 16-LED bar used as an output device.

```cpp
namespace cuisine::display {
    void number(uint16_t value);              // 4 digits, one LED column each
    void bar(int value, uint8_t colour, bool dim = false);   // 0..16
    void offset(int value, bool dim = false); // centred signed bar
    void octave(int8_t value);                // sign + magnitude
}
```

These write the LED state; `led::flush()` still publishes it.

---

## MIDI

`#include <midiMinimal.h>`

```cpp
namespace cuisine::midi {
    void init(uint8_t channel);
    void poll();                              // called by cuisine::poll()

    void onNoteOn(MidiNoteHandler h);         // void(*)(ch, note, velocity)
    void onNoteOff(MidiNoteHandler h);
    void onControlChange(MidiCcHandler h);    // void(*)(ch, cc, value)
    void onClock(MidiSimpleHandler h);        // void(*)(void)
    void onStart(MidiSimpleHandler h);
    void onStop(MidiSimpleHandler h);

    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendControlChange(uint8_t cc, uint8_t value, uint8_t channel);
    void sendClock();
    void sendStart();
    void sendStop();
}
```

UART1 at 31250 baud. Callbacks are plain function pointers — no lambdas with
captures, no `std::function`, and nothing allocated.

Incoming clock messages do not move the timebase by themselves: the clock only
follows MIDI when `clock::setSource(clock::kMidi)`.

---

## storage

`#include <storage.h>`

The SDK moves bytes. Address maps, save formats and versioning belong to the
application.

### FRAM

32 KB, byte addressable, no erase, survives power loss. Object:
`cuisine::storage::fram`.

```cpp
void init();

void readBytes(uint32_t address, void* buffer, uint16_t length);
void writeBytes(uint32_t address, const void* buffer, uint16_t length);

uint8_t read(uint32_t address);
void    write(uint32_t address, uint8_t value);

uint8_t status();
void    writeStatus(uint8_t value);

template <typename T> T    get(uint32_t address, T& value);       // fills and returns
template <typename T> void put(uint32_t address, const T& value);
```

`get` fills the reference **and** returns it, matching the old `EEPROM_SPI_WE`
convention; call sites that ignore the return still see the value in the
argument.

`init()` also drives the chip's `HOLD` pin high. Without that every transfer
pauses mid-frame and reads come back as garbage — the driver used to leave this
to the application.

Writes issue `WREN` before every operation. The status register is non-volatile:
a previous firmware can leave the chip write-protected (`WPEN`, bit 7), in which
case writes silently do nothing until `status()` and `writeStatus(0)` clear it.

### flash

1 MB, 256 B pages, 4 KB sectors, 32 KB blocks. Object:
`cuisine::storage::flash`.

```cpp
bool initialize();

void readBytes(uint32_t address, uint8_t* buffer, uint16_t length);
void writeBytes(uint32_t address, const uint8_t* buffer, uint16_t length);

void blockErase32K(uint32_t address);
void sectorErase(uint32_t address);      // 4 KB

uint32_t size() const;
bool     ready() const;
```

You must erase before writing: a sector erase for up to 4 KB, a block erase for
more. A 32 KB block erase takes hundreds of milliseconds — never do it inside
`poll()`, or key edges and MIDI input are lost for the duration. Do it in chunks,
or while the panel is idle.

Both memories run at `F_CPU/2` — 10 MHz on this board, the fastest the AVR can
drive SPI (the parts allow 25 and 104 MHz). Measured on the bench at that speed:
256 B FRAM write and read-back verify, and a 4 KB sector erase takes 6.8 ms
followed by a 256 B program and verify.

The flash shares the SPI bus with the LEDs, gates, DAC and FRAM. The DAC also
runs at 10 MHz; the shift registers use the Arduino default divider at 5 MHz.
Between operations the flash is deselected.

---

## Internal

`timer.h` is included by `cuisine.h` for historical reasons. `timerClass` drives
the clock's tick and is not part of the public surface: use
[`clock::onHardwareTick`](#clock) instead.
