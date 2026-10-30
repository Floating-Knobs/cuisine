// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <arduino.h>
#include <pot.h>
#include <elapsed.h>	
#include <util/atomic.h>
#include <avr/pgmspace.h>

#define ADC_SHIFT_DIV64 (6)
#define THRESHOLD_START 20
#define THRESHOLD_CONTINUE 8
#define MOVE_GRACE_MS 125

// Indices of the two wipers. Private to the driver: these used to be A/B
// macros in the public header.
namespace {
constexpr uint8_t A = 0;
constexpr uint8_t B = 1;
}
// 64-entry lookup table for atan2 (one octant: 0-45 degrees)
// Subsampled from 256-entry table (every 4th). Units: 1/2048 of a full circle.
// Index i corresponds to ratio i/63 in [0, 1) → angle in [0, π/4).
static const uint8_t PROGMEM atan2_table[64] = {
    0,5,10,15,20,26,31,36,41,46,51,56,61,66,70,75,
    80,85,89,94,98,103,107,111,115,120,124,128,131,135,139,143,
    148,152,155,159,162,166,169,173,176,179,183,186,189,192,195,198,
    201,204,207,210,212,215,218,220,223,225,228,230,233,235,237,239
};

// Integer atan2 function using lookup table
// Returns angle in range [0, 2048) representing [0, 2π)
// Matches original: atan2Fast(y,x) + PI
// atan2 returns [-π, π], adding π shifts to [0, 2π]
static int16_t atan2Int(int16_t y, int16_t x) {
    if (x == 0 && y == 0) {
        return 1024; // Return π (middle of range, was NAN + PI)
    }
    
    // Get absolute values
    int16_t absX = (x < 0) ? -x : x;
    int16_t absY = (y < 0) ? -y : y;
    
    // Compute angle within first octant (0 to π/4 = 0 to 256 units)
    uint8_t index;
    int16_t octantAngle;
    
    if (absX >= absY) {
        // |x| >= |y|: angle is atan(|y|/|x|), range [0, π/4]
        if (absX == 0) {
            octantAngle = 256; // π/4
        } else {
            index = (uint8_t)(((uint32_t)absY * 63) / absX);
            if (index > 63) index = 63;
            octantAngle = pgm_read_byte(&atan2_table[index]);
        }
    } else {
        // |y| > |x|: angle is π/2 - atan(|x|/|y|), range (π/4, π/2]
        index = (uint8_t)(((uint32_t)absX * 63) / absY);
        if (index > 63) index = 63;
        // 512 = π/2 in our units
        octantAngle = 512 - pgm_read_byte(&atan2_table[index]);
    }
    
    // Standard atan2 quadrant mapping (returns angle from +X axis, CCW positive)
    // Result is in [-π, π] mapped to [-1024, 1024]
    int16_t rawAngle;
    if (x >= 0) {
        if (y >= 0) {
            // Quadrant I: atan2 returns [0, π/2]
            rawAngle = octantAngle;
        } else {
            // Quadrant IV: atan2 returns [-π/2, 0]
            rawAngle = -octantAngle;
        }
    } else {
        if (y >= 0) {
            // Quadrant II: atan2 returns [π/2, π]
            rawAngle = 1024 - octantAngle;
        } else {
            // Quadrant III: atan2 returns [-π, -π/2]
            rawAngle = -1024 + octantAngle;
        }
    }
    
    // Add π (1024 units) to shift from [-π, π] to [0, 2π]
    // Then mask to keep in [0, 2048)
    return (rawAngle + 1024) & 2047;
}

volatile int potClass::valeurTapA{0};
volatile int potClass::valeurTapB{0};
volatile int potClass::valeurTap[2]{0, 0};
volatile bool potClass::_movingFlag = 0;
volatile int potClass::_lastReadingA = 0;
volatile int potClass::_lastReadingB = 0;
// int potClass::movement[8] = {0, 0, 0, 0, 0, 0, 0, 0};
// volatile int potClass::movement = {0};
// byte potClass::movementIndex = 0;
volatile bool potClass::_selectorFlag = 0;

int16_t potClass::angle = 0;
int16_t potClass::oldAngle = 0;
volatile uint32_t potClass::_lastMoveTime = 0;

