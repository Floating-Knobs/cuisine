// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>


#ifndef TIMER
#define TIMER


class timerClass
{
    public:
        static void setBPM(unsigned int BPM);
        static void init();
        static unsigned int getBPM() {return _BPM; }
        static bool tick();
        static void start() { TCA0_SINGLE_CTRLA |= TCA_SINGLE_ENABLE_bm; }
        static void stop() { TCA0_SINGLE_CTRLA &= ~TCA_SINGLE_ENABLE_bm; }
        static void reset() { TCA0_SINGLE_CTRLESET &= TCA_SINGLE_CMD_RESET_gc; } //to test
        static unsigned long getPeriodBuffer() { return _periodBuffer; }
        static void setTick(bool value = true) { _tick = value; }
    private:
        static unsigned long _BPM;
        static byte _clockPrescaler; 
        static unsigned long _periodBuffer;
        static bool _tick;
};
#endif