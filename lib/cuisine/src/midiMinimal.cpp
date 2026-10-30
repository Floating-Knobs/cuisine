// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <midiMinimal.h>

static MidiNoteHandler noteOnHandler = nullptr;
static MidiNoteHandler noteOffHandler = nullptr;
static MidiCcHandler ccHandler = nullptr;
static MidiSimpleHandler clockHandler = nullptr;
static MidiSimpleHandler startHandler = nullptr;
static MidiSimpleHandler stopHandler = nullptr;

static byte runningStatus = 0;
static byte data1 = 0;
static byte needed = 0;
static byte inputChannelFilter = 1;

void midiInit(byte inputChannel)
{
  // 0 enables omni (accept all channels).
  if (inputChannel > 16) {
    inputChannelFilter = 1;
  } else {
    inputChannelFilter = inputChannel;
  }

  runningStatus = 0;
  data1 = 0;
  needed = 0;

  Serial1.begin(31250);
}

void midiSetNoteOnHandler(MidiNoteHandler handler) { noteOnHandler = handler; }
void midiSetNoteOffHandler(MidiNoteHandler handler) { noteOffHandler = handler; }
void midiSetCcHandler(MidiCcHandler handler) { ccHandler = handler; }
void midiSetClockHandler(MidiSimpleHandler handler) { clockHandler = handler; }
void midiSetStartHandler(MidiSimpleHandler handler) { startHandler = handler; }
void midiSetStopHandler(MidiSimpleHandler handler) { stopHandler = handler; }

static void handleMessage(byte status, byte d1, byte d2)
{
  byte channel = (status & 0x0F) + 1;
  if (inputChannelFilter && channel != inputChannelFilter) {
    return;
  }

  switch (status & 0xF0) {
    case 0x80:
      if (noteOffHandler) { noteOffHandler(channel, d1, d2); }
      break;
    case 0x90:
      if (d2 == 0) {
        if (noteOffHandler) { noteOffHandler(channel, d1, d2); }
      } else {
        if (noteOnHandler) { noteOnHandler(channel, d1, d2); }
      }
      break;
    case 0xB0:
      if (ccHandler) { ccHandler(channel, d1, d2); }
      break;
    default:
      break;
  }
}

void midiPoll()
{
  while (Serial1.available()) {
    byte b = Serial1.read();

    if (b >= 0xF8) {
      if (b == 0xF8) { if (clockHandler) { clockHandler(); } }
      else if (b == 0xFA) { if (startHandler) { startHandler(); } }
      else if (b == 0xFC) { if (stopHandler) { stopHandler(); } }
      continue;
    }

    if (b & 0x80) {
      byte type = b & 0xF0;
      if (type == 0x80 || type == 0x90 || type == 0xB0) {
        runningStatus = b;
        needed = 2;
        data1 = 0;
      } else {
        runningStatus = 0;
        needed = 0;
      }
      continue;
    }

    if (runningStatus == 0 || needed == 0) {
      continue;
    }

    if (needed == 2) {
      data1 = b;
      needed = 1;
    } else {
      handleMessage(runningStatus, data1, b);
      needed = 2;
    }
  }
}

static inline void sendStatus(byte status, byte d1, byte d2)
{
  Serial1.write(status);
  Serial1.write(d1);
  Serial1.write(d2);
}

void midiSendNoteOn(byte note, byte velocity, byte channel)
{
  if (channel < 1 || channel > 16) { return; }
  sendStatus(0x90 | ((channel - 1) & 0x0F), note, velocity);
}

void midiSendNoteOff(byte note, byte velocity, byte channel)
{
  if (channel < 1 || channel > 16) { return; }
  sendStatus(0x80 | ((channel - 1) & 0x0F), note, velocity);
}

void midiSendControlChange(byte cc, byte value, byte channel)
{
  if (channel < 1 || channel > 16) { return; }
  sendStatus(0xB0 | ((channel - 1) & 0x0F), cc, value);
}

void midiSendClock() { Serial1.write((byte)0xF8); }
void midiSendStart() { Serial1.write((byte)0xFA); }
void midiSendStop() { Serial1.write((byte)0xFC); }
