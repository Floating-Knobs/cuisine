// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <timer.h>

// Ticks per quarter. This is the real grid: the period below is computed for
// exactly this rate, so division byte 5 (12 ticks) is a 1/16.
#define PPQN 48
// Parenthesised: used inside a product, and the unparenthesised version
// overflowed 32 bits before the division.
#define F_CPU_PERIOD (F_CPU / 1000000UL)
byte timerClass::_clockPrescaler = 0;
unsigned long timerClass::_periodBuffer = 0;
unsigned long timerClass::_BPM = 1200;
bool timerClass::_tick = false;

void timerClass::init()
{
    TCA0_SINGLE_CTRLD = 0;
    TCA0_SINGLE_CTRLB = 0;
    TCA0_SINGLE_PER = 0xFFFF;

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm; // enable overflow interrupt
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc; // normal mode
    TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm); // disable event input
}
void timerClass::setBPM(unsigned int BPM)
{
    _BPM = BPM;

    // Period in CPU ticks: 60 s / (BPM * ticks per quarter), in tenths of BPM.
    const unsigned long period =
        (600000000UL / ((unsigned long)_BPM * PPQN)) * F_CPU_PERIOD;

    // Pick the fastest prescaler whose count fits in 16 bits, and use the same
    // index for CTRLA. Dividing by one value and selecting another is how the
    // tick rate used to end up at half the intended one.
    //
    // No lookup table: a local `static const` array lives in flash on AVR and
    // reads as garbage through a data pointer, which is exactly how this went
    // wrong the first time.
    uint8_t prescaler = 7;
    uint16_t buffer = 0xFFFF;
    for (uint8_t p = 1; p <= 7; p++) {
        const uint32_t divider = (p < 5) ? (1u << p)
                               : (p == 5) ? 64u
                               : (p == 6) ? 256u
                                          : 1024u;
        const uint32_t counts = period / divider;
        prescaler = p;
        if (counts <= 0xFFFF) {
            buffer = (uint16_t)counts;
            break;
        }
        buffer = 0xFFFF;  // slower than this prescaler can express
    }

    _periodBuffer = buffer;
    _clockPrescaler = prescaler;

    stop();

    TCA0.SINGLE.CTRLA = _clockPrescaler << TCA_SINGLE_CLKSEL_gp;
    TCA0.SINGLE.PERBUF = _periodBuffer; // set period
    start();
}
bool timerClass::tick()
{
    if (_tick)
    {
        _tick = false;
        return true;
    }
    else { return false; }
    // Serial.println(TCA0_SINGLE_CNT, BIN);

    // if (TCA0_SINGLE_CNT >= _periodBuffer)
    // {
    //     // Serial.println("tick");
    //     TCA0_SINGLE_CNT -= _periodBuffer;
    //     // TCA0.SINGLE.CNT
    //     return true;
    // }
    // else { return false; }
}
// ISR(TCA0_OVF_vect)
// {
//     timerClass::setTick();
//     TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
//     // Serial.println("overflow");
// }
