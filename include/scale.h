// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>

#ifndef SCALE_H
#define SCALE_H

class scaleClass
{
    public:
        //static void setAll(unsigned int value) {_noteList = value; }
        static void setOne(byte group, byte value) { _activeNote[group][value] = !_activeNote[group][value]; }
        static bool getOne(byte group, byte value) { return _activeNote[group][value]; }
        static bool getAll(byte value);
        //static unsigned int getList();
        static byte quantize(byte input);
        static void setStatus(byte group) {_active[group] = !_active[group]; }
        static bool getStatus(byte group) {return _active[group]; }
        static bool getStatusAll() { return _active[0] | _active[1] | _active[2] | _active[3]; }
        static byte getNoteCount();
        static byte getNoteNumber(byte input);
    private:
        //static bool _isValid(byte group, byte value) { return _activeNote[group][value] ;}
        static bool _activeNote[4][12];
        static bool _active[4];
};
#endif