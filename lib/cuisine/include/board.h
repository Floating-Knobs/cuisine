// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

/**
 * board.h — every piece of Cuisine wiring, in one place.
 *
 * Nothing else in lib/cuisine names a pin. The constants live in
 * `cuisine::board`; the bare names at the bottom are compatibility aliases for
 * the sequencer application and will disappear once it migrates.
 *
 * MCU: ATmega4809 @ 20 MHz, 48 KB flash, 6 KB SRAM.
 * Storage: 32 KB FRAM (MB85RS256) + 1 MB SPI flash (ZD25Q80C), 3.3 V through
 * 2.2k/3.3k dividers on SCK/MOSI/HOLD, so SPI runs at 4 MHz.
 */
#ifndef BOARD_H
#define BOARD_H

#include <Arduino.h>

namespace cuisine {
namespace board {

// ─── SPI chip selects and latches ────────────────────────────────────
constexpr uint8_t SSLedPin          = 12;  // PB4  16 red/green LED register
constexpr uint8_t SSLedtransportPin = 2;   // PA2  7 transport LEDs
constexpr uint8_t SSRegisterPin     = 9;   // PB1  8 gate outputs
constexpr uint8_t SSDAC0            = 10;  // PB2  CV channels 0..1
constexpr uint8_t SSDAC1            = 11;  // PB3  CV channels 2..3
constexpr uint8_t SSFram            = 13;  // PB5
constexpr uint8_t SSFlash           = 19;  // PC5
constexpr uint8_t holdFlashPin      = 16;  // PC2
constexpr uint8_t holdFramPin       = 17;  // PC3

// ─── Analog ──────────────────────────────────────────────────────────
constexpr uint8_t POT_A_INPUT = PIN_PF2;  // PF2, ADC AIN12
constexpr uint8_t POT_B_INPUT = PIN_PF3;  // PF3, ADC AIN13
constexpr uint8_t PWM_RED     = PIN_PF4;  // PF4
constexpr uint8_t PWM_GREEN   = PIN_PF5;  // PF5

// ─── Keys ────────────────────────────────────────────────────────────
constexpr uint8_t keysPin    = 22;  // PD0, inverted
constexpr uint8_t commandPin = 23;  // PD1, inverted
constexpr uint8_t armPin     = 18;  // PC4
constexpr uint8_t muxPort    = 0;   // PE0..PE3, multiplexer channel select

// Silkscreen keys as single-bit masks. Combinations are `key | kKeyShift`
// (interface.h builds PAGE, SCALE, ACCENT and friends this way).
enum CommandKey : uint16_t {
    kKeyShift    = 1u << 0,
    kKeyLoop     = 1u << 1,
    kKeyDivMult  = 1u << 2,
    kKeyClear    = 1u << 3,
    kKeyPaste    = 1u << 4,
    kKeyCopy     = 1u << 5,
    kKeyTrack    = 1u << 6,
    kKeyStepEdit = 1u << 7,
    kKeyNote     = 1u << 8,
    kKeyEuclid   = 1u << 9,
    kKeyPattern  = 1u << 10,
    kKeyPage     = 1u << 11,
    kKeyRec      = 1u << 12,
    kKeyPlay     = 1u << 13,
};

// ─── Gates ───────────────────────────────────────────────────────────
constexpr uint8_t GATE_A_INPUT = PIN_PD2;
constexpr uint8_t GATE_B_INPUT = PIN_PD3;
constexpr uint8_t GATE_C_INPUT = PIN_PD4;
constexpr uint8_t GATE_D_INPUT = PIN_PD5;
constexpr uint8_t GATEnumber   = 8;

// ─── CV and LEDs ─────────────────────────────────────────────────────
constexpr uint8_t DAC_CHANNELS = 4;
constexpr uint8_t LEDnumber    = 16;
constexpr uint8_t kStepKeyCount = 16;

} // namespace board
} // namespace cuisine

// Compatibility aliases for the sequencer application.
using cuisine::board::SSLedPin;
using cuisine::board::SSLedtransportPin;
using cuisine::board::SSRegisterPin;
using cuisine::board::SSDAC0;
using cuisine::board::SSDAC1;
using cuisine::board::SSFram;
using cuisine::board::SSFlash;
using cuisine::board::holdFlashPin;
using cuisine::board::holdFramPin;
using cuisine::board::POT_A_INPUT;
using cuisine::board::POT_B_INPUT;
using cuisine::board::PWM_RED;
using cuisine::board::PWM_GREEN;
using cuisine::board::keysPin;
using cuisine::board::commandPin;
using cuisine::board::armPin;
using cuisine::board::muxPort;
using cuisine::board::GATE_A_INPUT;
using cuisine::board::GATE_B_INPUT;
using cuisine::board::GATE_C_INPUT;
using cuisine::board::GATE_D_INPUT;
using cuisine::board::GATEnumber;
using cuisine::board::DAC_CHANNELS;
using cuisine::board::LEDnumber;

#endif
