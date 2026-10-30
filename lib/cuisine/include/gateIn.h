// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef _GATEIN_H_INCLUDED
#define _GATEIN_H_INCLUDED

#include <board.h>

class gateInClass
{
    public :
        static void init(byte pin);
        static bool read(byte input);
        static bool readTrig(byte input);
        static void update();
        unsigned int stateRegister;
        bool updateFlag;
    private :
        static void _interrupt();
        static bool _gate[4];
        static bool _oldGate[4];
        static byte _oldGateInRegister;
        static byte _gateInRegister;
        byte _pin;
        
        

};

namespace cuisine {
namespace gate_in {

/// 0..3, the four jack inputs.
inline bool high(uint8_t input)   { return gateInClass::read(input); }

/// True if the input rose since the previous poll.
inline bool rising(uint8_t input) { return gateInClass::readTrig(input); }

} // namespace gate_in
} // namespace cuisine

#endif