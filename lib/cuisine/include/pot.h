// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef _POT_H_INCLUDED
#define _POT_H_INCLUDED

#include <board.h>

// Constant value definitions

#define ADC_HYSTERESIS  1       //Must be 1 or higher. Noise filter, determines how big ADC change needed
#define POT_SENSITIVITY 1        //Higher number = more turns needed to reach max value

#define ADC_MAX_VALUE 1023

#include <Arduino.h>
#include <elapsed.h>
class potClass
{
    public :
        static void init();
        // turn is fixed-point: 1024 == 1.0
        static int read(int input, int turn, int minValue, int maxValue);
        //static int readPosition();
        static bool readState(bool slew = false);
        static void updateReading();
        static void handleISR();
        // static byte averageMovement() { return abs((movement[0] + movement[1] + movement[2] + movement[3] + movement[4] + movement[5] + movement[6] + movement[7]) / 8); }
    private :
        //static void _updateReading();
        static volatile int valeurTapA;
        

        static volatile int valeurTapB;
        static volatile int valeurTap[2];
        static volatile bool _movingFlag;
        static volatile int _lastReadingA;
        static volatile int _lastReadingB;
        static bool _oldState;
        static bool _state;
        static int16_t angle;      // 0-2047 representing 0-2π
        static int16_t oldAngle;
        // static int movement[8];
        // static volatile int movement;
        // static byte movementIndex;
        static volatile bool _selectorFlag;
        static volatile uint32_t _lastMoveTime;
};

namespace cuisine {
namespace pot {

/// Apply the movement since the last call to a caller-owned value.
/// Argument order is unchanged from potClass::read: (value, turn, min, max).
/// `turn` is fixed point, 1024 == one full turn across the range.
inline int read(int value, int turn, int minValue, int maxValue) {
    return potClass::read(value, turn, minValue, maxValue);
}

/// True while the knob is inside the 125 ms movement grace window.
inline bool moving() { return potClass::readState(true); }

/// Raw movement flag: set by the ADC ISR, cleared by it after the grace.
inline bool movingLatch() { return potClass::readState(false); }

} // namespace pot
} // namespace cuisine

#endif