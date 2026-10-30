// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef INTERFACE_H
#define INTERFACE_H

#include <elapsed.h>
#include <track.h>
// #include <haptic.h>
// #include <memory.h>
#include <arp.h>
#include <tap.h>

#define SHIFT 1
#define TRACK 64
#define TRACK_PLAY TRACK + 1
#define NOTE_SELECT 256
#define SCALE NOTE_SELECT + 1
///////////////////////////to do class editor, transport, modificator
#define STEP_EDIT 128
#define STEP_MUTE STEP_EDIT + 1
//#define BANK
#define KEYBOARD_PAGE 256
#define EUCLID 512
#define ACCENT EUCLID + 1
#define PAGE_BYPASS_OVERRIDE_POT 1 << 15
#define PAGE_DONT_BYPASS_OVERRIDE_POT 1 << 14
// set last bit on indirect page for pot page management
#define LOW_VELOCITY 2 | PAGE_BYPASS_OVERRIDE_POT
#define HIGH_VELOCITY 4 | PAGE_BYPASS_OVERRIDE_POT
#define MODE_SET 16 | PAGE_DONT_BYPASS_OVERRIDE_POT
#define GATE_LENGHT 32 | PAGE_BYPASS_OVERRIDE_POT
#define CHORD 64 | PAGE_DONT_BYPASS_OVERRIDE_POT
#define BPM_PAGE 128 | PAGE_BYPASS_OVERRIDE_POT
#define MASTER_RESET 256 | PAGE_BYPASS_OVERRIDE_POT
#define TRACK_SETTINGS 512 | PAGE_BYPASS_OVERRIDE_POT
#define TRACK_SETTINGS_2 1024 | PAGE_BYPASS_OVERRIDE_POT
#define MASTER_SETTINGS 2048 | PAGE_BYPASS_OVERRIDE_POT


#define PATTERN 1024
#define BANK PATTERN + 1
// #define PATTERN_CHAIN PATTERN + 1
#define PATTERN_PAGE 2048 // <- double with PAGE
#define PLAY 8192
#define RESET PLAY + 1
#define REC 4096
#define PAGE 2048
//#define CLOCK_SHIFT
#define COPY 32
#define PASTE 16
#define CLEAR 8
#define DIV_MULT 4
#define CLOCK_SHIFT 5
#define LOOP 2
#define JUMP 3
#define COMMANDS_MASK (SHIFT | COPY | PASTE | CLEAR | PLAY | RESET | REC | PAGE)
#define DOUBLE_TAP_MASK (TRACK | PATTERN | STEP_EDIT | COPY | PASTE | CLEAR | EUCLID | SHIFT | PAGE)
#define POT_PAGE_MASK (TRACK | TRACK_PLAY | PATTERN | BANK | STEP_EDIT | STEP_MUTE | MODE_SET | NOTE_SELECT | SCALE)

#define POP_UP_TIME 125
#define SAVE_TIME 500

#define KEYBOARD 0b101010110101


class interfaceClass
{
    public :
        keyboardClass keyboard;
        //displayClass display;
        void update();
        void init();
#ifdef BENCH
        // Bench hook: exercises the app's own step write path and prints the
        // page bitmask before and after.
        static void benchToggleStep();
#endif
        static void updateVariation();
        static void outputUpdate();
        // static struct limitStruct
        // {
        //     byte _first;
        //     byte _last;
        //     bool _firstLast = 0;
        //     byte _firstPage;
        //     byte _lastPage;
        // }copy, clear, divMult;
        static limitStruct copy;
        static limitStruct clear;
        static limitStruct divMult;

        static bool MIDItick;
    private :
        static bool _diagnosis;
        static int _pot;
        static trackClass *_trackPointer;
        static variationStruct _variation;
        // static variationStruct _variationRead;

        static void _updateInput();
        // static void _updatePage();
        static void _updateTransport();
        static void _updateLiveInputFromPot();
        static void _keyboardTransport(bool state);
// note management
        static noteStruct _inputProcess(noteStruct input);
        static byte _getMIDInote(noteStruct input, bool transpose = false, trackClass* track = nullptr);

        static noteStruct _quantize(noteStruct in, bool keyboard = false);
        static void _noteOut(noteStruct note, bool state, trackClass* track, bool fromKey = false);
        static noteStruct _keyboardToNote(noteStruct key);
        static void _recordNote(trackClass* track, noteStruct value, bool state);
        static void _recordMode(trackClass* track, byte value, bool state);
        static void _handleExternalNoteOn(trackClass* track, byte note, byte velocity, bool allowKeyboardTransport);
        static void _handleExternalNoteOff(trackClass* track, byte note, byte velocity, bool allowKeyboardTransport);
        static noteStruct _midiToQuantizedKeyboardNote(byte midiNote);

