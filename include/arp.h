// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <track.h>
#include <structDefinition.h>
#ifndef ARP_H
#define ARP_H

#define ARP_MAX_NOTES 6

// extern noteStruct;
class arpClass {
  public:
    struct __attribute__((packed)) noteClasse
    {
      noteStruct note;
      indexStruct index;
    }notes[ARP_MAX_NOTES];
    byte count = 0;
    byte indexArp;

    arpClass() : count(0), indexArp(0) {}

    static noteStruct packValue(uint8_t value) {
      noteStruct n{};
      n.note = value & 0x0F;
      n.octave = (int8_t)((value >> 4) & 0x0F);
      n.accent = 0;
      n.velocity = 0;
      return n;
    }

    static uint8_t unpackValue(noteStruct n) {
      return (uint8_t)(n.note & 0x0F)
           | (uint8_t)(((uint8_t)n.octave & 0x0F) << 4);
    }

    void addNote(noteStruct note, indexStruct index) {
      for (byte i = 0; i < count; i++) {
        if (notes[i].note.note == note.note && notes[i].note.octave == note.octave) {
          return; // note already in the list
        }
      }
      if (count < ARP_MAX_NOTES) {
        count++;
        notes[count - 1].note = note;
        notes[count - 1].index = index;
      }
    }
    void removeNote(noteStruct note) {
      for (byte i = 0; i < count; i++) {
        if (notes[i].note.note == note.note && notes[i].note.octave == note.octave) {
          for (byte j = i+1; j < count; j++) {
            notes[j-1] = notes[j];
          }
          count--;
          if (indexArp >= count) {
            indexArp = 0;
          }
          return;  // Note found and removed, exit immediately
        }
      }
    }

    indexStruct getIndex(noteStruct note) {
      for (byte i = 0; i <= count; i++) {
        
        if (notes[i].note.note == note.note && notes[i].note.octave == note.octave ) {
          
          return notes[i].index;
        }
      }
      indexStruct out = {255, 255};
      return out;
    }
    noteStruct getCurrentNote() {
      if (count == 0) {
        noteStruct emptyNote = {0, 0, false};
        return emptyNote; // no notes in the list
      }
      return notes[indexArp].note; // peek current, don't advance
    }
    noteStruct getNextNote() {
      if (count == 0) {
        noteStruct emptyNote = {0, 0, false};
        return emptyNote; // no notes in the list
      }
      noteStruct note = notes[indexArp++].note;
      if (indexArp >= count) {
        indexArp = 0;
      }
      return note;
    }
    byte getCurrentValue() {
      if (count == 0) {
        return 0; // no notes in the list
      }
      return unpackValue(notes[indexArp].note); // peek current, don't advance
    }
    byte getNextValue() {
      if (count == 0) {
        return 0; // no notes in the list
      }
      noteStruct note = notes[indexArp++].note;
      if (indexArp >= count) {
        indexArp = 0;
      }
      return unpackValue(note);
    }

    byte getNumKeysPressed() {
      return count;
    }

    void reset() {
      count = 0;
      indexArp = 0;
    }
};
#endif
