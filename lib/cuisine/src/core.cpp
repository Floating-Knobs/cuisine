// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>

#include <cuisine.h>

namespace cuisine {

void begin() {
    LEDclass::init();
    keyboardClass::init();
    DACclass::init();
    gateOutClass::init();
    potClass::init();
    gateInClass::init(GATE_A_INPUT);
    clockClass::init();

    // One full matrix pass; the scan ISR needs this before edge detection is
    // meaningful (~2.4 ms at 150 us per slot).
    delayMicroseconds(2500);
    keyboardClass::update();
}

void poll() {
    keyboardClass::update();     // latch key edges for this iteration
    midiPoll();                  // drain MIDI input
    clockClass::masterUpdate();  // advance the master timebase
    gateOutClass::update();      // flush gates if they changed
    DACclass::update();          // flush CV if it changed
}

} // namespace cuisine
