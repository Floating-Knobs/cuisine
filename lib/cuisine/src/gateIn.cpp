// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <arduino.h>
#include <gateIn.h>

byte gateInClass::_gateInRegister {0};
byte gateInClass::_oldGateInRegister {0};
bool gateInClass::_gate[4] {0};
bool gateInClass::_oldGate[4] {0};

void gateInClass::_interrupt()
{
    // filter relevant bit from portD
    // Serial.println("interrupt");
    _gateInRegister = PORTD_IN >> 2;
    _gateInRegister = _gateInRegister & 0b00001111;
}
void gateInClass::init(byte pin)
{
    // set pins as input
    PORTD_DIR &= 0b11000011;
    //pinMode(GATE_A_INPUT, INPUT_PULLUP);
    // set interupt vector portD
    
    PORTD_PIN2CTRL = 0b10001001;
    PORTD_PIN3CTRL = 0b10001001;
    PORTD_PIN4CTRL = 0b10001001;
    PORTD_PIN5CTRL = 0b10001001;

    attachInterrupt(digitalPinToInterrupt(GATE_A_INPUT), _interrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(GATE_B_INPUT), _interrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(GATE_C_INPUT), _interrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(GATE_D_INPUT), _interrupt, CHANGE);
}
bool gateInClass::read(byte input)
{
    // Serial.println(_gateInRegister, BIN);
    return bitRead(_gateInRegister, input);
}
bool gateInClass::readTrig(byte input)
{
    
    
    //_oldGate[input] = bitRead(_oldGateInRegister, input);
    //if (_gateInRegister & !_oldGateInRegister;) { return TRUE; }
    bool out = _gate[input] & !_oldGate[input];
    
     // to do individual variable for each input
    return out;
}
void gateInClass::update()
{
    for (int i = 0; i < 4; i++)
    {
        // if ( _gate[i]) { Serial.println(i); }
        // Serial.println(PORTD_IN >> 2, BIN);
        _oldGate[i] = _gate[i];
        _gate[i] = bitRead( _gateInRegister, i);
    }
}
