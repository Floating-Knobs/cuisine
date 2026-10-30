// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <structDefinition.h>
#ifndef EUCLIDEAN_H
#define EUCLIDEAN_H

#define EUCLIDEAN_MAX_LENGTH 16
#define EUCLIDIAN_LIMIT 64
#define BEAT_PAGE 0
#define LENGHT_PAGE 1
#define OFFSET_PAGE 2
class euclideanClass
{
    public:
        byte lenght = 16;
        byte beats = 0;
        byte offset = 0;
        byte page;
        unsigned int pattern[4] = {0, 0, 0, 0};
        bool isOn() {return lenght > 0 && beats > 0;}
        bool getStep(indexStruct index) {
            // Same rule as getPattern: clamp before subscripting, and never
            // divide by a length of zero. Callers currently guard with isOn(),
            // but the function should not depend on that.
            if (index.page >= 4 || lenght == 0) { return false; }
            return bitRead(pattern[index.page], (index.step % lenght));
        }
        unsigned int getPattern(byte page = 0) {return page < 4 ? pattern[page] : 0;}
        void update()
        {
            if (lenght < 1) { lenght = 1; }
            if (beats > lenght) { lenght = beats; }

            memset(pattern, 0, sizeof(pattern));
            unsigned int patternBuffer = 0;

            // Bresenham / bucket distribution
            byte bucket = 0;
            for (byte i = 0; i < lenght; i++)
            {
                bucket += beats;
                if (bucket >= lenght)
                {
                    bucket -= lenght;
                    bitWrite(patternBuffer, i, true);
                }
            }

            // Align first hit to 0 and bake user offset in one rotate.
            // dest (j+offset) reads source (j+s) so first hit at s lands on offset (= green).
            byte rot = offset % lenght;
            byte s = 0;
            if (beats > 0 && !bitRead(patternBuffer, 0))
            {
                while (s < lenght && !bitRead(patternBuffer, s)) { s++; }
            }
            unsigned int rotated = 0;
            for (byte j = 0; j < lenght; j++)
            {
                bitWrite(rotated, (j + rot) % lenght, bitRead(patternBuffer, (j + s) % lenght));
            }

            for (unsigned int i = 0; i < EUCLIDIAN_LIMIT; i += lenght)
            {
                pattern[i >> 4] |= rotated << (i & 0x0F);
            }
        }
    };
#endif
