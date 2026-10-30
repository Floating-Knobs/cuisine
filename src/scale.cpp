// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <scale.h>

static bool scaleClass::_active[4] = {0};
static bool scaleClass::_activeNote[4][12] = {0};

static bool scaleClass::getAll(byte value)
{
  bool output = 0;
  for (int i = 0; i < 4; i ++) { output |= (_activeNote[i][value] & _active[i]); }
  return output;
}
static byte scaleClass::getNoteCount()
{
    byte out = 0;
    for (int i = 0; i < 12; i++)
    {
        if (getAll(i))
        {
            out++;
        }
    }
    return out;
}
static byte scaleClass::getNoteNumber(byte input)
{
    int out = -1;
    for (int i = 0; i < 12; i++)
    {
        if (getAll(i))
        {
            out++;
        }
        if (out == input) { return i; }
    }
    
}
static byte scaleClass::quantize(byte input)
{
    byte note = (input + 1) % 12;
    for (int i = 0; i < 12; i++)
    {
        if (getAll(note + i)) { return input + i; }
        else if (getAll(note - i)) { return input - i; }
    }
}
