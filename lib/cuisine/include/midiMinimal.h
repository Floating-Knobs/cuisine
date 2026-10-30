// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#pragma once

#include <Arduino.h>

typedef void (*MidiNoteHandler)(byte channel, byte note, byte velocity);
typedef void (*MidiCcHandler)(byte channel, byte cc, byte value);
typedef void (*MidiSimpleHandler)();

void midiInit(byte inputChannel);
void midiPoll();

void midiSetNoteOnHandler(MidiNoteHandler handler);
void midiSetNoteOffHandler(MidiNoteHandler handler);
void midiSetCcHandler(MidiCcHandler handler);
void midiSetClockHandler(MidiSimpleHandler handler);
void midiSetStartHandler(MidiSimpleHandler handler);
void midiSetStopHandler(MidiSimpleHandler handler);

void midiSendNoteOn(byte note, byte velocity, byte channel);
void midiSendNoteOff(byte note, byte velocity, byte channel);
void midiSendControlChange(byte cc, byte value, byte channel);
void midiSendClock();
void midiSendStart();
void midiSendStop();

namespace cuisine {
namespace midi {

inline void init(uint8_t channel) { midiInit(channel); }
inline void poll()                 { midiPoll(); }

inline void onNoteOn(MidiNoteHandler handler)          { midiSetNoteOnHandler(handler); }
inline void onNoteOff(MidiNoteHandler handler)         { midiSetNoteOffHandler(handler); }
inline void onControlChange(MidiCcHandler handler)     { midiSetCcHandler(handler); }
inline void onClock(MidiSimpleHandler handler)         { midiSetClockHandler(handler); }
inline void onStart(MidiSimpleHandler handler)         { midiSetStartHandler(handler); }
inline void onStop(MidiSimpleHandler handler)          { midiSetStopHandler(handler); }

inline void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    midiSendNoteOn(note, velocity, channel);
}
inline void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    midiSendNoteOff(note, velocity, channel);
}
inline void sendControlChange(uint8_t cc, uint8_t value, uint8_t channel) {
    midiSendControlChange(cc, value, channel);
}
inline void sendClock() { midiSendClock(); }
inline void sendStart() { midiSendStart(); }
inline void sendStop()  { midiSendStop(); }

} // namespace midi
} // namespace cuisine
