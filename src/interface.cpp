// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
// #include <cstdint>
// #include <cstdint>
#include <display.h>
#include <keyboard.h>
#include <track.h>
#include <LED.h>
#include <midiMinimal.h>
#include <clock.h>
#include <interface.h>
#include <gateOut.h>
#include <pot.h>
#include <DAC.h>
// #include <scale.h>
#include <gateIn.h>
// #include <haptic.h>
#include <arp.h>
#include <eventManager.h>
#include <stdlib.h>
#include <structDefinition.h>
#include <bench.h>
#include "config.h"
// #include <memory.h>
// cuisine::Micros loopTime;
#ifndef ENABLE_DIAGNOSIS
#define ENABLE_DIAGNOSIS 1
#endif
// To do :  - intégrer LED class à display class
//          - track manager
#define VERSION 1721 // V1.5.4.0
// new feature : 
// - Logic operator for gates and CV/Note :
//    - AND, OR, XOR, NAND
//    - ADD, SUB, MIN, MAX
// - Individual transposition for each track 

// optimisation :
// - interrupt based switch reading : no more missed key presses
// - changed  pot calculation from float to fixed point arithmetic for speed : more responsive UI

// Tweaks :
// - Settings page 2 : 
//  - moved the pattern source selection to step 12
//  - moved the accent out selection to step 15
//  - moved the pattern selection to step 12
// polished octave display
//fix :
// Midi input live recording
// no more hanging gate on the track play page

// deprecarted :
// - transpose track option : now acheivable via ADD operator

// Fixed-point sensitivities: 1024 == 1.0
#define DEFAULT_SENSITIVITY 716   // ~0.7 turns
#define VALUE_SENSITIVITY   4096  // 4.0 turns
// MIDI is handled via midiMinimal
variationStruct interfaceClass::_variation = {0, 0, 0};
byte interfaceClass::_selectedTrack = {0};
byte interfaceClass::_patternPage = {0};
// byte interfaceClass::_type = {0};
noteStruct interfaceClass::_keyboardNote = {0,0};
noteStruct interfaceClass::_writeNote = {0,0};
noteStruct interfaceClass::_outNote = {0,0};
noteStruct interfaceClass::_midiInNote = {0,0};
noteStruct interfaceClass::_noteSet = {0,0};
noteStruct interfaceClass::_notePerformed = {0,0};
// int interfaceClass::_octave = {3};
byte interfaceClass::_CCset = {0};
// byte interfaceClass::_track[TRACK_NUMBER].modulation = {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};
bool interfaceClass::_rec = {0};
bool interfaceClass::_overide = {0};
bool interfaceClass::_oldOveride = {0};
trackClass *interfaceClass::_trackPointer = &_track[0];
trackClass interfaceClass::_track[12] = {trackClass(0), trackClass(1), trackClass(2), trackClass(3), 
                                        trackClass(4), trackClass(5), trackClass(6), trackClass(7), 
                                        trackClass(8), trackClass(9), trackClass(10), trackClass(11)};
// arpClass interfaceClass::_trackPointer->arp;
bool interfaceClass::_masterResetMult = {0};
uint16_t interfaceClass::_recIndex = {0};
static unsigned int interfaceClass::_page = STEP_EDIT;
static unsigned int interfaceClass::_oldPage = STEP_EDIT;
const unsigned int interfaceClass::_typeToPageTable[2][4][2] PROGMEM = 
{
    //command
    { 
        {NOTE_SELECT, SCALE},
        {MODE_SET, 0},
        {MODE_SET, 0}, // slew to implement
        {0, 0}, // looper to define     
    },
    //pot & step
    { 
        {NOTE_SELECT, GATE_LENGHT},
        {MODE_SET, GATE_LENGHT },  
        {MODE_SET, GATE_LENGHT },        
        {0, 0}, // looper to define
    }
};
byte interfaceClass::_armedMute = {0};
bool interfaceClass::_armedMuteFlag = {0};

static byte saveProgressLeds(unsigned int t)
{
    if (t >= SAVE_TIME) return 15;
    return (byte)((t * 15UL) / SAVE_TIME);
}

// Paint hold progress; return true when hold expired and step trig-off (commit edge).
static bool saveHoldProgressUI(cuisine::Millis& sinceSave, byte stepIndex, byte armedColor)
{
    if (cuisine::keys::down(stepIndex))
    {
        if (cuisine::keys::pressed(stepIndex)) { sinceSave = 0; }
        byte color = (sinceSave < SAVE_TIME) ? armedColor : (cuisine::led::Yellow & cuisine::led::blink());
        cuisine::display::bar(saveProgressLeds(sinceSave), color);
    }
    return (sinceSave >= SAVE_TIME && cuisine::keys::released(stepIndex));
}

static inline uint8_t nextSetBit(uint16_t& mask)
{
    uint8_t idx = __builtin_ctz((unsigned int)mask); // index of lowest set bit
    mask &= mask - 1;                                // clear that bit
    return idx;
}

static bool getPlayedNote(trackClass* track, noteStruct& noteOut, bool& accentOut)
{
    if (!track) { return false; }

    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) { return false; }

    if (track->arp.getNumKeysPressed())
    {
        noteOut = track->arp.getCurrentNote();
        accentOut = noteOut.accent;
        return true;
    }

    EventState& live = eventManagerClass::liveInput[trackId];
    if (live.active)
    {
        noteOut = live.note;
        accentOut = live.note.accent || live.accent;
        return true;
    }

    if (track->getGateState() || track->getTrigStatus())
    {
        noteOut = track->readNote(track->lastGateIndex());
        accentOut = track->getAccentState();
        return true;
    }

    return false;
}

static bool getPlayedValue(trackClass* track, uint8_t& valueOut)
{
    if (!track) { return false; }

    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) { return false; }

    if (track->arp.getNumKeysPressed())
    {
        valueOut = track->arp.getCurrentValue();
        return true;
    }

    EventState& live = eventManagerClass::liveInput[trackId];
    if (live.active)
    {
        valueOut = live.value;
        return true;
    }

    if (track->getGateState() || track->getTrigStatus())
    {
        valueOut = track->readValue(track->lastGateIndex());
        return true;
    }

    return false;
}

// Renders the sequencer's variation on the LED bar. This used to live in
// displayClass, which forced the display layer to know about trackClass
// (SDK Phase 1.2).
static void displayVariation(trackClass* trackPointer, variationStruct variation, bool blink = false)
{
    cuisine::led::setMask(variation.step, cuisine::led::Red);
    cuisine::led::overlay(variation.accent, cuisine::led::Yellow);
    if (blink & cuisine::led::blink()) { cuisine::led::setMask(0xFFFF, cuisine::led::Off); }

    indexStruct index = trackPointer->getIndexPtr();
    if (index.step != 255 && variation.page == trackPointer->getPage()) {
        cuisine::led::set(index.step, cuisine::led::Green);
    }
}

void interfaceClass::_handleMuteDisplay(byte mute, byte muteTrigOff)
{
    const bool shiftHeld = cuisine::keys::command(SHIFT);
    const bool blinkOn = cuisine::led::blink();

    bool muteGroupDirty[4] = {false, false, false, false};
    bool muteStateDirty = false;

    const uint16_t stepTrigOff = cuisine::keys::stepsReleased();
    const uint16_t trackTrigOff = stepTrigOff & ((1u << TRACK_NUMBER) - 1);

    if (mute)
    {
        for (byte y = 0; y < 4; y++)
        {
            if (!bitRead(mute, y)) { continue; }

            cuisine::led::overlay(trackClass::getMuteGroupBits(y) & 0x0FFF, cuisine::led::Yellow);

            uint16_t bits = trackTrigOff;
            while (bits)
            {
                byte i = (byte)__builtin_ctz(bits);
                bits &= (uint16_t)(bits - 1);
                trackClass::toggleMuteGroupRAM(y, (1u << i));
                muteGroupDirty[y] = true;
            }
        }
    }

    uint16_t lit = trackClass::getRecMuteBits();
    for (byte y = 0; y < 4; y++)
    {
        if (trackClass::getMuteState(y))
        {
            lit |= trackClass::getMuteGroupBits(y);
        }
    }
    cuisine::led::overlay(lit & 0x0FFF, cuisine::led::Yellow);

    for (byte y = 0; y < 4; y++)
    {
        if (trackClass::getMuteState(y) && !shiftHeld)
        {
            cuisine::led::set(y + 12, cuisine::led::Yellow);
        }
        if (bitRead(_armedMute, y) & blinkOn)
        {
            cuisine::led::set(y + 12, cuisine::led::Red);
        }
        if (bitRead(_overide ? (muteTrigOff & ~_armedMute) : muteTrigOff, y) && !shiftHeld)
        {
            trackClass::toggleMuteStateRAM(y);
            muteStateDirty = true;
        }
    }

    for (byte y = 0; y < 4; y++)
    {
        if (muteGroupDirty[y])
        {
            trackClass::commitMuteGroupToFRAM(y);
        }
    }
    if (muteStateDirty)
    {
        trackClass::commitMuteStateToFRAM();
    }
}

void interfaceClass::_handleClearCommand()
{
    if (cuisine::keys::commands() != CLEAR)
    {
        return;
    }

    // live clear
    if (_rec)
    {
        if (cuisine::keys::commandPressed(CLEAR))
            trackClass::setRecMute(_selectedTrack, true);
        if (cuisine::keys::commandReleased(CLEAR))
            trackClass::setRecMute(_selectedTrack, false);

        indexStruct index = _trackPointer->getIndex();
        _trackPointer->writeValue(index, 0);
        _trackPointer->writeStep(index, 0);
        return;
    }

    // clear selection
    uint16_t trigOff = cuisine::keys::stepsReleased();
    while (trigOff)
    {
        uint8_t i = nextSetBit(trigOff);

        if (!clear._firstLast)
        {
            clear._first = i | _patternPage << 4;
            clear._firstLast = true;
        }
        else
        {
            clear._last = i | _patternPage << 4;
            clear._firstLast = false;
            byte destination = (copy._first <= copy._last ? clear._first : clear._last);
            for (byte y = clear._first; y <= clear._last; clear._first <= clear._last ? y++ : y--)
            {
                _trackPointer->writeValue(_trackPointer->indexUnpack(destination), 0);
                _trackPointer->writeStep(_trackPointer->indexUnpack(destination), 0);
                destination = clear._first <= clear._last ? destination + 1 : destination - 1;
                destination = destination % (STEP_NUMBER + 1);
            }
        }
    }
}

byte interfaceClass::_trackSaved = {0};
byte interfaceClass::_trackLoaded = {0};
bool interfaceClass::_selectionSavedFlag = {1};
bool interfaceClass::_selectionLoadFlag = {1};
byte interfaceClass::_targetPattern = {0};
limitStruct interfaceClass::copy;
limitStruct interfaceClass::clear;
limitStruct interfaceClass::divMult;
// unsigned int interfaceClass::_trackSelection[4] = {0xFFF, 0xFFF, 0xFFF, 0xFFF};
static bool interfaceClass::_trackSelection[4][12] = {0};
static bool interfaceClass::_selectionState[4] = {0};

bool interfaceClass::_diagnosis = 0;
bool interfaceClass::MIDItick = 0;
int interfaceClass::_pot = 0;
//byte interfaceClass::_selectedTrack = {0};

static bool interfaceClass::_pageKeySelectFlag = 0;
static bool interfaceClass::_pagePotSelectFlag = 0;
static bool interfaceClass::_pageStepSelectFlag = 0;
static bool interfaceClass::_stepWriteFlag = 0; /////////////
static bool interfaceClass::_popUp = 0;
static bool interfaceClass::_doubleTapCommand = false;
static bool interfaceClass::_shiftStepMode = false;
static unsigned interfaceClass::_lastTap = 0;
static bool interfaceClass::_chaseFlag = false;
static bool interfaceClass::_oldChaseFlag = false;

static cuisine::Millis interfaceClass::_sinceCommand = 0;
static cuisine::Millis interfaceClass::_sincePotOff = 0;
static cuisine::Millis interfaceClass::_sinceStep = 0;
// cuisine::Millis _sinceMute = 0;
static cuisine::Millis interfaceClass::_sinceSave = 0;
bool interfaceClass::_quickClearArmed = false;

