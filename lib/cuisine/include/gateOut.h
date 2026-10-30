// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <arduino.h>
#include <SPI.h>
#include <board.h>

#ifndef _GATEOUT_H_INCLUDED
#define _GATEOUT_H_INCLUDED

// to do : implemet logic mixing of outputs, default to OR for now
class gateOutClass {
  public: 
    static void init();
    static void write(int number, bool value);
    static byte read();
    static bool readOne(byte number);
    static void update();

  private:
    static byte _gateOutRegister;
    static byte _oldGateOutRegister;
    static void _send();
};

namespace cuisine {
namespace gate_out {

/// 0..7 logical outputs; board.h maps them to the physical register.
inline void    set(uint8_t number, bool on) { gateOutClass::write(number, on); }
inline uint8_t mask()                       { return gateOutClass::read(); }
inline bool    get(uint8_t number)          { return gateOutClass::readOne(number); }

} // namespace gate_out
} // namespace cuisine

#endif