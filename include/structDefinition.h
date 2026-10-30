// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>

#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

#define BANK_NUMBER 16
#define PATTERN_NUMBER 12
#define TRACK_NUMBER 12
#define VARIATION_NUMBER 4
#define STEP_NUMBER (16 * VARIATION_NUMBER)

#define FRAM_SIZE 0x8000
#define FRAM_LAST_ADDRESS (FRAM_SIZE - 1)

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
struct limitStruct
{
    byte _first;
    byte _last;
    bool _firstLast = 0;
    byte _firstPage;
    byte _lastPage;
};
struct variationStruct
{
    unsigned int step;
    unsigned int accent;
    byte page;
};
struct __attribute__((packed)) patternStruct
{

    uint16_t step[VARIATION_NUMBER]; //
    uint16_t accent[VARIATION_NUMBER]; //
    byte duration[STEP_NUMBER]; //
    byte first = 0; //
    byte last = 15; //
    // unsigned int value[STEP_NUMBER]; //
    byte value[STEP_NUMBER]; //
    byte size = 0;
    byte timeDivision;
    byte preset;
    byte patternNumber;
    bool modifiedFlag;

    bool mute;
    byte gateLenght;

};
struct __attribute__((packed)) presetStruct
{
    int16_t MIDIchannel;
    byte gateOut;
    byte accentOut;
    int16_t CVout;
    // byte mode;
    byte type : 4;
    byte patternSource : 4;
    byte clockSource;
    byte clockInput;
    byte resetSource;
    byte resetInput;
    byte _playMode;
    byte CC;
    bool transpose;
};


struct trackStruct
{
    patternStruct pattern[PATTERN_NUMBER];
    presetStruct preset;
};
struct OperatorSettings {
    uint8_t gateSources[4]; // 0: AND, 1: OR, 2: XOR, 3: NAND
    uint8_t cvSources[4];   // 4: ADD, 5: SUB, 6: MULT, 7: MIX
};

struct bankPresetStruct
{
    // unsigned int scale[4];
    uint16_t scale[2];
    byte chord[4];
    int16_t bpm;
    uint16_t muteGroup[4] = {0,0,0,0};
    byte muteState;
    bool trackSelector[4][TRACK_NUMBER]; 
    byte chordProgressionTrack;
    
};
// struct bankStruct
// {
//     trackStruct track[TRACK_NUMBER];
//     bankPresetStruct preset;
// };
struct bankStruct
{
    // patternStruct pattern[PATTERN_NUMBER][TRACK_NUMBER];
    // presetStruct trackPreset[TRACK_NUMBER];
    trackStruct track[TRACK_NUMBER];
    bankPresetStruct bankPreset;
};
// struct memoryStruct
// {
//     bankStruct bank[BANK_NUMBER];
//     enum clockMode {internal, MIDI};
// }
struct __attribute__((packed)) noteStruct 
{
    uint8_t note : 4;          // Scale index 
    int8_t  octave : 4;        // Octave +- 4 
    uint8_t accent : 1;    // Accent flag
    uint8_t velocity : 7;  // MIDI velocity 0-127

    bool operator==(const noteStruct& other) const {
        return note == other.note &&
               octave == other.octave;
    }

    bool operator!=(const noteStruct& other) const {
        return !(*this == other);
    }
};
struct indexStruct
{
    byte step;
    byte page;
    byte chain;
};
struct overrideStruct
{
    byte first;
    byte last;
    indexStruct index = {0,0};
    // byte page;
    bool flag;
    bool loopWriteFlag = 0;
};

#define LAYOUT_OFFSET(type, member, offset) \
    static_assert(__builtin_offsetof(type, member) == (offset), "saved-data layout changed: " #type "::" #member)
static_assert(sizeof(noteStruct) == 2, "saved-data layout changed: noteStruct");
static_assert(sizeof(patternStruct) == 153, "saved-data layout changed: patternStruct");
static_assert(sizeof(presetStruct) == 14, "saved-data layout changed: presetStruct");
static_assert(sizeof(trackStruct) == 1850, "saved-data layout changed: trackStruct");
static_assert(sizeof(bankPresetStruct) == 68, "saved-data layout changed: bankPresetStruct");
static_assert(sizeof(bankStruct) == 22268, "saved-data layout changed: bankStruct");
static_assert(sizeof(OperatorSettings) == 8, "saved-data layout changed: OperatorSettings");
LAYOUT_OFFSET(patternStruct, step, 0);
LAYOUT_OFFSET(patternStruct, accent, 8);
LAYOUT_OFFSET(patternStruct, duration, 16);
LAYOUT_OFFSET(patternStruct, first, 80);
LAYOUT_OFFSET(patternStruct, last, 81);
LAYOUT_OFFSET(patternStruct, value, 82);
LAYOUT_OFFSET(patternStruct, size, 146);
LAYOUT_OFFSET(patternStruct, timeDivision, 147);
LAYOUT_OFFSET(patternStruct, patternNumber, 149);
LAYOUT_OFFSET(patternStruct, modifiedFlag, 150);
LAYOUT_OFFSET(presetStruct, MIDIchannel, 0);
LAYOUT_OFFSET(presetStruct, gateOut, 2);
LAYOUT_OFFSET(presetStruct, accentOut, 3);
LAYOUT_OFFSET(presetStruct, CVout, 4);
LAYOUT_OFFSET(presetStruct, clockSource, 7);
LAYOUT_OFFSET(presetStruct, transpose, 13);
LAYOUT_OFFSET(trackStruct, preset, 1836);
LAYOUT_OFFSET(bankStruct, bankPreset, 22200);
LAYOUT_OFFSET(bankPresetStruct, scale, 0);
LAYOUT_OFFSET(bankPresetStruct, chord, 4);
LAYOUT_OFFSET(bankPresetStruct, bpm, 8);
LAYOUT_OFFSET(bankPresetStruct, muteGroup, 10);
LAYOUT_OFFSET(bankPresetStruct, muteState, 18);
LAYOUT_OFFSET(bankPresetStruct, chordProgressionTrack, 67);
#undef LAYOUT_OFFSET

// FRAM memory mapping:

// pattern Buffer
unsigned int patternBufferAdress(int track);
#define PATTERN_BUFFER_SIZE (sizeof(patternStruct) * TRACK_NUMBER + sizeof(trackStruct))
#define TRACK_SIZE sizeof(trackStruct)
#define PATTERN_SIZE sizeof(patternStruct)

unsigned int patternAdress(int track, int pattern);
unsigned int presetAdress(int track);
unsigned int trackAdress(int track);
unsigned int bankPresetAdress();
unsigned int operatorAdress(int track);
unsigned int transposeMetaAdress();
unsigned int transposeAdress(int track);

// bool loadingFlag = false;
// byte target = 0;

#endif