byte cycleDivider = 0;

// Defined at the bottom; hooked into the SDK timer interrupt from init().
static void midiClockTick();

void interfaceClass::init()
{
    // Hardware is already up (cuisine::begin() in setup), including the
    // keyboard scan settle, so edge detection is valid from here on.
    cuisine::clock::onHardwareTick(midiClockTick);

    trackClass::initEEPROM();

    _updateInput();

    for (byte i = 0; i < TRACK_NUMBER; i++) 
    { 
        
        if(cuisine::keys::commands() == SHIFT && cuisine::keys::arm())
        {
            _track[i].reinit(); 
            //init all track to default value :
            if (i < 4) 
            { 
                _track[i].setType(NOTE);
                _track[i].setMIDIchannel(i + 1);
                _track[i].setMIDIoutChannel(i + 1);
                int cvOut = i < 2 ? i + 1 : -1;
                
                _track[i].setCVout(cvOut);
                _track[i].setGateout(i);
                _track[i].setCC(0);
            }
            // Drum
            else if (i < 8) 
            { 
                _track[i].setType(DRUM);
                _track[i].setMIDIchannel(10);
                _track[i].setMIDIoutChannel(10);
                _track[i].setCVout(-1);
                _track[i].setGateout(i);
                _track[i].setCC(0);
            }
            //  Mod
            else if (i < 12) 
            { 
                _track[i].setType(MOD);
                _track[i].setMIDIchannel(0);
                _track[i].setMIDIoutChannel(0);
                int cvOut = i < 10 ? (i % 2) + 2 + 1 : -1;
                _track[i].setCVout(cvOut);
                _track[i].setGateout(255); // no gate out
                _track[i].setCC(0);
            }
            // set all operator sources to 255
            for(int y = 0; y < 8; y++)
            {
                _track[i].setOperatorSource(y, 255);
            }
            _track[i].setTransposeOffset({0, 0});
            //general settings :
            // clock mode

            //Common bank :
            // scale[4]
            // BPM
            // group mute
            // pattern selector

            // preset:
            _track[i].clockInputSet(0);
            _track[i].clockSourceSet(0);
            _track[i].resetInputSet(0);
            _track[i].resetSourceSet(0);
            _track[i].setAccentOut(255);
            // _track[i].setTranspose(false);
            _track[i].savePreset();

            //  Pattern :
            _track[i].setFirstStep(0);
            _track[i].setLastStep(15);
            _track[i].setTimeDivision(5); // 1/16
            _track[i].setSize(0);
            _track[i].setClockOffset(0);
            _track[i].setLowVelocity(80);
            _track[i].setHighVelocity(127);
            _track[i].setPlayMode(NORMAL);
            _track[i].writeAllStep(false);
            _track[i].writeAllAccent(false);


            for(int y = 0; y < PATTERN_NUMBER; y++)
            {
                
                _track[i].savePattern(y);

            }

            _track[i].selectPattern(0);

            trackClass::resetScale();
            trackClass::setBPM(1270);
            
        }
       _track[i].init();
        
        for (int y = 0; y < 4; y++)
        {
            trackClass::setMuteState(y, false);
        }
    }
    trackClass::bankPresetInit();

#if ENABLE_DIAGNOSIS
    if(cuisine::keys::down(0) && cuisine::keys::down(15))
    {
        _diagnosis = true;
        
    }
#endif
    // display firmwire version;
    if(cuisine::keys::commands() == PAGE)
    {
        cuisine::led::setMask(0xFFFF, cuisine::led::Off);
        cuisine::led::flush(8, true);
        cuisine::display::number(VERSION);
        cuisine::led::flush(8, true);
        delay(10000);
    }

    cuisine::midi::onNoteOn(_MIDInoteOnHandle);
    cuisine::midi::onControlChange(_MIDIccHandle);
    cuisine::midi::onNoteOff(_MIDInoteOffHandle);
}
void interfaceClass::update()
{
    // Bench protocol: consumes serial commands and queues deterministic ticks.
    // Empty in the production build (see bench.h).
    bench::poll();

    // Tap tempo: the SHIFT edge used to be caught in the keyboard ISR. It moved
    // here so the input layer no longer depends on the feature (SDK Phase 1.1).
    if (cuisine::keys::commandsPressed() & SHIFT) { tap::addTap(); }

    cycleDivider++;
    // debug();
    loadUpdate();
    saveUpdate();
    // Process all triggers BEFORE updating old values
    // This ensures triggers are detected in _updateInput() and _updateTransport()
    _updateInput();
    clockClass::masterUpdate(); 
    cuisine::midi::poll();
    _updateTransport();
    keyboardClass::update(); // Update old values AFTER all trigger processing
    _handleClearCommand();
    updateVariation();

    cuisine::led::flush(8);
    cuisine::led::setMask(0xFFFF, cuisine::led::Off); 

    // potClass::updateReading();
    gateInClass::update();

    trackClass::latchPlayTrigs();
    for (int i = 0; i < TRACK_NUMBER; i++)
    {
        _track[i].update();
    }
    
    _oldChaseFlag = _chaseFlag;
    _patternPage = _chaseFlag ? _trackPointer->getIndex().page : _patternPage;
    switch (_page)
    {
        case TRACK :
        {
            byte mute = cuisine::keys::steps() >> 12;
            byte muteTrigOff = cuisine::keys::stepsReleased() >> 12;

            _handleMuteDisplay(mute, muteTrigOff);

            if(!mute)
            {
                // track selection (skip loop when no key activity)
                unsigned int trackTrig = cuisine::keys::stepsPressed();
                unsigned int trackTrigOff = cuisine::keys::stepsReleased();
                if (trackTrig || trackTrigOff)
                {
                    uint16_t trig = (uint16_t)trackTrig;
                    while (trig)
                    {
                        uint8_t i = nextSetBit(trig);
                        if (i < TRACK_NUMBER)
                        {
                            _selectedTrack = i;
                            _trackPointer = &_track[i];
                            _patternPage = 0;
                            if(_overide && _trackPointer->getPlayMode() == GATE)
                            {
                                _MIDInoteOnHandle(0, i, 127);
                            }
                        }
                    }
                    uint16_t trigOff = (uint16_t)trackTrigOff;
                    while (trigOff)
                    {
                        uint8_t i = nextSetBit(trigOff);
                        if (i < TRACK_NUMBER)
                        {
                            _selectedTrack = i;
                            _trackPointer = &_track[i];
                            if(_overide && _trackPointer->getPlayMode() == GATE)
                            {
                                _MIDInoteOffHandle(0, i, 127);
                            }
                        }
                    }
                }

                if(_trackPointer->getTrackMute(_selectedTrack)) { (cuisine::led::blink()) ? cuisine::led::set(_selectedTrack, cuisine::led::Yellow) : cuisine::led::set(_selectedTrack, cuisine::led::Green); }
                else { cuisine::led::set(_selectedTrack, cuisine::led::Green); }

                // display track state (batch LED update)
                unsigned int gateMask = 0;
                for (int i = 0; i < 12; i++) { if (_track[i].getGateState()) gateMask |= (1 << i); }
                if (gateMask) { cuisine::led::overlay(gateMask, cuisine::led::Red); }
            }
            break;
        }
        case TRACK_PLAY :
        {

            byte mute = cuisine::keys::steps() >> 12;
            byte muteTrigOff = cuisine::keys::stepsReleased() >> 12;

            _handleMuteDisplay(mute, muteTrigOff);

            if(_trackPointer->getTrackMute(_selectedTrack)) { (cuisine::led::blink()) ? cuisine::led::set(_selectedTrack, cuisine::led::Yellow) : cuisine::led::set(_selectedTrack, cuisine::led::Green); }
            else { cuisine::led::set(_selectedTrack, cuisine::led::Green); }

            // display track state (batch LED update)
            unsigned int gateMask = 0;
            for (int i = 0; i < 12; i++) { if (_track[i].getGateState()) gateMask |= (1 << i); }
            if (gateMask) { cuisine::led::overlay(gateMask, cuisine::led::Red); }

            if(cuisine::keys::stepsPressed())
            {
                uint16_t trig = cuisine::keys::stepsPressed();
                while (trig)
                {
                    uint8_t i = nextSetBit(trig);

                    //select track :
                    if(i < 12)
                    {
                        _selectedTrack = i;
                        _trackPointer = &_track[i];
                    }
                    // GATE / ONE_SHOT: manual start; suppress play-source follow while held
                    if(_trackPointer->isIndependentPlay())
                    {
                        _trackPointer->setKeyboardFlag(true);
                        // already playing + SHIFT → reset; else start
                        _trackPointer->getTransport() && cuisine::keys::command(SHIFT) ? _trackPointer->reset() : _trackPointer->setTransport(true);

                    }
                    // NORMAL: output the track's selected value
                    else
                    {
                        switch(_trackPointer->getType())
                        {
                            case NOTE:
                            {
                                noteStruct noteOut = _inputProcess(_trackPointer->getDefaultNote());
                                eventManagerClass::setLiveNote(_trackPointer, noteOut, true);
                                break;
                            }
                            case DRUM :
                            {
                                byte value = _trackPointer->modulation;
                                eventManagerClass::setLiveValue(_trackPointer, value, true);
                                break;
                            }
                            case MOD :
                            {
                                byte value = _trackPointer->modulation;
                                eventManagerClass::setLiveValue(_trackPointer, value, true);
                                break;
                            }
                        }
                    }
                }
            }
            if(cuisine::keys::stepsReleased() && cuisine::keys::commands() != SHIFT)
            {
                uint16_t trigOff = cuisine::keys::stepsReleased();
                while (trigOff)
                {
                    uint8_t i = nextSetBit(trigOff);
                    if (i < 12)
                    {
                        trackClass* releasedTrack = &_track[i];
                        releasedTrack->setKeyboardFlag(false);
                        if(releasedTrack->getPlayMode() == GATE)
                        {
                            releasedTrack->getTransport() ? releasedTrack->setTransport(false) : void();
                        }
                        else if (!releasedTrack->isIndependentPlay())
                        {
                            switch(releasedTrack->getType())
                            {
                                case NOTE:
                                {
                                    noteStruct noteOut = _inputProcess(releasedTrack->getDefaultNote());
                                    eventManagerClass::setLiveNote(releasedTrack, noteOut, false);
                                    break;
                                }
                                case MOD:
                                {
                                    eventManagerClass::setLiveValue(releasedTrack, releasedTrack->modulation, false);
                                    break;
                                }
                                case DRUM:
                                {
                                    eventManagerClass::setLiveValue(releasedTrack, releasedTrack->modulation, false);
                                    break;
                                }
                            }
                        }
                        // ONE_SHOT: flag cleared; transport runs until loop end
                    }
                }
            }
            
            break;
        }
        case BPM_PAGE :
        {
            // Tap Tempo Logic
            if (cuisine::keys::commandPressed(SHIFT))
            {
                // tap::addTap();

                if (_shiftStepMode)
                {
                    // Finalize Shift+Step Sequence
                    unsigned int newBPM = tap::getBPM();
                    if (newBPM > 0)
                    {
                        clockClass::setBPM(newBPM);
                        trackClass::setBPM(newBPM);
                        
                        // Reset All Tracks (Stop/Start transport to align)
                        clockClass::stopHandler();
                        clockClass::startHandler();
                        for(int i=0; i<TRACK_NUMBER; i++) 
                        { 
                            _track[i].reset(); 
                        }
                    }
                    _shiftStepMode = false;
                    tap::reset();
                }
                else
                {
                    // Simple Shift Mode
                    if (tap::getLastTapsNumber() > 0)
                    {
                        unsigned int newBPM = tap::getBPM();
                        if (newBPM > 0)
                        {
                            clockClass::setBPM(newBPM);
                            trackClass::setBPM(newBPM);
                        }
                    }
                }
            }
            else if (cuisine::keys::stepsPressed())
            {
                tap::addTap();
                _shiftStepMode = true;
            }

            unsigned int BPM = trackClass::getBPM();

            BPM = cuisine::pot::read(BPM, 8 * 1024, 300, 2700);
            
            clockClass::setBPM(BPM);
            trackClass::setBPM(BPM);
            cuisine::display::number(BPM);
            break;
        }
        case STEP_EDIT :
        case STEP_MUTE :
        {

            if(!(cuisine::keys::commands() & (COPY | PASTE | CLEAR)) && cuisine::keys::stepsReleased())
            {
                uint16_t trigOff = cuisine::keys::stepsReleased();
                while (trigOff)
                {
                    uint8_t i = nextSetBit(trigOff);

                    // byte step = i | (_patternPage << 4);
                    indexStruct index = {i, _patternPage};

                    // check if we are just out of the note select pop up
                    if (_stepWriteFlag )
                    {
                        _trackPointer->writeStep(index, true);
                        _stepWriteFlag = false;
                    }
                    else
                    {
                        _trackPointer->writeStep(index);
                        if(_trackPointer->readStep(index)) { _trackPointer->writeAccent(index, false); }
                    }
                    //mute and unmute step with STEP_MUTE or overide active (bypassing _noteSet value)
                    if(_page == STEP_EDIT | !_overide && !cuisine::keys::command(SHIFT) )
                    {
                        // init gate duration to 0
                        _trackPointer->writeDuration(index, 0);
                        switch (_trackPointer->getType())
                        {
                            case MOD:
                            {
                                _trackPointer->writeValue( index, _trackPointer->modulation);
                                break;
                            }
                            case NOTE:
                            {
                                _noteSet.accent = false;
                                _trackPointer->writeNote( index, _noteSet);
                                break;
                            }
                            case DRUM:
                            {
                                _trackPointer->writeValue( index, _trackPointer->modulation);
                            }
                        }
                    }
                }
            }
            
            displayVariation(_trackPointer, _variation);
            (cuisine::led::blink()) ? cuisine::led::overlay(_trackPointer->euclid.getPattern(_patternPage), cuisine::led::Yellow) : void(0);
            break;
        }
        case ACCENT :
        {
            if(!(cuisine::keys::commands() & (COPY | PASTE | CLEAR)) && cuisine::keys::stepsReleased())
            {
                uint16_t trigOff = cuisine::keys::stepsReleased();
                while (trigOff)
                {
                    uint8_t i = nextSetBit(trigOff);
                    indexStruct index = {i, _patternPage};
                    if (_trackPointer->readStep(index))
                    {
                        bool accent = _trackPointer->readAccent(index);
                        _trackPointer->writeAccent(index, !accent);
                    }
                }
            }
            displayVariation(_trackPointer, _variation);
            break;
        }
        case LOW_VELOCITY :
        {
            byte velocity = _trackPointer->getLowVelocity();
            velocity = cuisine::pot::read(velocity, DEFAULT_SENSITIVITY, 1, 127);
            _trackPointer->setLowVelocity(velocity);
            cuisine::display::bar(_trackPointer->getLowVelocity() >> 3, cuisine::led::Red, true);
            break;
        }
        case HIGH_VELOCITY :
        {
            byte velocity = _trackPointer->getHighVelocity();
            velocity = cuisine::pot::read(velocity, DEFAULT_SENSITIVITY, 1, 127);
            _trackPointer->setHighVelocity(velocity);
            cuisine::display::bar(_trackPointer->getHighVelocity() >> 3, cuisine::led::Yellow);
            break;
        }
        case NOTE_SELECT :
        {
            noteStruct notePlayed;
            bool notePlayedAccent = false;
            bool playedNoteActive = getPlayedNote(_trackPointer, notePlayed, notePlayedAccent);
            // noteStruct transposeNote = _trackPointer->getTransposeOffsetPtr();
            noteStruct* pointerNote;
            bool editingTranspose = cuisine::keys::command(SHIFT);
            editingTranspose ? pointerNote = _trackPointer->getTransposeOffsetPtr() : pointerNote = &_notePerformed;
            
            // _keyboardNote = *pointerNote;
            // if(cuisine::keys::commandPressed(SHIFT))
            // {
            //     _keyboardNote = transposeNote;
            // }
            // else if(cuisine::keys::commandReleased(SHIFT))
            // {
            //     _keyboardNote = _noteSet;
            // }

            // display set
            if (!trackClass::getNoteScaleCount()) {cuisine::led::setMask(KEYBOARD, cuisine::led::Yellow);}
            else
            {
                cuisine::display::bar(trackClass::getNoteScaleCount() - 1, cuisine::led::Yellow);
            }

            // note select
            if (_trackPointer->getType() == NOTE)
            {   
                int combinedNote =  pointerNote->note + (pointerNote->octave * (trackClass::getNoteScaleCount() ? trackClass::getNoteScaleCount() : 12));
                if (cuisine::pot::moving())
                {
                    combinedNote = cuisine::pot::read(combinedNote, VALUE_SENSITIVITY, 0, 127);
                    pointerNote->octave = trackClass::getNoteScaleCount() ? combinedNote / trackClass::getNoteScaleCount() : combinedNote / 12;
                    pointerNote->note = trackClass::getNoteScaleCount() ? combinedNote % trackClass::getNoteScaleCount() : combinedNote % 12;
                }
                // if (!editingTranspose) { _trackPointer->setTransposeOffset(*pointerNote); }
                if (!editingTranspose) 
                { 
                    _trackPointer->setDefaultNote(*pointerNote); 
                    _noteSet = _notePerformed;
                }
                if (_overide && !editingTranspose)
                {
                    eventManagerClass::setLiveNotePot(_trackPointer, *pointerNote, cuisine::pot::moving());
                }

                // pop up from step edit
                // 1. Initial Contact (Pick-up/First Write)
                if ( cuisine::keys::steps() && !cuisine::keys::command(SHIFT) && (_pageKeySelectFlag || _pageStepSelectFlag))
                {
                    for (int i = 0; i < 16; i++)
                    {
                        if ( cuisine::keys::down(i) )
                        {
                            indexStruct index = {i, _patternPage};
                            
                            // Handle initial contact (Pick-up or Write)
                            if (_popUp) {
                                if ( _trackPointer->readStep(index) ) 
                                { 
                                    _noteSet = _trackPointer->readNote(index); 
                                }
                                else { _trackPointer->writeNote(index, _noteSet); }
                                // Note: _popUp will be cleared below after the loop
                            }

                            // 2. Continuous Write: If pot moves while button is held, update the step
                            if (cuisine::pot::moving()) {
                                static noteStruct oldNote = *pointerNote;
                                // check if step old value is different from new value
                                if (oldNote.note != pointerNote->note || oldNote.octave != pointerNote->octave) {
                                    oldNote = *pointerNote;
                                    _trackPointer->writeNote(index, *pointerNote);
                                    // Ensure step is active if we are intentionally setting a value
                                    if (!_trackPointer->readStep(index)) _trackPointer->writeStep(index, true);
                                }
                            }
                        }
                    }
                    _popUp = false; // Move this outside the loop to ensure all pressed steps are processed
                }
                

                ///////////////////////////////////////////////
                // output internal keyboard (skip loop when no key activity)
                unsigned int trig = cuisine::keys::stepsPressed();
                unsigned int trigOff = cuisine::keys::stepsReleased();
                if (trig || trigOff)
                {
                    for (int i = 0; i < 16; i++)
                    {
                        // octave set
                        if (i >= 12)
                        {

                            if (cuisine::keys::pressed(i)) 
                            { 
                                if(i == 12)
                                {
                                    pointerNote->octave--; 
                                }
                                else if(i == 13)
                                {
                                    pointerNote->octave++;
                                }
                                // accent off
                                else if ( i == 14)
                                {
                                    pointerNote->accent = false;
                                    if(_overide)
                                    {
                                        eventManagerClass::setLiveAccent(_trackPointer, false, true);
                                    }
                                }
                                // accent on
                                else if ( i == 15)
                                {
                                    pointerNote->accent = true;
                                    if(_overide)
                                    {
                                        eventManagerClass::setLiveAccent(_trackPointer, true, true);
                                    }
                                }
                                pointerNote->octave = CLAMP(pointerNote->octave, -4, 4);
                                if (editingTranspose) { _trackPointer->setTransposeOffset(*pointerNote); }
                            }
                            if (cuisine::keys::released(i))
                            {
                                // Momentary override: on release, derive new override from the OTHER key only.
                                // This avoids stale level reads of the key that just triggered the falling edge.
                                if (i == 14)
                                {
                                    pointerNote->accent = false;
                                    // Accent cuisine::led::Off released: if ACCENT ON is still held, override ON; otherwise disable override.
                                    if(_overide)
                                    {
                                        eventManagerClass::setLiveAccent(_trackPointer, cuisine::keys::down(15), cuisine::keys::down(15));
                                    }
                                }
                                else if (i == 15)
                                {
                                    pointerNote->accent = false;
                                    // Accent ON released: if ACCENT cuisine::led::Off is still held, override cuisine::led::Off; otherwise disable override.
                                    if(_overide)
                                    {
                                        eventManagerClass::setLiveAccent(_trackPointer, false, cuisine::keys::down(14));
                                    }
                                }
                            }
                            // if(cuisine::keys::commandPressed(SHIFT) 
                            // || cuisine::keys::commandReleased(SHIFT)
                            // || cuisine::keys::pressed(i) )
                        }    
                        //read keyboard                  
                        else if (i < 12)
                        {
                            
                            if (cuisine::keys::pressed(i))
                            {   
                                // Prepare raw keyboard note (for display and storage)
                                _keyboardNote = *pointerNote;
                                pointerNote->note = i;

                                if (editingTranspose) 
                                { 
                                    // In transpose edit, store RAW offset (no scale wrapping here)
                                    _trackPointer->setTransposeOffset(*pointerNote);
                                }
                                else
                                {
                                    // In normal note mode, play a processed copy but keep `_noteSet` raw
                                    if(_trackPointer->isIndependentPlay()) 
                                    {
                                        !_trackPointer->getTransport() ? _trackPointer->setTransport(true) : void();
                                    }
                                    pointerNote->accent = (cuisine::keys::down(15) && !cuisine::keys::down(14)) ? true : pointerNote->accent;  
                                    pointerNote->accent = (cuisine::keys::down(14) && !cuisine::keys::down(15)) ? false : pointerNote->accent; 

                                    _noteSet = _inputProcess(*pointerNote);
                                    _noteSet.accent = pointerNote->accent;
                                    eventManagerClass::setLiveNote(_trackPointer, _noteSet, true);
                                }
                                
                            }
                            else if ( cuisine::keys::released(i) && !cuisine::keys::command(SHIFT))
                            { 
                                _keyboardNote = *pointerNote;
                                _keyboardNote.note = i;
                                
                                // Only live gate off needs processed note; storage remains raw
                                _keyboardNote = _inputProcess(_keyboardNote);
                                eventManagerClass::setLiveNote(_trackPointer, _keyboardNote, false);   
                            }
                        }    
                    }
                } 
                
                // display notes
                // notePlayed
                // notePointer
                noteStruct noteRef = *pointerNote;
                byte octaveRange = trackClass::getNoteScaleCount() ? trackClass::getNoteScaleCount() : 12;
                bool inRange = false;
                noteRef =  _inputProcess(noteRef);
                // first we compare octave :
                int8_t octaveDiff = noteRef.octave - pointerNote->octave;
                // notePlayed in range of octaveRange
                if (octaveDiff >= 0 && octaveDiff < octaveRange)
                {
                    // notePlayed.note += octaveDiff * trackClass::getNoteScaleCount();
                    inRange = true;
                }
                else
                {
                    inRange = false;
                }

                if (playedNoteActive && pointerNote->note == notePlayed.note) 
                { 
                    // cuisine::led::set(_noteSet.note, cuisine::led::Green & cuisine::led::shortBlink()); 
                    byte color = cuisine::led::blink() ? cuisine::led::Red : cuisine::led::Green;
                    cuisine::led::set(notePlayed.note, color);
                }
                else
                {
                    cuisine::led::set(pointerNote->note, cuisine::led::Green);

                    if (playedNoteActive)
                    {
                        // diplay played note transposed :
                        noteStruct trackTranspose = _trackPointer->getTransposeOffset();
                        byte note = _trackPointer->getTranspose() ? trackTranspose.note + notePlayed.note : notePlayed.note;
                        const byte scaleNotes = _trackPointer->getNoteScaleCount();
                        cuisine::led::set(scaleNotes ? note % scaleNotes : note, cuisine::led::Red);
                    }
                }
                cuisine::display::octave(pointerNote->octave);
                if (playedNoteActive && notePlayedAccent) { cuisine::led::set(15, cuisine::led::Red); }
                // _noteSet.octave != notePlayed.octave ? cuisine::display::octave(notePlayed.octave) : 
                // cuisine::display::octave(_noteSet.octave); 
                if (playedNoteActive && _noteSet.octave > notePlayed.octave) 
                { 
                    cuisine::led::shortBlink() ? cuisine::led::set(12, cuisine::led::Yellow) : void();
                }
                else if (playedNoteActive && _noteSet.octave < notePlayed.octave)
                {
                    cuisine::led::shortBlink() ? cuisine::led::set(13, cuisine::led::Yellow) : void();
                }
            }           
            break;
        }
        case SCALE :
        {
            // display set
            cuisine::led::setMask(KEYBOARD, cuisine::led::Yellow);

            for(int i = 12; i < 16; i++)
            {
                if(cuisine::keys::pressed(i))
                {
                    trackClass::setChordState(i - 12, true);
                    _page = CHORD;
                }
            }
            for (int i = 0; i < 12; i++)
            {
                if (cuisine::keys::pressed(i) & !(cuisine::keys::steps() & 0b1111000000000000))
                {
                    trackClass::setScale(i);
                }
            }
            
            cuisine::led::overlay(trackClass::getScale(true), cuisine::led::Green, false);
            cuisine::led::overlay(trackClass::getScale(), cuisine::led::Red, false);
            cuisine::led::overlay(trackClass::getChordState() << 12, cuisine::led::Red);
            
            break;
        }
        case CHORD :
        {
            // activate or deactivate chord
            for(int i = 12; i < 16; i++)
            {
                if(cuisine::keys::pressed(i))
                {
                    trackClass::setChordState(i - 12, true);
                }
                else if(cuisine::keys::released(i) && !(cuisine::keys::commands() == SHIFT))
                {
                    trackClass::setChordState(i - 12, false);
                }
            }
            

            byte chord = cuisine::keys::steps() >> 12;
            for(int i = 0; i < 12; i++)
            {
                if(cuisine::keys::pressed(i))
                {
                    for(int y = 0; y < 4; y++)
                    {
                        if(bitRead(chord, y))
                        {
                            trackClass::setChord(y, i);
                        }
                    }
                }
            }
            // back to SCALE page if no chord pressed
            if(!cuisine::keys::steps())
            {
                _page = SCALE;
            }
            else
            {
                
                for(int i = 12; i < 16; i++)
                {
                    if(bitRead(cuisine::keys::steps(), i))
                    {
                        // display scale
                        for (int i = 0; i < trackClass::getNoteScaleCount(); i++)
                        {
                            cuisine::led::set(i, cuisine::led::Green);
                        }
                        cuisine::led::overlay(trackClass::getChord(i - 12, true), cuisine::led::Red, false);
                    }
                }
                // cuisine::led::overlay(trackClass::getChordState() << 12, cuisine::led::Red);
            }
            break;
        }
        case MODE_SET :
        {
            static byte value = 0;
            byte *valuePointer = &value;
            //CC/velocity value
            if ( _trackPointer->getType() == MOD || _trackPointer->getType() == DRUM)
            {
                // 1. POTENTIOMETER: Continuous Live Control
                if(cuisine::pot::moving())
                {
                    _trackPointer->modulation = cuisine::pot::read(_trackPointer->modulation, DEFAULT_SENSITIVITY, 0, 255);
                }
                // 2. STEP BUTTONS: Memory Interaction (Edit/Pick-up)
                if ( cuisine::keys::steps() && !cuisine::keys::command(SHIFT) && (_pageKeySelectFlag || _pageStepSelectFlag))
                {
                    indexStruct index = {0, _patternPage};
                    for (int i = 0; i < 16; i++)
                    {
                        if ( cuisine::keys::down(i)  )
                        {
                            index = {i, _patternPage};
                            if (_popUp) {
                                // Initial contact: Pick-up if active, Write if inactive
                                eventManagerClass::handleStepInteraction(_trackPointer, index, true);
                            }
                        }
                    }
                    // Continuous write while the step is held: update step value on pot movement
                    if (cuisine::pot::moving()) {
                        byte oldValue = _trackPointer->readValue(index);
                        if(oldValue != _trackPointer->modulation)
                        {
                            _trackPointer->writeValue(index, _trackPointer->modulation);
                            if (!_trackPointer->readStep(index)) _trackPointer->writeStep(index, true);
                        }
                    }
                    _popUp = false;
                }


                // 3. KEYBOARD TRIGGERS: Momentary Performance
                {
                    uint16_t trig = cuisine::keys::stepsPressed();
                    uint16_t trigOff = cuisine::keys::stepsReleased();
                    if (trig || trigOff)
                    {
                        for (byte i = 0; i < 16; i++)
                        {
                            uint8_t val = (uint8_t)(((i + 1) << 4) - 1);
                            if (trig & (1u << i))
                            {
                                _keyboardTransport(true);
                                eventManagerClass::handleLiveModOn(_trackPointer, val);
                            }
                            else if ((trigOff & (1u << i)) && !cuisine::keys::command(SHIFT))
                            {
                                _keyboardTransport(false);
                                eventManagerClass::handleLiveModOff(_trackPointer, val);
                            }
                        }
                    }
                }

                // 4. DISPLAY: Visual Feedback
                uint8_t displayValue = _trackPointer->modulation;
                if(_overide)
                {
                    getPlayedValue(_trackPointer, displayValue);
                }

                if (!displayValue) {  cuisine::led::setMask(0xFF, cuisine::led::Off); }
                else 
                {
                    cuisine::display::bar( (displayValue >> 4), cuisine::led::Yellow);
                }
            }
            break;
        }
        case EUCLID :
        {
            cuisine::led::setMask(0xFFFF, cuisine::led::Off);
            byte* valueToChangePointer;
            if(_trackPointer->euclid.page == BEAT_PAGE) { valueToChangePointer = &_trackPointer->euclid.beats;}
            else if(_trackPointer->euclid.page == LENGHT_PAGE) { valueToChangePointer = &_trackPointer->euclid.lenght;}
            else if(_trackPointer->euclid.page == OFFSET_PAGE) { valueToChangePointer = &_trackPointer->euclid.offset;}
            if(cuisine::pot::movingLatch())
            {
                
                *valueToChangePointer = cuisine::pot::read(*valueToChangePointer, DEFAULT_SENSITIVITY, 0, 16);
                _trackPointer->euclid.offset = (_trackPointer->euclid.offset > 15) ?  15 : _trackPointer->euclid.offset;
                _trackPointer->euclid.update();
            }
            
            if (cuisine::keys::stepsReleased())
            {
                for (int i = 0; i < 16; i++)
                {
                    if ( cuisine::keys::released(i) )
                    {
                        *valueToChangePointer = i + 1;
                        _trackPointer->euclid.update();
                    }
                }
            }

            displayVariation(_trackPointer, _variation, true);
            cuisine::led::overlay(_trackPointer->euclid.getPattern(), cuisine::led::Yellow);
            // set all the first beat of the euclidian pattern green
            unsigned int firstSteps = 0;
            for (int i = 0; i < 64; i += _trackPointer->euclid.lenght)
            {
                int index = (i + _trackPointer->euclid.offset) % 64;
                firstSteps |= 1 << index;
            }
            ( _trackPointer->euclid.beats) ? cuisine::led::overlay(firstSteps, cuisine::led::Green) : void(0);
            
            break;
        }
        case GATE_LENGHT :
        {
            byte duration = 0;
            if ( cuisine::keys::steps() )
            {
                for (int i = 0; i < 16; i++)
                {   
                    if (cuisine::keys::down(i) )
                    {
                        indexStruct index = {i, _patternPage};
                        if (_trackPointer->readStep(index) ) { duration = _trackPointer->getDuration(index); }
                        else { duration = 0; }
                        duration = cuisine::pot::read(duration, DEFAULT_SENSITIVITY, 0, 255);
                        _trackPointer->writeDuration(index, duration );
                                  
                    }
                }
            }
            // set offset gate duration
            else if (cuisine::keys::command(SHIFT) && !cuisine::keys::steps()) 
            { 
                duration = _trackPointer->gateLenghtOffset;
                duration = cuisine::pot::read(_trackPointer->gateLenghtOffset, DEFAULT_SENSITIVITY, 0, 255);
                _trackPointer->gateLenghtOffset = duration;
            }
            duration = duration >> 4;
            if (duration) { cuisine::display::bar(duration, cuisine::led::Red);}
                        

            if (_trackPointer->gateLenghtOffset == 255 && !cuisine::keys::steps() && cuisine::led::blink()) { cuisine::led::set(15, cuisine::led::Green);}
            break;
        }
        case TRACK_SETTINGS :
        {
            
            cuisine::led::set(_trackPointer->getType(), cuisine::led::Yellow);
            cuisine::led::set( (_trackPointer->getClockSource() | 4), cuisine::led::Red);
            cuisine::led::set( (_trackPointer->getResetSource() | 8), cuisine::led::Green);
            {
                byte m = _trackPointer->getPlayMode();
                byte color = (m == ONE_SHOT) ? cuisine::led::Yellow : (m == GATE) ? cuisine::led::Red : cuisine::led::Green;
                cuisine::led::set(13, color);
            }
            cuisine::led::set( 12, _trackPointer->getMIDIchannel() ? cuisine::led::Green : cuisine::led::Off);
            cuisine::led::set( 14, _trackPointer->getCVOut() >= 0 ? cuisine::led::Yellow : cuisine::led::Off);
            cuisine::led::set( 15, _trackPointer->getGateOut() < 255 ? cuisine::led::Red : cuisine::led::Off);

            
            //read keyboard
            for (int i = 0; i < 16; i++)
            {
                
                if ( cuisine::keys::released(i) )
                {
                    cuisine::led::setMask(0xFFFF, cuisine::led::Off);
                    byte input =  i & 0b0000000011;
                    
                    //type set
                    if (i < 4) 
                    { 
                        if(input == DRUM && _trackPointer->getType() != input) { _trackPointer->setNoteSelected(_getMIDInote(_noteSet)); }
                        _trackPointer->setType(input); 
                        
                    }
                    // clock source and input
                    else if (i < 8  ) 
                    { 
                        if(cuisine::keys::command(SHIFT))
                        {
                            for(int j = 0; j < TRACK_NUMBER; j++)
                            {
                                _track[j].clockSourceSet(input);
                            }
                        }
                        else
                        {
                            _trackPointer->clockSourceSet(input); 
                        }
                    }
                    // reset source and input
                    else if (i < 12  ) 
                    { 
                        if(cuisine::keys::command(SHIFT))
                        {
                            for(int j = 0; j < TRACK_NUMBER; j++)
                            {
                                _track[j].resetSourceSet(input);
                            }
                        }
                        else
                        {
                            _trackPointer->resetSourceSet(input); 
                        }
                    }
                    // MIDI/modular in/out param
                    else if (i == 13 && !cuisine::keys::command(SHIFT)) {
                        _trackPointer->setPlayMode( (byte)((_trackPointer->getPlayMode() + 1) % 3) );
                    } 
                }
                else if ( cuisine::keys::down(i) ) // && i != 13)
                {
                    
                    // to do : move up
                    byte input =  i & 0b0000000011;
                    byte parameter = i & 0b11111100;
                    parameter = parameter >> 2;
                    

                    cuisine::led::setMask(0xFFFF, cuisine::led::Off);
                    if(parameter == 0 && _trackPointer->getType() != NOTE) 
                    { 
                        if(_trackPointer->getType() == MOD)
                        {
                            byte CC = _trackPointer->getCC();
                            CC = cuisine::pot::read(CC, VALUE_SENSITIVITY, 0, 127);
                            _trackPointer->setCC(CC);
                            cuisine::display::number(CC * 10);
                        }
                        else if(_trackPointer->getType() == DRUM)
                        {
                            byte note = _trackPointer->getNoteSelected();
                            note = cuisine::pot::read(note, VALUE_SENSITIVITY, 0, 127);
                            _trackPointer->setNoteSelected(note);
                            if(!cuisine::keys::command(SHIFT)) { cuisine::display::number(note * 10); }
                            else 
                            { 
                                cuisine::led::setMask(KEYBOARD, cuisine::led::Yellow);
                                byte octave = note / 12;
                                note = note % 12;
                                cuisine::led::set(note, cuisine::led::Red);
                                cuisine::display::octave(octave);
                            }
                        }
                    }
                    
                    //clock input set
                    else if (parameter == 1) 
                    {

                        input = _trackPointer->getClockInput();
                        byte value = cuisine::pot::read(input, DEFAULT_SENSITIVITY, 0, 27);
                        if(cuisine::keys::command(SHIFT))
                        {
                            for(int j = 0; j < TRACK_NUMBER; j++)
                            {
                                _track[j].clockInputSet(value);
                            }
                        }
                        _trackPointer->clockInputSet(value);
                        byte color;
                        
                        if (value < 12) { color = cuisine::led::Red;}
                        else {color = cuisine::led::Yellow; }
                        if(value > 15) { value -= 16; }
                        cuisine::led::set(value, color);
                    }
                    // reset input set
                    else if (parameter == 2)
                    {
                        input = _trackPointer->getResetInput();
                        byte color;
                        byte value;
                        
                        value = cuisine::pot::read(input, DEFAULT_SENSITIVITY, 0, 27);
                        value = _trackPointer->getResetSource() == TRACK_FIRST_STEP ? CLAMP(value, 0, 11) : value;
                        color = value < 12 ? cuisine::led::Red : cuisine::led::Yellow;
                        cuisine::led::set(value > 15 ? value - 16 : value, color);
                        
                        if(cuisine::keys::command(SHIFT))
                        {
                            for(int j = 0; j < TRACK_NUMBER; j++)
                            {
                                _track[j].resetInputSet(value);
                            }
                        }
                        else
                        {
                            _trackPointer->resetInputSet(value);
                        }
                        
                    }
                    // Output :

                    //MIDI
                    else if (i == 12)
                    {
                        if (cuisine::keys::command(SHIFT))
                        {
                            int value = (_trackPointer->getMIDIchannel() >> 8) & 0xFF;
                            value = cuisine::pot::read(value, DEFAULT_SENSITIVITY, 0, 16);
                            _trackPointer->setMIDIinChannel(value);
                            cuisine::led::set((value - 1), cuisine::led::Red);
                        }
                        else
                        {
                            int value = _trackPointer->getMIDIoutChannel();
                            value = cuisine::pot::read(value, DEFAULT_SENSITIVITY, 0, 16);
                            _trackPointer->setMIDIoutChannel(value);
                            cuisine::led::set((value - 1), cuisine::led::Green);
                        }
                    }
                    // //Play Mode
                    else if (i == 13)
                    {
                        byte value = _trackPointer->getPlaySource();
                        // Serial.println(value);
                        value = cuisine::pot::read((value), DEFAULT_SENSITIVITY, 0, 28);
                        _trackPointer->setPlaySource(value);
                        byte color = value < 13 ? cuisine::led::Red : cuisine::led::Yellow;
                        value = value > 16 ? value - 16 : value;
                        cuisine::led::set((value - 1), color);
                    }
                    // CV
                    else if (i == 14)
                    {

                        int value = _trackPointer->getCVOut();
                        value = cuisine::pot::read((value + 1), DEFAULT_SENSITIVITY, 0, 4);
                        _trackPointer->setCVout(value);
                        cuisine::led::set( (value - 1), cuisine::led::Yellow);
                    }
                    // gate
                     else if (i == 15)
                    {
                        int value = _trackPointer->getGateOut();
                        value = (value == 255) ? -1 : value;
                        value = cuisine::pot::read((value), DEFAULT_SENSITIVITY, -1, 7);
                        value = (value == -1) ? 255 : value;
                        cuisine::led::set( (value), cuisine::led::Red);
                        _trackPointer->setGateout(value);
                    }
                }
                // save preset if any change
                if (cuisine::keys::released(i) || cuisine::keys::commandReleased(SHIFT))
                {
                    if(cuisine::keys::command(SHIFT))
                    {
                        for(int j = 0; j < TRACK_NUMBER; j++)
                        {
                            _track[j].savePreset();
                        }
                    }
                    else
                    {
                        _trackPointer->savePreset();
                    }
                }
            }
            break;
        }
        case TRACK_SETTINGS_2 :
        {
            OperatorSettings ops = _trackPointer->getOperators();
            
            // Visuals for Operators (Steps 0-7)
            for (int i = 0; i < 8; i++) {
                uint8_t source = (i < 4) ? ops.gateSources[i] : ops.cvSources[i - 4];
                if (source == 255) {
                    cuisine::led::set(i, cuisine::led::Off); // Slot is cuisine::led::Off
                } else if (i < 4)
                {
                    cuisine::led::set(i, cuisine::led::Red); // Slot is active (dimmed)
                }
                else
                {
                    cuisine::led::set(i, cuisine::led::Yellow); // Slot is active (dimmed)
                }
            }

            // cuisine::led::set(8, _trackPointer->getTranspose() ? cuisine::led::Green : cuisine::led::Off);
            cuisine::led::set(11, cuisine::led::Off);
            cuisine::led::set(15, _trackPointer->getAccentOut() < 255 ? cuisine::led::Yellow : cuisine::led::Off);
            cuisine::led::set(12, _trackPointer->preset.patternSource  ? cuisine::led::Yellow : cuisine::led::Off);

            for (int i = 0; i < 16; i++)
            {
                // Operator Source Selection (Steps 0-7)
                if (i < 8 && cuisine::keys::down(i))
                {
                    uint8_t currentSource = (i < 4) ? ops.gateSources[i] : ops.cvSources[i - 4];
                    int value = (currentSource == 255) ? -1 : currentSource;
                    if(cuisine::pot::moving())
                    {
                        // Hold Step + Turn Pot to select source (0-11: Tracks, 12-15: Ext Gates, -1: cuisine::led::Off)
                        value = cuisine::pot::read(value, DEFAULT_SENSITIVITY, -1, (i < 4 ? 15 : 11));
                        
                        uint8_t newSource = (value == -1) ? 255 : (uint8_t)value;
                        if (newSource != currentSource) {
                            _trackPointer->setOperatorSource(i, newSource);
                        }
                    }
                    // Visual feedback for selection
                    cuisine::led::setMask(0xFFFF, cuisine::led::Off);
                    cuisine::led::set(i, cuisine::led::Yellow); // Current slot bright
                    if (value != -1) {
                        cuisine::led::set(value, (value < 12 ? cuisine::led::Green : cuisine::led::Red)); // Selected source
                    }
                }
                // else if ( cuisine::keys::pressed(i) && potClass::readState(true ))
                // {
                //     if(i == 8) {_trackPointer->setTranspose(); }
                // }
                // set accent output
                else if(cuisine::keys::down(15))
                {
                    int value = _trackPointer->getAccentOut();
                    if(potClass::readState(true ))
                    {
                        value = (value == 255) ? -1 : value;
                        value = cuisine::pot::read((value), DEFAULT_SENSITIVITY, -1, 7);
                        value = (value == -1) ? 255 : value;
                        _trackPointer->setAccentOut(value);
                    }
                    cuisine::led::setMask(0xFFFF, cuisine::led::Off);
                    cuisine::led::set( (value), cuisine::led::Yellow);
                }
                // pattern source set
                else if (cuisine::keys::down(12))
                {
                    int value = _trackPointer->preset.patternSource;
                    if(potClass::readState(true ))
                    {
                        value = cuisine::pot::read((value), DEFAULT_SENSITIVITY, 0, 12);
                        
                    }
                    _trackPointer->preset.patternSource = value;
                    cuisine::led::setMask(0xFFFF, cuisine::led::Off);
                    cuisine::led::set( (value - 1), cuisine::led::Yellow);
                    // set for all tracks
                    if(cuisine::keys::command(SHIFT))
                    {
                        for(int j = 0; j < TRACK_NUMBER; j++)
                        {
                            _track[j].preset.patternSource = value;
                        }
                    }
                }
                else if (cuisine::keys::released(i) || cuisine::keys::commandReleased(SHIFT))
                {
                    if(cuisine::keys::command(SHIFT))
                    {
                        for(int j = 0; j < TRACK_NUMBER; j++)
                        {
                            _track[j].savePreset();
                        }
                    }
                    else
                    {
                        _trackPointer->savePreset();
                    }
                }
            }
            break;
        }
        case MASTER_SETTINGS :
        {
            //Master clock set
            for (int i = 0; i < 16; i++)
            {
                
                if ( cuisine::keys::down(i) )
                {
                    if (i == 0)
                    {
                        byte value = clockClass::getClock();
                        // Serial.println(value);
                        value = cuisine::pot::read(value, DEFAULT_SENSITIVITY, 0, 1);
                        clockClass::setClock(value);

                        cuisine::led::set(value, cuisine::led::Green);
                    }
                }
                else if (i == 0) {cuisine::led::set(i, cuisine::led::Green);}
            }
            
            break;
        }
        case MASTER_RESET :
        {
            trackClass::resetStruct stepReset = trackClass::unpackStepReset(_trackPointer->getResetInput());
            byte color;
            if(cuisine::keys::commandPressed(SHIFT)) 
            { 
                if(_masterResetMult) { _page = TRACK_SETTINGS;}
                _masterResetMult = !_masterResetMult;
            }
            if(_masterResetMult)
            {
                color = cuisine::led::Red;
                stepReset.step = cuisine::pot::read(stepReset.step, DEFAULT_SENSITIVITY, 0, 15);
                cuisine::led::set(stepReset.step, color);
            }
            else
            {
                color = cuisine::led::Yellow;
                stepReset.multiplier = cuisine::pot::read(stepReset.multiplier, DEFAULT_SENSITIVITY, 0, 15);
                cuisine::led::set(stepReset.multiplier, color);
            }

            byte value = trackClass::packStepReset(stepReset);
            _trackPointer->resetInputSet(value);
            
            break;
        }
        case PATTERN :
        {

            cuisine::led::set(_trackPointer->getPatternIndex(), cuisine::led::Green);

            byte selection = cuisine::keys::steps() >> 12;
            byte selectionTrigOff = cuisine::keys::stepsReleased() >> 12;

            for (int i = 0; i < 16; i++)
            {
                //selection set
                if (selection)
                {
                    for (int y = 0; y < 4; y++)
                    {
                        if (bitRead(selection, y) ) // || trackClass::getMuteState(y) ) 
                        { 
                            if (i < TRACK_NUMBER)
                            {
                                if ( getSelection(y, i) ) { cuisine::led::set(i, cuisine::led::Yellow, true); }
                                if ( cuisine::keys::released(i) && bitRead(selection, y) ) { interfaceClass::setSelection(y, i); }
                            }
                        }
                    }
                }
                
                // 1 track
                if (i < PATTERN_NUMBER)
                {
                    if(!isSelectionActive())
                    {
                        //save and load active track
                        if(cuisine::keys::command(SHIFT) && !cuisine::keys::arm())
                        {
                            if (saveHoldProgressUI(_sinceSave, i, cuisine::led::Red))
                            {
                                _trackPointer->savePattern(i);
                                _trackPointer->selectPattern(i);
                            }
                        }
                        else if (cuisine::keys::pressed(i)  && !selection)
                        {
                            _trackPointer->selectPattern(i);
                            
                            
                        }
                    }
                    // save and load selected track
                    else
                    {
                        
                        if(cuisine::keys::command(SHIFT))
                        {
                            if (saveHoldProgressUI(_sinceSave, i, cuisine::led::Red))
                            {
                                saveTrackSelection(i);
                            }
                        }
                        
                        // select
                        else if (cuisine::keys::pressed(i)  && !selection)
                        {
                            for(int y = 0; y < 4; y++)
                            {
                                if(getSelectionState(y))
                                {
                                    for(int a = 0; a < TRACK_NUMBER; a++)
                                    {
                                        if(getSelection(y, a))
                                        {
                                            _track[a].selectPattern(i);
                                        }
                                    }
                                }
                               
                            }
                        }
                    }                    
                }
            }
            //display active selection group
            for (int y = 0; y < 4; y++)
            {
                if (getSelectionState(y) ) { cuisine::led::set( (y + 12), cuisine::led::Yellow, true); }
                //selection activate
                if (bitRead(selectionTrigOff, y) && !cuisine::keys::command(SHIFT) ) { setSelectionState(y); }
            }
            break;
        }
        case BANK :
        {
            cuisine::led::set(trackClass::bankIndex, cuisine::led::Green);
            for (int i = 0; i < 16; i++)
            {
                bool saveReady = saveHoldProgressUI(_sinceSave, i, cuisine::led::Green);
                if ( cuisine::keys::command(SHIFT) )
                {
                    if(saveReady)
                    {
                        _sinceSave = 0;
                        
                        for(int y = 0; y < TRACK_NUMBER; y++)
                        {
                            _track[y].savePattern();
                            _track[y].savePreset();
                        }
                        trackClass::saveBank(i);
                    }
                }
                // load
                else if (saveReady)
                {
                    cuisine::Millis sinceLoad = 0;
                    trackClass::loadBank(i);
                    
                    for(int y = 0; y < TRACK_NUMBER; y++)
                    {
                        _track[y].selectPattern(0);
                        _track[y].init();
                        
                    }
                }
            }

            
            break;
        }
        // case PATTERN_CHAIN :
        // {
        //     for (int i = 0; i < 16; i++)
        //     {
        //         if (cuisine::keys::pressed(i))
        //         {
        //             // _memory.loadPattern(_trackPointer, i);
        //         }
        //     }
            
        //     break;
        // }
        case DIV_MULT :
        {

            byte division = _trackPointer->getTimeDivision();
            if(cuisine::pot::moving())
            {
                division = cuisine::pot::read(division, DEFAULT_SENSITIVITY, 0, 15);
                _trackPointer->setTimeDivision(division);
            }

            if(cuisine::keys::stepsPressed() || cuisine::keys::stepsReleased())
            {
                for (int i = 0; i < 16; i++)
                {
                    if( cuisine::keys::pressed(i) ) 
                    { 
                        _trackPointer->setTimeDivision(i);
                        // live play with div mult
                        if(_trackPointer->getPlayMode() == GATE)
                        {
                            if(cuisine::keys::pressed(i)) {_MIDInoteOnHandle(0, _trackPointer->getID(), 127);}
                            
                        }
                    }
                    else if (cuisine::keys::released(i) && !cuisine::keys::command(SHIFT)  && (_trackPointer->getPlayMode() == GATE)) {_MIDInoteOffHandle(0, _trackPointer->getID(), 127);}
                }
            }
            
            displayVariation(_trackPointer, _variation, true);
            if ( _trackPointer->getTimeType() ) {  cuisine::led::set(division, cuisine::led::Green); }
            else {  cuisine::led::set(division, cuisine::led::Yellow);}
            
            break;
        }
        case CLOCK_SHIFT :
        {
            #ifndef OFFSET
            _page = DIV_MULT;
            break;

            #endif
            int offset = _trackPointer->getClockOffset();
            unsigned int ppqn = _trackPointer->getPPQN() - 1;
            
            offset = cuisine::pot::read(offset, DEFAULT_SENSITIVITY, -ppqn, ppqn);
            int value = map(abs(offset), 0, ppqn, 0, 7);
            if (offset < 0) { value = 0 - value; }
            _trackPointer->setClockOffset(offset);
            cuisine::display::offset(value, true);
            break;
        }
        case LOOP :
        {
            if(cuisine::keys::steps())
            {
                static bool lastTrig;
                for (int i = 0; i < 16; i++)
                {
                    if(cuisine::keys::released(i))
                    {
                        lastTrig = true;
                        divMult._firstLast = false;
                    }
                    if( cuisine::keys::pressed(i)) //|| (lastTrig && cuisine::keys::down(i))) 
                    { 
                        if ( !divMult._firstLast )
                        {
                            lastTrig = false;
                            if (_overide)
                            {
                                _trackPointer->setOverrideFirst(i, _patternPage);
                                _rec ? _trackPointer->setLoopWriteFlag(true) : void(0);
                                // _trackPointer->setOverridIndex(i);
                            }
                            else
                            {
                                _trackPointer->setFirstStep(i);
                            }
                            // _trackPointer->setIndex(i);
                            divMult._firstLast = true;
                        }
                        else // if ( i != _trackPointer->getFirstStep() )
                        {
                            _overide ? _trackPointer->setOverrideLast(i) : _trackPointer->setLastStep(i);
                        }
                    }
                    // else { _trackPointer->setLastStep(cuisine::pot::read(_trackPointer->getLastStep(), DEFAULT_SENSITIVITY, 0, 15) ); }
                }
            }
            else 
            { 
                divMult._firstLast = false;
                _trackPointer->resetOverride();
            }
            
            displayVariation(_trackPointer, _variation, true);
            cuisine::led::set(_trackPointer->getFirstStep(), cuisine::led::Green);
            cuisine::led::set(_trackPointer->getLastStep(), cuisine::led::Yellow);
            break;
        }
        case JUMP :
        {
            if(cuisine::keys::steps())
            {
                for(int i = 0; i < 16; i++)
                {
                    if(cuisine::keys::pressed(i))
                    {
                        if(_overide)
                        {
                            _trackPointer->setOverrideJump(i);
                        }
                        _trackPointer->setIndexPtrValue(i, _patternPage);
                        
                    }
                }
            }
            else {_trackPointer->resetOverride();}
            displayVariation(_trackPointer, _variation, true);
            break;
        }

    }

    // Function keys :

    //copy :
    if(cuisine::keys::commands() == COPY)
    {
        switch (_page)
        {
            case TRACK:
            {
                // // copy track settings
                // _trackPointer->copyTrack();
                // // Serial.println("copy track");
                break;
            }
            case STEP_EDIT:
            case STEP_MUTE:
            {
                uint16_t trig = cuisine::keys::stepsPressed();
                while (trig)
                {
                    uint8_t i = nextSetBit(trig);
                    // step
                    _trackPointer->copyPattern();
                    // _copiedTrack = _trackPointer->getID();
                    if ( !copy._firstLast )
                    {
                        copy._first = i | _patternPage << 4;
                        copy._firstLast = true;
                    }
                    else
                    {
                        copy._last = i  | _patternPage << 4;
                        copy._firstLast = false;
                    }
                }
            }
        }

        

    }
    //paste :
    if(cuisine::keys::commands() == PASTE)
    {
        switch(_page)
        {
            case TRACK:
            {
                // // paste track settings
                // if(cuisine::keys::stepsPressed())
                // {
                //     for(int i = 0; i < TRACK_NUMBER; i++)
                //     {
                //         if( cuisine::keys::pressed(i) ) 
                //         {
                //             _trackPointer->pasteTrack(i);
                //         }
                //     }
                //     // Serial.println("paste track");
                // }
                
                break;
            }
            case STEP_EDIT:
            case STEP_MUTE:
            {
                // paste pattern
                uint16_t trig = cuisine::keys::stepsPressed();
                while (trig)
                {
                    uint8_t i = nextSetBit(trig);
                    byte destination = _trackPointer->indexPack(i, _patternPage);
                    _trackPointer->pastePattern(copy._first, copy._last, destination);
                }
                copy._firstLast = false;
            }
        }
        uint16_t trigOff = cuisine::keys::stepsReleased();
        while (trigOff)
        {
            uint8_t i = nextSetBit(trigOff);
            byte destination = _trackPointer->indexPack(i, _patternPage);
            _trackPointer->pastePattern(copy._first, copy._last, destination);
        }
        copy._firstLast = false;
    }
    
    // quick save/load/clear
    if(cuisine::keys::command(SHIFT))
    {
        
        //quickSave
        if(cuisine::keys::commandPressed(COPY))
        {

            _trackPointer->quicksave();
        }

        //quickLoad
        else if(cuisine::keys::commandPressed(PASTE))
        {
            _trackPointer->loadPattern();
        }

        // quickClear
        else if(cuisine::keys::command(CLEAR))
        {
            if(cuisine::keys::commandPressed(CLEAR)) { _sinceSave = 0; }
            if(_sinceSave >= SAVE_TIME) { _quickClearArmed = true; }
            byte color = cuisine::led::Red;
            byte display = saveProgressLeds(_sinceSave);
            cuisine::display::bar(display, color);
        }
    }

    // Fire quick clear on CLEAR release (independent of SHIFT) so releasing both keys works
    if(_quickClearArmed && cuisine::keys::commandReleased(CLEAR))
    {
        _trackPointer->writeAllStep(false);
        _trackPointer->writeAllAccent(false);
        _trackPointer->setSize(0);
        _quickClearArmed = false;
        _sinceSave = 0;
    }
    
    interfaceClass::outputUpdate();
    DACclass::update();
    gateOutClass::update();
    // set tap page flag
    tap::setTapPageFlag(_page == BPM_PAGE);
    
#if ENABLE_DIAGNOSIS
    while(_diagnosis)
    {
        keyboardClass::update();
        _updateInput();
        clockClass::masterUpdate();
        cuisine::midi::poll();
        _updateTransport();
        potClass::updateReading();
        gateInClass::update();
        updateVariation();
        cuisine::led::flush(8);
        cuisine::led::setMask(0xFFFF, cuisine::led::Off);
        _pot = cuisine::pot::read(_pot, DEFAULT_SENSITIVITY, 0, 4095);
        int display = (int)((_pot * 16L) / 4095) - 1;
        cuisine::display::bar(display, cuisine::led::Yellow);
        DACclass::update();

        // Switch test: steps → gateOut + yellow/MIDI; commands → green
        uint16_t steps = cuisine::keys::steps();
        for (byte i = 0; i < 16; i++)
        {
            bool on = bitRead(steps, i);
            cuisine::gate_out::set(i, on);
            if (on)
            {
                cuisine::led::set(i, cuisine::led::Yellow);
                cuisine::midi::sendNoteOn(i, 127, 1);
            }
            if (keyboardClass::commandReadNo(i))
            {
                cuisine::led::set(i, cuisine::led::Green);
            }
        }
        // gateIn & CV out test (overwrites gateOut 0..3)
        for (byte i = 0; i < 4; i++)
        {
            bool value = cuisine::gate_in::high(i);
            cuisine::gate_out::set(i, value);
            if (value)
            {
                cuisine::led::set(i, cuisine::led::Yellow);
            }
            cuisine::cv::setRaw(i, _pot);
        }
        gateOutClass::update();
        DACclass::update();
    }
#endif
}
void interfaceClass::_updateTransport()
{
    //Sub page
    byte* pagePointer;
    byte maxPage;
    byte* activePage;
    byte trackSize = _trackPointer->getSize();
    if(_page == EUCLID) { pagePointer = &_trackPointer->euclid.page; maxPage = 3; activePage = &maxPage; }
    else { pagePointer = &_patternPage; maxPage = 4; activePage = &trackSize; }

    unsigned int commandTrig = cuisine::keys::commandsPressed();

    if (commandTrig == PLAY)
    {
        if (clockClass::getMasterTransportFlag()) { clockClass::stopHandler(); cuisine::midi::sendStop(); }
        else 
        { 
            clockClass::startHandler(); 
            cuisine::midi::sendStart();
            // for(int i = 0; i < TRACK_NUMBER; i++) { _track[i].updateGateLength(); }
        }
        
    }
    else if (commandTrig == REC)
    {
        _rec = !_rec;
        _recIndex = _trackPointer->getOrigin().step;
        if (!_rec)
            trackClass::clearRecMuteAll();
    }
    else if (commandTrig == PAGE
        && cuisine::keys::command(SHIFT)
        && _page != EUCLID) 
    { 
        
        byte size = _trackPointer->getSize() + 1;
        if (size >= VARIATION_NUMBER ) { size = 0;}
        _trackPointer->setSize(size);
    }
    else if (commandTrig == PAGE)
    { 
        (*pagePointer)++;
        if (*pagePointer >= maxPage) { *pagePointer = 0;}

        if (_page != EUCLID && _chaseFlag && _oldChaseFlag) {_chaseFlag = false;}
    }
    
    //overide mode
    _oldOveride = _overide;
    _overide = cuisine::keys::arm();
    if(!_overide && _oldOveride)
    {
        _trackPointer->arp.reset();
        for (int i = 0; i < TRACK_NUMBER; i++)
        {
            eventManagerClass::clearLive(i);
        }
        trackClass::setMuteStateAll(_armedMute);
        _armedMute = 0;
    }
    
    bool overrideLED = _armedMute ? cuisine::led::blink() : _overide;
    cuisine::led::transport(OVERIDE_LED, overrideLED);
    cuisine::led::transport(PLAY_LED, clockClass::getMasterTransportFlag());
    cuisine::led::transport(REC_LED, _rec);
    bool currentPageDisplay = (_page == EUCLID && !_trackPointer->getTransport()) ? true : false;
    cuisine::led::page(*activePage, *pagePointer, maxPage, _trackPointer->getIndexPtr().page, currentPageDisplay);
    
}
void interfaceClass::_updateInput()
{
    bool shift = cuisine::keys::command(SHIFT);
    //if no popup from pot and step
    if (!_pagePotSelectFlag && !_pageStepSelectFlag)
    {
        unsigned int commandTrigRaw = cuisine::keys::commandsPressed();
        unsigned int commandTrig    = commandTrigRaw;

        unsigned int gate = cuisine::keys::commands();
        
        if (commandTrigRaw)
        {
            if(_doubleTapCommand == true && _sinceCommand < 250 && (commandTrig &= DOUBLE_TAP_MASK)) 
            { 
                if(commandTrig == _lastTap)
                {
                    // Quick command
                    if(!cuisine::keys::steps())
                    {
                        switch(commandTrigRaw)
                        {
                            case SHIFT :
                                { _page = BPM_PAGE; break; }
                            case TRACK :
                                { _page = TRACK_SETTINGS; break; }
                            case STEP_EDIT :
                                { _page = TRACK_SETTINGS_2; break;}
                            case PATTERN :
                                { _page = MASTER_SETTINGS; break;}
                            case COPY :
                            { 
                                if(_page == STEP_EDIT || _page == STEP_MUTE)
                                {
                                    _trackPointer->copyPattern();
                                    copy._first = 0 | _patternPage << 4;
                                    copy._last = 16  | _patternPage << 4;
                                }
                                break;
                            }
                            case PASTE :
                            { 
                                if(_page == STEP_EDIT || _page == STEP_MUTE)
                                {
                                    byte destination = _trackPointer->indexPack(0, _patternPage);
                                    _trackPointer->pastePattern(copy._first, copy._last, destination);
                                }
                                break;
                            }
                            case PAGE :
                            {
                                if(shift)
                                {
                                    // copy
                                    _trackPointer->copyPattern();
                                    byte source = _patternPage;
                                    // if(patternSource >= 4) { patternSource = 3;}
                                    

                                    copy._first = 0 | source << 4;
                                    copy._last = 15  | source << 4;
                                    // paste
                                    byte destination = source + 1;
                                    if(destination >= 4) { destination = 0;}

                                    
                                    _patternPage = destination;
                                    destination = _trackPointer->indexPack(0, destination);
                                    _trackPointer->pastePattern(copy._first, copy._last, destination);
                                    _trackPointer->setSize(_patternPage - 1);
                                    

                                }
                                else // chase flage On
                                {
                                    _chaseFlag = true;
                                }
                                
                            }
                            case EUCLID :
                            {
                                _trackPointer->commitEuclid();
                            }
                            
                        }
                    }
                    else if (commandTrigRaw == SHIFT && _overide)
                    {
                        switch(_page)
                        {
                            // arm mute group
                            case TRACK :
                            {
                                for(int i = 11; i < 16; i++)
                                {
                                    if(cuisine::keys::down(i))
                                    {
                                        bitWrite(_armedMute, i - 12, !bitRead(_armedMute, i - 12) );
                                    }
                                }
                            }
                        }
                    }
                    
                }
                // _page = commandTrig == TRACK ? TRACK_SETTINGS : MASTER_SETTINGS;
                // Serial.println("double tap");
                _doubleTapCommand = false; 
                return;
            }
            else { _doubleTapCommand = true; _lastTap = commandTrig; }

            _pageKeySelectFlag = true;
            _oldPage = _page;
            if(commandTrig & ~COMMANDS_MASK)
            {
                gate &= ~COMMANDS_MASK;
                //if pattern size don't hold page
                if (gate == KEYBOARD_PAGE) { _page = pgm_read_word(&_typeToPageTable[0][_trackPointer->getType()][shift]); }
                else if (gate == PAGE) 
                {
                    _page = cuisine::keys::commands();
                    _sinceCommand = 250; // <- ??
                    return;
                }
                else { _page = cuisine::keys::commands();}
            }
            
             
            _sinceCommand = 0; 
                  
        }
        else if ( _pageKeySelectFlag && !(gate & ~COMMANDS_MASK)) // if gate off AND long gate get back on gate off
        {
            if ((_sinceCommand > 250)) { _page = _oldPage; }           
            _sinceCommand = 0; 
            _pageKeySelectFlag = false;         
        }   
    }
    // if no pop up from key and step
    if (!_pageKeySelectFlag && !_pageStepSelectFlag)
    {
        bool pot = cuisine::pot::moving();
        //pot pop up 
        if (pot) 
        { 
            _sincePotOff = 0; 
            if (_page == STEP_EDIT) // to optimize
            {
                _oldPage = _page;
                _page = pgm_read_word(&_typeToPageTable[1][_trackPointer->getType()][shift]);
                _pagePotSelectFlag = true;
            }
            else if (_page == TRACK && shift)
            {
                // _oldPage = _page;
                // _page = BPM_PAGE;
                // _pagePotSelectFlag = true;
            }
            else if (_page == ACCENT)
            {
                _oldPage = _page;
                _page = cuisine::keys::command(SHIFT) ? HIGH_VELOCITY : LOW_VELOCITY;
                _pagePotSelectFlag = true;
            }
            // _pagePotSelectFlag = true; // <- to check
        }
        
        else if ( _pagePotSelectFlag && !(pot) && (_sincePotOff > POP_UP_TIME) && !shift)  // if gate off AND long gate get back on gate off
        {
            _page = _oldPage;
            _pagePotSelectFlag = false;
        }
    }
    //if no pop up from pot and key
    if (!_pagePotSelectFlag && !_pageKeySelectFlag)
    {
        unsigned int trig = cuisine::keys::stepsPressed();        
        unsigned int gate = cuisine::keys::steps();

        if (trig && (_page == STEP_EDIT)) { _sinceStep = 0; }
        else if (gate && (_page == STEP_EDIT) )
        {
            if (_sinceStep > (POP_UP_TIME * 4))
            {
                _oldPage = _page;
                _page = pgm_read_word(&_typeToPageTable[1][_trackPointer->getType()][shift]);
                _pageStepSelectFlag = true;
                _popUp = true;
            }
        }
        else if ( !(gate) && _pageStepSelectFlag )
        {
            _stepWriteFlag = true;
            _page = _oldPage;       
            _sinceStep = 0; 
            _pageStepSelectFlag = false;       
        }
    }  
}
static void interfaceClass::updateVariation() // <--
{
    indexStruct index = {0, _patternPage}; //_trackPointer->getIndex();
    _variation.step = _trackPointer->readStepPage(index);
    _variation.accent = _trackPointer->readAccentPage(index);
    _variation.page = _patternPage;
}
static noteStruct interfaceClass::_midiToQuantizedKeyboardNote(byte midiNote)
{
    noteStruct out = MIDIpack(midiNote);
    unsigned int scale = trackClass::getScale();
    if (scale == 0) { return out; }

    int pitch = midiNote % 12;
    int octave = midiNote / 12;
    int selectedPitch = pitch;
    int octaveAdjust = 0;
    bool found = false;

    if (scale & (1 << pitch))
    {
        found = true;
    }

    for (int offset = 1; offset < 12 && !found; offset++)
    {
        int upPitch = pitch + offset;
        int upOctaveAdjust = 0;
        if (upPitch > 11)
        {
            upPitch -= 12;
            upOctaveAdjust = 1;
        }
        if (scale & (1 << upPitch))
        {
            selectedPitch = upPitch;
            octaveAdjust = upOctaveAdjust;
            found = true;
            break;
        }

        int downPitch = pitch - offset;
        int downOctaveAdjust = 0;
        if (downPitch < 0)
        {
            downPitch += 12;
            downOctaveAdjust = -1;
        }
        if (scale & (1 << downPitch))
        {
            selectedPitch = downPitch;
            octaveAdjust = downOctaveAdjust;
            found = true;
            break;
        }
    }

    int degree = 0;
    for (int i = 0; i < selectedPitch; i++)
    {
        if (scale & (1 << i)) { degree++; }
    }
    out.note = degree;
    out.octave = CLAMP(octave - 4 + octaveAdjust, -4, 4);
    return out;
}
static void interfaceClass::_handleExternalNoteOn(trackClass* track, byte note, byte velocity, bool allowKeyboardTransport)
{
    if (!track) { return; }

    byte type = track->getType();
    if (velocity)
    {
        if (allowKeyboardTransport && _page == SCALE) { trackClass::setScale(note % 12); }
        noteStruct quantNote = _midiToQuantizedKeyboardNote(note);

        if (!_overide)
        {
            if (type == NOTE)
            {
                // Keep keyboard transport for NOTE tracks
                if (allowKeyboardTransport) { _keyboardTransport(true); }
                // Hardware output handled by dispatch()
            }
            // DRUM and MOD hardware output handled by dispatch()
        }

        if (type == NOTE)
        {
            // Use eventManager to populate liveInput for recording
            quantNote.velocity = velocity;
            quantNote.accent = velocity > track->getLowVelocity();

            if (track == _trackPointer)
            {
                _notePerformed = quantNote;
                _noteSet = quantNote;
            }
            eventManagerClass::setLiveNote(track, quantNote, true);
        }
        else if (type == DRUM || type == MOD)
        {
            byte value = velocity << 1;
            trackClass* previousTrack = _trackPointer;
            _trackPointer = track;
            eventManagerClass::setLiveValue(track, value, true);
            _trackPointer = previousTrack;
        }
    }
    else if (type == NOTE)
    {
        _handleExternalNoteOff(track, note, velocity, allowKeyboardTransport);
    }
}
static void interfaceClass::_handleExternalNoteOff(trackClass* track, byte note, byte velocity, bool allowKeyboardTransport)
{
    if (!track) { return; }

    byte type = track->getType();
    if (type == NOTE && !cuisine::keys::command(SHIFT))
    {
        noteStruct quantNote = _midiToQuantizedKeyboardNote(note);
        // byte outputNote = _getMIDInote(quantNote, track->getTranspose(), track);
        
        // Use eventManager to populate liveInput for recording
        eventManagerClass::setLiveNote(track, quantNote, false);
        
        if (!_overide)
        {
            if (!track->arp.getNumKeysPressed())
            {
                if (allowKeyboardTransport) { _keyboardTransport(false); }
            }
        }
    }
    else if (type == DRUM || type == MOD)
    {

        trackClass* previousTrack = _trackPointer;
        _trackPointer = track;
        eventManagerClass::setLiveValue(track, 0, false);
        _trackPointer = previousTrack;
    }
}
static void interfaceClass::_MIDInoteOnHandle(byte channel, byte note, byte velocity) 
{
#if ENABLE_DIAGNOSIS
    if(_diagnosis) 
    {
        cuisine::led::setMask(0xFFFF, cuisine::led::Green);
        cuisine::led::flush(255);
        delay(100);
        return;
    }
#endif
    // channel == 0 for transport control / playMode
    if(!channel)
    {
        if((_overide) && _track[note].isIndependentPlay() && !_track[note].getTransport()) 
        { 
            _track[note].setTransport(true);      
            return;
        }

    }
    if (channel == 16)
    {
        if (note < TRACK_NUMBER)
        {
            if ((_overide) && _track[note].isIndependentPlay() && !_track[note].getTransport())
            {
                _track[note].setTransport(true);
            }
            return;
        }

        _handleExternalNoteOn(_trackPointer, note, velocity, true);
        return;
    }

    if (channel >= 1 && channel <= 15)
    {
        for (int i = 0; i < TRACK_NUMBER; i++)
        {
            if (_track[i].getMIDIinChannel() == channel)
            {
                _handleExternalNoteOn(&_track[i], note, velocity, i == _selectedTrack);
            }
        }
    }
}
static void interfaceClass::_MIDInoteOffHandle(byte channel, byte note, byte velocity) 
{
    if(!channel)
    {
        if(_overide && _track[note].getPlayMode() == GATE && _track[note].getTransport()) { _track[note].setTransport(false); }
    }
    if (channel == 16)
    {
        if (note < TRACK_NUMBER)
        {
            if (_overide && _track[note].getPlayMode() == GATE && _track[note].getTransport())
            {
                _track[note].setTransport(false);
            }
            return;
        }

        _handleExternalNoteOff(_trackPointer, note, velocity, true);
        return;
    }

    if (channel >= 1 && channel <= 15)
    {
        for (int i = 0; i < TRACK_NUMBER; i++)
        {
            if (_track[i].getMIDIinChannel() == channel)
            {
                _handleExternalNoteOff(&_track[i], note, velocity, i == _selectedTrack);
            }
        }
    }
}

