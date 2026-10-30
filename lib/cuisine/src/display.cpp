// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
// #include <LED.h>
// #include <cstdint>
#include <display.h>
// #include <track.h>

// Private names for the 2-bit colour mask, used only inside this renderer.
namespace {
constexpr uint8_t OFF    = 0;
constexpr uint8_t RED    = 1;
constexpr uint8_t GREEN  = 2;
constexpr uint8_t YELLOW = 3;

// Was a kDimTime macro that collided with the LED driver's own kDimTime.
constexpr uint16_t kDimTime = 500;
}

const byte displayClass::_octaveColor[5] PROGMEM = {OFF, GREEN, YELLOW, RED, RED};
static cuisine::Millis displayClass::_sinceBlink = 0;
static bool displayClass::_blink = 0;
static cuisine::Millis displayClass::_sinceDim = 0;
static bool displayClass::_dim = 0;
//void displayClass::init() { _LED.init(); }
void displayClass::value(int value, byte color, bool dim = false)
{

    unsigned int display = 0;
    
    for (byte i = value; i > 0; i--) 
    { 
        bitWrite(display, i, true); 
    }
    display++;
    LEDclass::setAll( display, color, dim);
}
void displayClass::offset(int value, bool dim = false)
{

    unsigned int display = 0;
    
    if(value < 0) 
    { 
        for (int i = 6 + value; i >= 0 ; i--) 
        { 
            bitWrite(display, i, true); 
        }
        display = ~display & 0xFF;
        LEDclass::setAll( display, YELLOW, dim);
    }
    else if(value > 0) 
    {
        for (int i = 0; i <= value; i++) 
        { 
            bitWrite(display, i, true); 
        }
        display <<= 8;
        LEDclass::setAll( display, GREEN, dim);
    }
    LEDclass::setSome(0b0000000110000000, RED, dim);
   
}
void displayClass::octaveNumber(int8_t value)
{
    int8_t octave = value;
    byte index = abs(octave);
    if (index > 4) index = 4; // Safety bounds check

    // Look up the color ONCE
    byte color = pgm_read_byte(&_octaveColor[index]);

    // Handle blinking if needed
    if (index == 4 && !LEDclass::blink()) {
        color = OFF;
    }

    // Use the color directly
    if (octave < 0) {  LEDclass::setOne(12, color); }
    else {  LEDclass::setOne(13, color); }
}
void displayClass::digit(unsigned int value)
{
    byte digit[4];

    digit[0] = (value / 1000) % 10;
    digit[1] = (value / 100) % 10;
    digit[2] = (value / 10) % 10;
    digit[3] = value % 10;

    for (byte i = 0; i < 4; i++) { digitOne(digit[i], i); }
}
void displayClass::digitOne(byte value, byte offset)
{
    for (byte i = 0; i < value; i++)
    {
        //byte adress = (i << 2);
        if (i < 4) { LEDclass::setOne( ((i << 2) + offset), GREEN ); }
        else if (i < 8) {  LEDclass::setOne( ( ((i - 4) << 2) + offset), YELLOW ); }
        else {  LEDclass::setOne( (((i - 8) << 2) + offset), RED ); }
    }
}
// bool displayClass::blink()
// {
//     if (_sinceBlink > BLINK_TIME)
//     {
//         _blink = !_blink;
//         _sinceBlink = 0;
//     }
//     return _blink;
// }
bool displayClass::dim()
{
    if (_sinceDim > kDimTime)
    {
        _dim = !_dim;
        _sinceDim = 0;
    }
    return _dim;
}