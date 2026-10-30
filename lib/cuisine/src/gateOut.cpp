// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <gateOut.h>

byte gateOutClass::_gateOutRegister = {0};
byte gateOutClass::_oldGateOutRegister = {0};

void gateOutClass::init()
{
    _gateOutRegister = 0;
    pinMode(SSRegisterPin, OUTPUT);
    digitalWriteFast(SSRegisterPin, HIGH);
    // while(true);
    _send();
}

void gateOutClass::write(int number, bool value)
{
    if(number == 255) { return; }
    else if(number == 4) { number = 5; }
    else if(number == 5) { number = 7; }
    else if(number == 7) { number = 4; }
    bitWrite(_gateOutRegister, number, value);
    
    // _send();
    
}

byte gateOutClass::read()
{
    return _gateOutRegister;
}

bool gateOutClass::readOne(byte number)
{
    return bitRead(_gateOutRegister, number);
}
void gateOutClass::_send()
{
    // Transactions, not SPI.begin(): that call resets the bus divider every
    // frame and silently changes the speed for whatever runs next.
    static SPISettings settings(F_CPU / 2, MSBFIRST, SPI_MODE0);

    digitalWriteFast(SSRegisterPin, LOW);
    SPI.beginTransaction(settings);
    SPI.transfer(_gateOutRegister);
    SPI.endTransaction();
    digitalWriteFast(SSRegisterPin, HIGH);
}
void gateOutClass::update()
{
    if(_oldGateOutRegister != _gateOutRegister)
    {
        _send();
        _oldGateOutRegister = _gateOutRegister;
    }
}