static noteStruct interfaceClass::MIDIpack(byte note)
{
    noteStruct out = {0};
    out.note = note % 12;
    out.octave = (note / 12) - 4;
    return out;
}
static byte interfaceClass::MIDIunpack(noteStruct note)
{
    return note.note + ((note.octave + 4) * 12);
}

// process input so that is fit the scale
static noteStruct interfaceClass::_inputProcess(noteStruct input)
{
    noteStruct out = input;
    unsigned int scale = trackClass::getScale();
    if(scale == 0) { return out; }

    byte scaleCount = trackClass::getNoteScaleCount(false);
    if(out.note >= scaleCount) 
    {
        out.octave += (out.note / scaleCount);
        out.note %= scaleCount;
    }
    out.octave = CLAMP(out.octave, -4, 4);
    return out;
}
static byte interfaceClass::_getMIDInote(noteStruct input, bool transpose, trackClass* track)
{
    if(transpose)
    {
        noteStruct transposed = track ? track->getTransposeOffset() : trackClass::transposeNeutral();
        input.note += transposed.note;
        input.octave += (transposed.octave);
    }
    
    
    // process so that input fit the scale
    input = _inputProcess(input);
    // Serial.println(input.note);
    byte out = input.note + ((input.octave + 4) * 12);

    unsigned int scale = trackClass::getScale();
    // if no scale set, return the note
    if(scale == 0) { return out; }

    int count = 0;
    
    // else return the note from the scale
    for (int i = 0; i < 12; ++i) { 
        if ((scale & (1 << i)) != 0) {
            
            if (count == input.note) {
                out = i;
            }
            count++;
        }
    }
    out += input.octave * 12;
    return out;

}
static noteStruct interfaceClass::_keyboardToNote(noteStruct key) 
{
    noteStruct out = key;
    unsigned int scale = trackClass::getScale();
    if(scale == 0) { return out; }
    byte scaleCount = trackClass::getNoteScaleCount();
    if(out.note >= scaleCount) 
    {
        out.octave += (out.note / scaleCount);
        out.note %= scaleCount;
    }
    int count = 0;
    
    // if no scale set, return the note
    for (int i = 0; i < 12; ++i) { 
        if ((scale & (1 << i)) != 0) {
            
            if (count == key.note) {
                out.note = i;  // Return the position of the xth set bit (0-indexed)
            }
            count++;
        }
    }
    return out;
}
static void interfaceClass::saveUpdate()
{
    // debug();
    if (_trackSaved < TRACK_NUMBER && !_selectionSavedFlag) 
    {

        unsigned int trackSelection = 0;
        for(int y = 0; y < 4; y++)
        {
            if(getSelectionState(y))
            {
                for(int a = 0; a < TRACK_NUMBER; a++)
                {
                    if(getSelection(y, a))
                    {
                        bitWrite(trackSelection, a, 1);
                    }
                }
            }
            
        }

        if (trackSelection & (1 << _trackSaved)) 
        {
            _track[_trackSaved].savePattern(_targetPattern);
        }
        _trackSaved++;
        // If there are more tracks to save, schedule the next saving in the next loop iteration
        if (_trackSaved < TRACK_NUMBER) 
        {
        // Schedule the next saving in the next loop iteration
        return;
        }
        // Reset currentTrack for the next invocation
        _trackSaved = 0;
        _selectionSavedFlag = true;
    }
    
    
}
static void interfaceClass::loadUpdate()
{
    if (_trackSaved < TRACK_NUMBER && !_selectionLoadFlag && cycleDivider  >= 4) 
    {
        cycleDivider = 0;

        unsigned int trackSelection = 0;
        for(int y = 0; y < 4; y++)
        {
            if(getSelectionState(y))
            {
                for(int a = 0; a < TRACK_NUMBER; a++)
                {
                    if(getSelection(y, a))
                    {
                        bitWrite(trackSelection, a, 1);
                    }
                }
            }
            
        }

        if (trackSelection & (1 << _trackLoaded)) 
        {
            _track[_trackLoaded].loadPattern(_targetPattern);
            // debug();
        }
        _trackLoaded++;
        // If there are more tracks to save, schedule the next saving in the next loop iteration
        if (_trackLoaded < TRACK_NUMBER) 
        {
        // Schedule the next saving in the next loop iteration
        return;
        }
        // Reset currentTrack for the next invocation
        _trackLoaded = 0;
        _selectionLoadFlag = true;
    }
}
static void interfaceClass::saveTrackSelection(byte patternNumber)
{
    _targetPattern = patternNumber;
    _selectionSavedFlag = false;
}
// EXTERNAL target;
static void interfaceClass::loadTrackSelection(byte patternNumber)
{
    _targetPattern = patternNumber;
    _selectionLoadFlag = false;
    // debug();
}
static void interfaceClass::_updateLiveInputFromPot()
{
    for (int i = 0; i < TRACK_NUMBER; i++)
    {
        byte type = _track[i].getType();
        
        if (type == MOD)
        {
            bool moving = cuisine::pot::moving();
            bool isTarget = (_selectedTrack == i) && (_page & POT_PAGE_MASK && _page >> 15 == 0);
            bool isActive = false;

            if (isTarget)
            {
                if (_track[i].isEmpty() || _overide || (_rec && _track[i].getTrigStatus()))
                {
                    if (clockClass::getMasterTick() || !_track[i].getTransport())
                    {
                        if (moving) {
                            _track[i].modulation = cuisine::pot::read(_track[i].modulation, DEFAULT_SENSITIVITY, 0, 255);
                            isActive = true;
                        }
                    }
                    eventManagerClass::setLiveValue(&_track[i], _track[i].modulation, moving);
                }
                else if (_overide) 
                {
                    if (moving) {
                        _track[i].modulation = cuisine::pot::read(_track[i].modulation, DEFAULT_SENSITIVITY, 0, 255);
                        isActive = true;
                    }
                    eventManagerClass::setLiveValue(&_track[i], _track[i].modulation, moving);
                }
                else 
                {
                    eventManagerClass::setLiveValue(&_track[i], _track[i].modulation, isActive);
                }
                
            }
        }
        else if (type == NOTE)
        {
            if (_overide && _selectedTrack == i 
                && (_page & POT_PAGE_MASK && _page >> 15 == 0)
                && _page != NOTE_SELECT && _page != SCALE && _page != SHIFT)
            {
                bool moving = cuisine::pot::moving();
                if (moving)
                {
                    _track[i].modulation = cuisine::pot::read(_track[i].modulation, DEFAULT_SENSITIVITY, 0, 255);
                }
                noteStruct potNote = MIDIpack(_track[i].modulation >> 1);
                potNote = _inputProcess(potNote);
                eventManagerClass::setLiveNotePot(&_track[i], potNote, moving);
            }
        }
        else if (type == DRUM)
        {
            // Conditions for DRUM pot reading: override + selected track + valid page + master tick + pot active
            if (_overide && _selectedTrack == i 
                && (_overide || _rec)
                && (cuisine::keys::steps() == 0 || _page != STEP_EDIT)
                && clockClass::getMasterTick() == true  
                && (_page != BPM_PAGE && _page != EUCLID && _page != GATE_LENGHT && _page != DIV_MULT))
            {
                bool moving = cuisine::pot::moving();
                if (moving)
                {
                    _track[i].modulation = cuisine::pot::read(_track[i].modulation, DEFAULT_SENSITIVITY, 0, 255);
                }
                eventManagerClass::setLiveValue(&_track[i], _track[i].modulation, moving);
            }
        }
    }
}