void potClass::init() 
{
    PORTF_PIN3CTRL = 0x04;              // disable digital input buffer on analog pin
    PORTF_PIN2CTRL = 0x04;              // 

    ADC0_CTRLA = ADC_RESSEL_10BIT_gc;   // 10-bit resolution
    ADC0_CTRLC = 0b01010101;                  // reduced capacitance, Vdd ref, prescaler of 16 (0x40 | 0x10 | 0x03)
    ADC0_MUXPOS = 12;                 // (AIN12)
    // ADC0_CTRLA |= ADC_FREERUN_bm;       // start freerun mode
    ADC0_CTRLA |= ADC_ENABLE_bm;        // turn ADC on
    ADC0.CTRLB = ADC_SAMPNUM_ACC64_gc;  // 64 samples accumulated
    
    ADC0.INTCTRL |= ADC_RESRDY_bm;      // enable result ready interrupt
    
    ADC0_COMMAND |= ADC_STCONV_bm;      // start a conversion


}
void potClass::handleISR()
{
    uint16_t adcVal = ADC0_RES;           // get ADC value
    adcVal = adcVal >> ADC_SHIFT_DIV64;   // divide by 64 for averaging using sample accumulation
//   Serial.println(adcVal);
    valeurTap[_selectorFlag] = adcVal - 511; // store value
    
    // Only evaluate movement after B is updated (full pair acquired)
    if (_selectorFlag == B) 
    {
        int movementA = _lastReadingA - valeurTap[A];
        int movementB = _lastReadingB - valeurTap[B];
        bool overStart = abs(movementA) > THRESHOLD_START || abs(movementB) > THRESHOLD_START;
        bool overContinue = abs(movementA) > THRESHOLD_CONTINUE || abs(movementB) > THRESHOLD_CONTINUE;
        uint32_t now = millis();

        if (!_movingFlag)
        {
            if (overStart)
            {
                _movingFlag = true;
                _lastMoveTime = now;
                _lastReadingA = valeurTap[A];
                _lastReadingB = valeurTap[B];
            }
        }
        else
        {
            if (overContinue)
            {
                _lastMoveTime = now;
                _lastReadingA = valeurTap[A];
                _lastReadingB = valeurTap[B];
            }
            if (now - _lastMoveTime > MOVE_GRACE_MS)
            {
                _movingFlag = false;
            }
        }
    }

    _selectorFlag = !_selectorFlag;
    ADC0_MUXPOS = 12 + _selectorFlag;   // switch ADC input

    ADC0_COMMAND |= ADC_STCONV_bm;      // start a new 
}

ISR(ADC0_RESRDY_vect)
{
    potClass::handleISR();
}

void potClass::updateReading()
{
    // Now handled by ISR
}

bool potClass::readState(bool slew)
{
    if (slew)
    {
        uint32_t lastMove;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            lastMove = _lastMoveTime;
        }
        return ((millis() - lastMove) < MOVE_GRACE_MS);
    }
    return _movingFlag;
}
int potClass::read(int value, int turn, int minValue, int maxValue)
{
    if(!readState(true)) return value;
    int outoffset = 0;
    if(minValue < 0) // check if minValue is negative and add an offset to have only positive values
    { 
        outoffset = abs(minValue);  
        minValue += outoffset;
        maxValue += outoffset;
        value += outoffset;
    }
    int maxValueBuffer = maxValue; // to have same speed for pot selection
    if (maxValue < 15) { maxValue = 15; }
    
    // Fixed-point conversion: angle is 0-2047 representing [0, 2π]
    // Original: scaleFactor = ((maxValue / 2.0) / PI) / turn = maxValue / (2 * PI * turn)
    //           scaledAngle = angle * scaleFactor
    // Since angle [0, 2π] maps to [0, 2048]:
    //   scaledAngle = (angle / 2048) * 2π * scaleFactor = angle * maxValue / (2048 * turn)
    oldAngle = angle;
    
    int localTapA, localTapB;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        localTapA = valeurTap[A];
        localTapB = valeurTap[B];
    }
    
    // atan2Int returns 0-2047 representing [0, 2π], matching original (atan2Fast + PI)
    angle = atan2Int(localTapA, localTapB);

    // turn is fixed-point with 1024 == 1.0 (number of turns)
    // Original float formula:
    //   scaledAngle = angle * maxValue / (2048 * turn)
    // With turn_fp = turn * 1024, this becomes:
    //   scaledAngle = angle * maxValue / (2 * turn_fp)
    if (turn <= 0) turn = 1;

    const int32_t denom = 2 * (int32_t)turn;
    int32_t scaledAngle     = ((int32_t)angle    * maxValue) / denom;
    int32_t oldScaledAngle  = ((int32_t)oldAngle * maxValue) / denom;

    // threshold = maxValue / (2 * turn) -> maxValue * 512 / turn_fp
    int32_t threshold = ((int32_t)maxValue * 512) / (int32_t)turn;
    // D2=B: offset = maxValue / turn ≈ 2 * threshold
    int32_t offset = threshold * 2;

    int32_t delta = scaledAngle - oldScaledAngle;
    
    // Handle wraparound
    if (delta < -threshold) { 
        value += delta + offset; 
    } else if (delta > threshold) { 
        value += delta - offset; 
    } else {
        value += delta;
    }

    if (value > maxValueBuffer) { value = maxValueBuffer; }
    else if (value < minValue) {value = minValue; }
    return value - outoffset;
}
//bool potClass::readStateOff() 
//{
//    _oldState = _state;
//    _state = readState();
//
//    return !_state & _oldState;
//}