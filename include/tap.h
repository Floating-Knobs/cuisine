// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef TAP_H
#define TAP_H

#include <Arduino.h>

#define MAX_TAP_BUFFER 32

class tap
{
    public:
        static void start();
        static void addTap();
        static void reset();
        static unsigned int getBPM();
        static int getLastTapsNumber();
        static bool isReset();
        static void setTapPageFlag(bool value);

    private:
        static volatile unsigned long _lastTapTime;
        static volatile unsigned int _tapIntervals[MAX_TAP_BUFFER];
        static volatile byte _tapIndex;
        static volatile byte _tapCount;
        static volatile bool _resetFlag;
        static volatile bool _tapPageFlag;
};

#endif