static void interfaceClass::outputUpdate()
{
    // Phase 1: Update liveInput from pot (if conditions met)
    _updateLiveInputFromPot();
    
    // Phase 3: Resolve baseState (merges seq + live, handles recording)
    bool transportRunning = false;
    for (int i = 0; i < TRACK_NUMBER; i++)
    {
        if (_track[i].getTransport()) { transportRunning = true; break; }
    }
    eventManagerClass::resolve(_track, _overide, _rec, _selectedTrack, _recIndex, transportRunning);
    
    // Phase 3.5: Apply Operator Matrix logic (Gate Logic)
    eventManagerClass::applyOperators(_track); 
    
    // Phase 4: Dispatch to hardware
    eventManagerClass::dispatch(_track);
    
    // Phase 5: Update drum trigger timers
    eventManagerClass::updateDrumTriggers();

    // Phase 6: Clear step-level flags (trig/noteOff latches) for next tick
    for (int i = 0; i < TRACK_NUMBER; i++)
    {
        _track[i].clearStepFlags();
    }
}
void interfaceClass::_keyboardTransport(bool state)
{
    for (int i = 0; i < TRACK_NUMBER; i++)
    {
        // if (i <= 2)
        // {
        //     Serial.print("track : ");
        //     Serial.println(i);
        //     Serial.print("source :");
        //     Serial.println(_track[i].getPlaySource());
        // }

        const byte m = _track[i].getPlayMode();
        if (m && _selectedTrack == _track[i].getPlaySource() - 1)
        {
            if (state || m == GATE) { _track[i].setTransport(state); }
            _track[i].setKeyboardFlag(state);
        }
        
    }
}
// MIDI clock out on every other master tick. The timer interrupt itself now
// lives in the SDK; this is registered as its hook.
static void midiClockTick()
{
    interfaceClass::MIDItick = !interfaceClass::MIDItick;
    if (interfaceClass::MIDItick)
    {
        cuisine::midi::sendClock();
    }
}
static void interfaceClass::_MIDIccHandle(byte channel, byte CC, byte value)
{
    for(int i = 0; i < TRACK_NUMBER; i++)
    {
        if(channel == _track[i].getMIDIinChannel())
        {
            
            // set CC value for selected track
            if((cuisine::keys::commands() == REC || (_page == TRACK_SETTINGS && cuisine::keys::down(MOD)) ) 
                && _selectedTrack == i)
            {
                _trackPointer->setCC(CC);
            }
            // set CC value
            if(CC == _track[i].getCC() && _track[i].getMIDIinChannel() == channel)
            {
                _track[i].modulation = value << 1;
                // Feed into eventManager liveInput buffer - dispatch() will handle output
                eventManagerClass::setLiveValue(&_track[i], _track[i].modulation, true);
                // Serial.println(value);
            }
        }
    }
}

#ifdef BENCH
// Toggles step 1 of track 0 through the same path the UI uses, then reads the
// page bitmask back. If "before" and "after" match, the write never landed.
void interfaceClass::benchToggleStep()
{
    const unsigned int before = _track[0].readStepPage((byte)0);
    _track[0].writeStep(trackClass::indexUnpack(1));
    const unsigned int after = _track[0].readStepPage((byte)0);

    Serial.print(F("K "));
    Serial.print(before);
    Serial.print(' ');
    Serial.println(after);
}
#endif
