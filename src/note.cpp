// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

// #include <Arduino.h>
// // #include <note.h>
// #include <track.h>

// struct noteStruct 
// {
//     byte note;
//     byte octave;
//     byte velocity;
// };
// void writeNote(byte step, byte note, byte octave, trackClass::patternStruct &pattern)
// {
//   note &= 0b00000111;
//   octave &= 0b00000111;
//   octave <<= 3;
//   note |= octave;
//   // erase note and octave
//   pattern.value[step] &= ~0b00111111;
//   pattern.value[step] |= note;
// }
// struct noteStruct readNote(byte step, bool scale = false, trackClass::patternStruct &pattern )
// {
//   noteStruct out = {0,0};
//   out.note = pattern.value[step] & 0b00000111;
//   if (!scale) { out.note = getNoteFromScaleValue(out.note); }

//   out.octave = pattern.value[step] >> 3;
//   out.octave &= 0b00000111;
//   return out;
// }
// // check if note is in scale
// bool isNoteInScale(unsigned int myScale, int note) {
//   return (myScale & (1 << (note % 12))) != 0;
// }
// int getNoteFromScaleValue(byte value, unsigned int myScale) {
//   if (myScale == 0) { return -1;}
//   Serial.println(value);
//   //find the selected note corresponding to the value
//   int noteIndex = -1;
//   for (int i = 0; i < 12; i++) {
//     if (bitRead(myScale, i)) {
//       noteIndex++;
//       if (noteIndex == value ) {
//         return noteIndex;
//       }
//     }
//   }
// }
// bool setScale(byte note, &unsigned int myScale) {
//   if (countSetBits(myScale) < 8)
//   {
//     bitWrite(myScale, note, !bitRead(myScale, note));
//     return false;
//   }
//   else if (bitRead(myScale, note))
//   {
//     bitWrite(myScale, note, !bitRead(myScale, note));
//   }
//   else { return true; }
  
// }
// {
//   if (getNoteScaleCount() < 8)
//   {
//     bitWrite(_scale[_currentScale], note, !bitRead(_scale[_currentScale], note));
//     return false;
//   }
//   else if (bitRead(_scale[_currentScale], note))
//   {
//     bitWrite(_scale[_currentScale], note, !bitRead(_scale[_currentScale], note));
//   }
//   else { return true; }
  
// }






// static byte trackClass::getNoteScaleCount() {
//   return countSetBits(_scale[_currentScale]);
// }
// byte countSetBits(unsigned int num) {
//   byte count = 0;
//   while (num != 0) {
//     count += num & 1;
//     num >>= 1;
//   }
//   return count;
// }
