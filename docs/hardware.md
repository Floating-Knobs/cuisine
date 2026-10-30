# Hardware

The board itself: what is on it, how fast it runs, and what the pins do.

[`board.h`](../lib/cuisine/include/board.h) is the single source of truth for
pin names; this page explains them. If the two ever disagree, the header wins.

## At a glance

| | |
|---|---|
| MCU | ATmega4809, AVR128 core, 20 MHz |
| flash | 48 KB, of which 48,640 B usable (see [boot](#boot-and-flashing)) |
| SRAM | 6 KB |
| FRAM | 32 KB, MB85RS256, byte addressable, no erase |
| SPI flash | 1 MB, ZD25Q80C, 256 B pages, 4 KB sectors, 32 KB blocks |
| panel keys | 16 step, 14 command, one arm switch |
| other input | one endless potentiometer, 4 gate inputs |
| outputs | 8 gate, 4 CV (12-bit), 16 red/green LEDs, 7 transport LEDs |
| MIDI | in and out on USART1 |

## Power and levels

Two voltage domains. The MCU, the panel and the CV/gate outputs are 5 V; both
memory chips are 3.3 V. SCK, MOSI and the two HOLD lines cross through
2.2k/3.3k dividers, which is why the memories are the slowest thing on the bus
even though the parts could run far faster.

The HOLD pins must be driven high or the chips ignore the bus. Both drivers do
this in their `init()`; an application that drives the memories itself must do
the same.

## The SPI bus

One bus, seven chip selects, three speeds. "Measured" means verified on this
board with a 256-byte pattern written and read back.

| device | select | clock | note |
|---|---|---|---|
| FRAM | PB5 (13) | 10 MHz | measured, 256 B write + verify |
| SPI flash | PC5 (19) | 10 MHz | measured, 4 KB erase 6.8 ms |
| DAC 0 / 1 | PB2 (10), PB3 (11) | 10 MHz | `SPISettings(F_CPU)` |
| gate register | PB1 (9) | 10 MHz | 8 bits per frame |
| LED register | PB4 (12) | 10 MHz | 32 bits per frame |
| transport latch | PA2 (2) | 10 MHz | 8 bits per frame |

10 MHz is `F_CPU/2`, the fastest an AVR can drive SPI — the MCU is the limit,
not the parts (FRAM 25 MHz, flash 104 MHz). Mode 0, MSB first.

Every path uses `beginTransaction`/`endTransaction`, so no device leaves the bus
at a divider that surprises the next one.

## Pin map

| pin | port | function |
|---|---|---|
| 12 | PB4 | LED register latch (16 LEDs, 32 bits) |
| 2 | PA2 | transport LED latch (7 LEDs) |
| 9 | PB1 | gate output register latch (8 bits) |
| 10 | PB2 | DAC 0 chip select (CV 0, 1) |
| 11 | PB3 | DAC 1 chip select (CV 2, 3) |
| 13 | PB5 | FRAM chip select |
| 19 | PC5 | SPI flash chip select |
| 16 | PC2 | flash HOLD |
| 17 | PC3 | FRAM HOLD |
| 22 | PD0 | step keys, inverted, pulled up |
| 23 | PD1 | command keys, inverted, pulled up |
| 18 | PC4 | arm switch, pulled up |
| PE0–PE3 | — | multiplexer address, panel key select |
| PF2 | — | pot wiper A (ADC) |
| PF3 | — | pot wiper B (ADC) |
| PF4 | — | panel PWM, red |
| PF5 | — | panel PWM, green |
| PD2–PD5 | — | gate inputs 0–3 |

## Panel input

**Keys.** Sixteen step keys and fourteen command keys share one 16-channel
multiplexer addressed on PE0–PE3, with the multiplexed state read on PD0 (steps)
and PD1 (commands). Both inputs are inverted and pulled up, so a pressed key
reads high. One address drives two switches — the step key and the command key
on that channel — which is how 30 keys fit on 16 channels.

**Scan timing.** TCB0 interrupts every **60 µs** (`CCMP = 600`, `CLK_PER/2` at
20 MHz). The scanner walks 32 slots per pass, so every channel is refreshed
within **1.92 ms** — that interval is the floor on key latency, and it is why
`begin()` waits for one pass before trusting edges.

**Pot.** Two wipers 90° apart on PF2 and PF3. Their angle is decoded into
relative movement, so the knob has no end stops and no absolute position: the
value lives in the application. A movement window of 125 ms defines `moving()`.

## Outputs

**Gates.** Eight bits in one shift register, latched on PB1. The logical index
is remapped on the way out (4 → 5, 5 → 7, 7 → 4), which `board.h` owns; the API
index is always 0..7.

**CV.** Two dual 12-bit DACs on PB2 and PB3, written as 16-bit frames: bit 15
selects the channel inside the chip, bit 12 enables the output. The application
sets a count from 0 to 4095 or a MIDI note; note 0 is zero volts and each note
is 83.3 mV.

**LEDs.** Sixteen red/green LEDs — 32 bits, green first then red, then a latch
pulse on PB4. Colour is a 2-bit mask, so red, green, or both (yellow).

**Transport LEDs.** Seven more, independent of the 16, shifted out and latched
on PA2: PLAY, REC, OVR, PAGE 1..4.

**Display.** There is no display chip. "Digits" are drawn on the 16 LEDs by the
`display` module.

## MIDI

USART1 at 31250 baud. The USB serial port used for the test protocol is USART0
at 115200 through the CH340.

## Memory

**FRAM** holds the live data — patterns, presets, banks — and survives power
loss without an erase step. Writes need a `WREN` before every operation on this
part, despite the datasheet calling it optional.

**SPI flash** holds archived banks. It must be erased before writing, in 4 KB
sectors or 32 KB blocks, and an erase blocks the bus for milliseconds: 6.8 ms
for a sector, roughly 55 ms for a block. Never inside `poll()`.

## Boot and flashing

A 512-byte Optiboot bootloader occupies `0x0000`–`0x01FF`, so the application
links at **`0x200`** and **48,640 B are usable**, not the 49,152 B the flash has.

Uploads use the Arduino protocol over the CH340 at 115200 baud: `avrdude`, the
`arduino` upload protocol, device signature `0x1e9651`. The board resets into
the bootloader through DTR.

Brown-out flags are cleared at startup so a marginal supply cannot leave a stale
reset cause behind.