        static byte _getOctave(byte value) { return ( (value - 24) / 12); }
        static void _MIDInoteOnHandle(byte channel, byte note, byte velocity);
        static void _MIDInoteOffHandle(byte channel, byte note, byte velocity);
        static byte MIDIunpack(noteStruct note);
        static noteStruct MIDIpack(byte note);

        static noteStruct _noteProcess(noteStruct note);
        static noteStruct _scale(noteStruct note); // <-- to replace with quantize function
        static noteStruct _transpose(noteStruct note);

        static void _noteOnProcess(byte channel, byte note, byte velocity, bool internal = true);
        static void _noteOffProcess(byte channel, byte note, byte velocity, bool internal = true);
        static void _MIDIccHandle(byte channel, byte CC, byte value);
        static void _trackOutput(trackClass *trackPt, byte value);
        static cuisine::Millis _sinceCommand;
        static cuisine::Millis _sincePotOff;
        static cuisine::Millis _sinceStep;
        // cuisine::Millis _sinceMute;
        static cuisine::Millis _sinceSave;
        static bool _quickClearArmed;
        // LEDclass _LED;
        // memoryClass _memory;
        static __attribute__((aligned(16))) trackClass _track[12];
        static byte _selectedTrack; // <-- could be a pointer onoly
        // static byte _trackNumber;
        // static byte _type;
        static const unsigned int _typeToPageTable[2][4][2] PROGMEM;
        static unsigned int _page;
        static unsigned int _oldPage;
        static bool _pageKeySelectFlag;
        static bool _pagePotSelectFlag;
        static bool _pageStepSelectFlag;
        static bool _stepWriteFlag; /////////////
        static bool _popUp;
        static bool _doubleTapCommand;
        static bool _doubleTapStep;
        static bool _shiftStepMode;
        static unsigned _lastTap;
        static bool _chaseFlag;
        static bool _oldChaseFlag;
        // const unsigned int _keys = 0b101010110101;
        
        
        // can be copied : step, track, pattern, preset
        // byte _firstPage;
        // byte _lastPage;
        // byte _copiedTrack;
        // unsigned int _stepSelection;
        // byte _paste;
        
        // bool _openNoteSelectFlag = 0;
        // bool _openGateLenghFlag = 0;
        // bool _openModValueFlag = 0;
        static noteStruct _keyboardNote;
        static noteStruct _writeNote;
        static noteStruct _outNote;
        static noteStruct _midiInNote;
        static noteStruct _noteSet;
        static noteStruct _notePerformed;
        // static int _octave;
        static byte _CCset;
        static byte _mod;
        static bool _rec;
        static bool _overide;
        static bool _oldOveride;
        static byte _noteCount;
        // static byte 
        static byte _patternPage;
        static arpClass _arp;
        static bool _masterResetMult;
        static uint16_t _recIndex; // <-- to check

        static byte _armedMute;
        static bool _armedMuteFlag;
        static void _handleMuteDisplay(byte mute, byte muteTrigOff);

        static bool _trackSelection[4][12];
        static bool _selectionState[4];
        static void _handleClearCommand();
        static void setSelection(byte group, byte track) 
        {
            //     fram.put(adress, in);
            trackClass::setMute(group, track);
            // _trackSelection[group][track] = !_trackSelection[group][track];
            
        }
        static bool getSelection(byte group, byte track){ return trackClass::getMute(group, track); }
        static void setSelectionState(byte group)
        {
            _selectionState[group] = !_selectionState[group];
        }
        static bool getSelectionState(byte group) { return _selectionState[group]; }
        static bool isSelectionActive()
        {
                for (byte i = 0; i < 4; i++)
                {
                        if (_selectionState[i]) { return true; }
                }
                return false;
        }
        // static void selectPatternTrackSelection(byte patternNumber);
        static void saveTrackSelection(byte patternNumber);
        static void loadTrackSelection(byte patternNumber);
        static void saveUpdate();
        static void loadUpdate();
        static byte _trackSaved;
        static byte _trackLoaded;
        static bool _selectionSavedFlag;
        static bool _selectionLoadFlag;
        static byte _targetPattern;
};

#endif