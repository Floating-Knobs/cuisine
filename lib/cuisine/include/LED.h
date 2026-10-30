// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <SPI.h>
#include <Arduino.h>
#include <elapsed.h>
#include <board.h>

#ifndef _LED_H_INCLUDED
#define _LED_H_INCLUDED

// #define PLAY_LED PIN_PA2
// #define RECORD_LED PIN_PA3
// #define OVERIDE_LED PIN_PA7

#define PLAY_LED 0
#define REC_LED 1
#define OVERIDE_LED 2
#define PAGE_1_LED 3
#define PAGE_2_LED 4
#define PAGE_3_LED 5
#define PAGE_4_LED 6

// Colour is a 2-bit mask, one bit per channel: Yellow == Red | Green.
// Kept as constants rather than macros so they cannot collide with user code.
namespace cuisine {
namespace led {

constexpr uint8_t Off    = 0;
constexpr uint8_t Red    = 1;
constexpr uint8_t Green  = 2;
constexpr uint8_t Yellow = 3;

} // namespace led
} // namespace cuisine

// // PWM parameters
// #define PWM_FREQUENCY  500 // PWM frequency in Hz
// #define PWM_RESOLUTION 64 // PWM resolution (8-bit resolution)
// #define PWM_PERIOD 1000000 / PWM_FREQUENCY // PWM period in microseconds
// #define PWM_PERIOD_RESOLUTION PWM_PERIOD / PWM_RESOLUTION // PWM period resolution in microseconds

class LEDclass
{
    public :
        static void update(byte brightness, bool forceUpdate = false);
        static void init();
        // static void resetOne(byte position);
        // static void resetAll();
        static void setOne(byte position, byte Color, bool dim = false);
        static void setTransport(byte position, bool state) { bitWrite(_transportLED, position, state); };
        static void setPage(byte size, byte active, byte maxPage, byte current, bool special = false);
        static void setAll( unsigned int data, byte color, bool dim = false);
        static void setSome( unsigned int data, byte color, bool dim = false);
        static bool blink();
        static bool blinkLow();
        static bool shortBlink();
        static bool dim();

    private :
        // static bool _LEDtable[16][2]; // -> unused ?
        static unsigned int _redLED;
        static unsigned int _greenLED;
        // static unsigned int _orangeLED;
        static unsigned int _dimLED;

        static unsigned int _oldRedLED;
        static unsigned int _oldGreenLED;
        // static unsigned int _oldOrangeLED;
        static unsigned int _oldDimLED;

        static bool _updateFlagStep;
        static byte _transportLED;
        static byte _oldTransportLED;
        static cuisine::Millis _blink;
        static cuisine::Millis _blinkLow;
        static cuisine::Millis _shortBlink;
        static unsigned int _PWM;

};

namespace cuisine {
namespace led {

// 16 red/green LEDs, colour a 2-bit mask: red, green, or both (yellow).
inline void set(uint8_t index, uint8_t color, bool dim = false) {
    LEDclass::setOne(index, color, dim);
}
inline void setMask(uint16_t mask, uint8_t color, bool dim = false) {
    LEDclass::setAll(mask, color, dim);
}
inline void overlay(uint16_t mask, uint8_t color, bool dim = false) {
    LEDclass::setSome(mask, color, dim);
}

// 7 transport LEDs.
inline void transport(uint8_t index, bool on) { LEDclass::setTransport(index, on); }

inline void page(uint8_t size, uint8_t active, uint8_t maxPage, uint8_t current,
                 bool special = false) {
    LEDclass::setPage(size, active, maxPage, current, special);
}

/// Push the current LED state to the hardware. Called once per poll by the app.
inline void flush(uint8_t brightness, bool force = false) {
    LEDclass::update(brightness, force);
}

// Shared blink phases.
inline bool blink()      { return LEDclass::blink(); }
inline bool blinkLow()   { return LEDclass::blinkLow(); }
inline bool shortBlink() { return LEDclass::shortBlink(); }

} // namespace led
} // namespace cuisine

#endif