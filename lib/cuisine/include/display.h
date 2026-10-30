// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen


#ifndef DISPLAY_H
#define DISPLAY_H
#include <LED.h>
#include <elapsed.h>

class displayClass
{
    public :
        static void init();
        static void note(byte value);
        static void digitOne(byte value, byte offset);
        static void digit(unsigned int value);
        static void octaveNumber(int8_t value);
        static void value(int value, byte color, bool dim = false);
        static void offset(int value, bool dim = false);
        // static bool blink();
        static bool dim();
    private :
        //LEDclass _LED;
        const unsigned int _Keys = 0b101011010101;
        static const byte _octaveColor[5] PROGMEM;
        static cuisine::Millis _sinceBlink;
        static cuisine::Millis _sinceDim;
        static bool _blink;
        static bool _dim;
};

namespace cuisine {
namespace display {

/// 4-digit number renderer (one LED column per digit).
inline void number(uint16_t value) { displayClass::digit(value); }

/// Horizontal bar, 0..16 LEDs.
inline void bar(int value, uint8_t color, bool dim = false) {
    displayClass::value(value, color, dim);
}

/// Centred offset bar for signed values.
inline void offset(int value, bool dim = false) { displayClass::offset(value, dim); }

/// Octave sign + magnitude on LEDs 12/13.
inline void octave(int8_t value) { displayClass::octaveNumber(value); }

} // namespace display
} // namespace cuisine

#endif