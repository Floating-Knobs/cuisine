// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <structDefinition.h>

// struct bankStruct
// {
//     trackStruct track[TRACK_NUMBER];
//     unsigned int scale[4];
//     int BPM;
//     unsigned int muteGroup[4];
//     unsigned int trackSelector[4];
// };

// FRAM memory mapping:

// pattern Buffer
unsigned int patternBufferAdress(int track)
{
    return track * sizeof(patternStruct);
}
// +1 for the copy buffer
// #define PATTERN_BUFFER_SIZE sizeof(patternStruct) * (TRACK_NUMBER + 1)
// #define TRACK_SIZE sizeof(trackStruct)

unsigned int patternAdress(int track, int patternNumber)
{
    return PATTERN_BUFFER_SIZE + offsetof(trackStruct, pattern) + patternNumber * PATTERN_SIZE + track * TRACK_SIZE;
}
unsigned int presetAdress(int track)
{
    return PATTERN_BUFFER_SIZE + offsetof(trackStruct, preset) + track * TRACK_SIZE;
}
// unsigned int trackAdress(int track)
// {
//     return PATTERN_BUFFER_SIZE + track * TRACK_SIZE;
// }
unsigned int bankPresetAdress()
{
    return PATTERN_BUFFER_SIZE + offsetof(bankStruct, bankPreset);
}
unsigned int operatorAdress(int track)
{
    return PATTERN_BUFFER_SIZE + sizeof(bankStruct) + track * sizeof(OperatorSettings);
}
unsigned int transposeMetaAdress()
{
    return operatorAdress(TRACK_NUMBER);
}
unsigned int transposeAdress(int track)
{
    return transposeMetaAdress() + 1 + track * sizeof(noteStruct);
}
unsigned int trackAdress(int track)
{
    return PATTERN_BUFFER_SIZE + offsetof(bankStruct, track) + track * TRACK_SIZE;
}
