// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

//digital Pin input definition
#ifndef _KEYBOARD_H_INCLUDED
#define _KEYBOARD_H_INCLUDED

#include <Arduino.h>
#include <board.h>
//#define multiplexerChannel PORTE_OUT


struct keyStruct
{
  unsigned int scan;
  unsigned int oldScan;
  unsigned int pendingRise;
  unsigned int pendingFall;
  unsigned int pendingHigh;
  unsigned int pendingLow;
  unsigned int rise;
  unsigned int fall;
  unsigned int high;
  unsigned int low;
};

class keyboardClass
{
    public:
        static void init();
        static void update();
        static void scanChannel(); // Called by ISR to scan one channel
        static unsigned int stepRead();
        static unsigned int stepReadTrig();
        static unsigned int stepReadTrigOff();
        static unsigned int commandRead(); // to do : class moman
        static bool commandReadNo(byte number);
        static unsigned int commandReadTrig();
        static bool stepReadOne(byte number);
        static bool stepReadOneTrig(byte number);
        static bool stepReadOneTrigOff(byte number);
        static bool commandReadOne(unsigned int number);
        static bool commandReadOneTrig(unsigned int number);
        static bool commandReadOneTrigOff(unsigned int number);
        static bool arm();
    private:
        static volatile byte _multiplexerChannel;  // volatile - modified in ISR
        static volatile keyStruct _step;           // volatile - modified in ISR
        static volatile keyStruct _command;        // volatile - modified in ISR
        static volatile bool _armSwitch;             // volatile - modified in ISR
};

// class macroClass
// {
//     public:
//         static void execute();
//         static void set(byte number, byte page, unsigned int steps);
//     private :
//         static struct macroStruct _macro[64];
//         static bool _macroFlag;
//         static bool _macroFlagOld;
//         static bool _macroMode; // 0 = release, 1 = press
//         static byte _index;
// }

namespace cuisine {
namespace keys {

// 16 step keys, bit n == key n.
inline uint16_t steps()          { return keyboardClass::stepRead(); }
inline uint16_t stepsPressed()   { return keyboardClass::stepReadTrig(); }
inline uint16_t stepsReleased()  { return keyboardClass::stepReadTrigOff(); }
inline bool     down(uint8_t i)  { return keyboardClass::stepReadOne(i); }
inline bool     pressed(uint8_t i)  { return keyboardClass::stepReadOneTrig(i); }
inline bool     released(uint8_t i) { return keyboardClass::stepReadOneTrigOff(i); }

// 14 command keys, mask-based (combinations are key | SHIFT; see the app).
inline uint16_t commands()         { return keyboardClass::commandRead(); }
inline uint16_t commandsPressed()  { return keyboardClass::commandReadTrig(); }
inline bool     command(uint16_t mask)         { return keyboardClass::commandReadOne(mask); }
inline bool     commandPressed(uint16_t mask)  { return keyboardClass::commandReadOneTrig(mask); }
inline bool     commandReleased(uint16_t mask) { return keyboardClass::commandReadOneTrigOff(mask); }

// Arm switch, level.
inline bool arm() { return keyboardClass::arm(); }

} // namespace keys
} // namespace cuisine

#endif