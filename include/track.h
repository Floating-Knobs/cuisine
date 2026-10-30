// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen


#ifndef TRACK_H
#define TRACK_H

#include <Arduino.h>
#include <clock.h>
#include <trackClock.h>
// #include <display.h>
#include <storage.h>
#include <euclid.h>
#include <gateOut.h>
#include <structDefinition.h>
// #include <memory.h>
// type :
// #define GATE 0
#define NOTE 0  // exclusif
#define MOD 1
#define DRUM 2
// #define PITCH 2 // trig

#define FLASH_SIZE 0x800000
#define SECTOR_ERASE 0b11011000
#define WREN 0b00000110

#define SECTOR_SHIFT 15

#define SAVE_TIME 250
// variables to recall when changing pattern

#include <arp.h>

indexStruct indexWrap(indexStruct index, byte first, byte last, byte size);

class trackClass 
{
    public :
        trackClass(byte id) : _ID(id) {}
        arpClass arp;
        static indexStruct getIndex(byte track) { return _allIndex[track]; }
        indexStruct getIndex() { return _index; }
        indexStruct getIndexPtr() { return *_indexPtr; }
        // Mute group
        static void setMute(byte group, byte track)
        {
            toggleMuteGroupRAM(group, (unsigned int)(1UL << track));
            commitMuteGroupToFRAM(group);
        }
        // RAM-only mute toggles: used by UI hot paths to avoid FRAM stalls.
        static void toggleMuteGroupRAM(byte group, unsigned int trackMask)
        {
            _bankPreset.muteGroup[group] ^= trackMask;
        }

        static void commitMuteGroupToFRAM(byte group)
        {
            unsigned int adress = bankPresetAdress()
                                   + offsetof(bankPresetStruct, muteGroup)
                                   + (unsigned int)group * sizeof(uint16_t);
            cuisine::storage::fram.put(adress, _bankPreset.muteGroup[group]);
        }
        static bool getMute(byte group, byte track){ return bitRead(_bankPreset.muteGroup[group], track)  & 0xFFF; }
        static unsigned int getMuteGroupBits(byte group) { return _bankPreset.muteGroup[group]; }
        static uint16_t getRecMuteBits() { return _recMute; }
        static void setMuteStateAll( unsigned int group)
        {
            bool dirty = false;
            for(int i = 0; i < 4; i++)
            {
                if(bitRead(group, i))
                {
                    _bankPreset.muteState ^= (byte)(1u << i);
                    dirty = true;
                }
            }
            if (dirty)
            {
                unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, muteState);
                cuisine::storage::fram.put(adress, _bankPreset.muteState);
            }
        }
        static void toggleMuteStateRAM(byte group)
        {
            _bankPreset.muteState ^= (byte)(1u << group);
        }

        static void commitMuteStateToFRAM()
        {
            unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, muteState);
            cuisine::storage::fram.put(adress, _bankPreset.muteState);
        }
        static void setMuteState(byte group)
        {
            toggleMuteStateRAM(group);
            commitMuteStateToFRAM();
        }
        static void setMuteState(byte group, bool state)
        {
            bitWrite(_bankPreset.muteState, group, state);
            commitMuteStateToFRAM();
        }
        static bool getMuteState(byte group) { return bitRead(_bankPreset.muteState, group) & 0x0F; }
        static bool getTrackMute(byte track);
        static void setRecMute(byte track, bool state);
        static void clearRecMuteAll();
        static void bankPresetInit()
        {
            _bankPreset = cuisine::storage::fram.get(bankPresetAdress(), _bankPreset);
            clockClass::setBPM(_bankPreset.bpm);
        }
        static int getBPM() { return _bankPreset.bpm; }
        static void setBPM(int bpm) 
        { 
            unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, bpm);
            _bankPreset.bpm = bpm; 
            cuisine::storage::fram.put(adress, bpm); 
        }
        static byte getChordProgressionTrack() { return _bankPreset.chordProgressionTrack; }
        static void setChordProgressionTrack(byte track) 
        { 
            unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, chordProgressionTrack);
            _bankPreset.chordProgressionTrack = track; 
            cuisine::storage::fram.put(adress, track);
        }
        void init();
        void reinit();
        byte getID() {return _ID; }
        void update();
        void updateClock(bool advanceMaster = true);
        void updateGate(bool masterTickOccurred);
        void _processOneTick();
        // read and edit sequence
        bool isEmpty();
        bool readStep(byte step, byte page) { unsigned int out = 0; cuisine::storage::fram.get(_stepAddress(page), out); return bitRead( out, step ); }
        bool readStep(indexStruct index) { return readStep(index.step, index.page); }//pattern.step[indexPack(index)]; }
        unsigned int readStepPage(indexStruct index) { unsigned int out = 0; cuisine::storage::fram.get(_stepAddress(index.page), out); return out; }
        unsigned int readStepPage(byte page) { unsigned int out = 0; cuisine::storage::fram.get(_stepAddress(page), out); return out; }
        unsigned int readStepPage() { unsigned int out = 0; cuisine::storage::fram.get(_stepAddress(_indexPtr->page), out); return out; }
        
        bool readAccent(byte step, byte page) { unsigned int out = 0; cuisine::storage::fram.get(_accentAddress(page), out); return bitRead( out, step ); }
        bool readAccent(indexStruct index) { return readAccent(index.step, index.page); }//pattern.step[indexPack(index)]; }
        unsigned int readAccentPage(indexStruct index) { unsigned int out = 0; cuisine::storage::fram.get(_accentAddress(index.page), out); return out; }
        unsigned int readAccentPage() { unsigned int out = 0; cuisine::storage::fram.get(_accentAddress(_indexPtr->page), out); return out; }
        void writeAccent( indexStruct index);
        void writeAccent( indexStruct index, bool value);
        void writeAllAccent(bool value);
        // note specific :
        void writeNote(indexStruct index, noteStruct note);
        struct noteStruct readNote(indexStruct index); 

        byte getMidiNote( noteStruct note);
        static byte packNote(noteStruct note);

        byte readValue(indexStruct index) { byte out = 0; cuisine::storage::fram.get(_valueAdress(indexPack(index)), out); return out; } 
        void writeValue( indexStruct index, byte value);

        void writeStep( indexStruct index);
        void writeStep( indexStruct index, bool value);
        void writeStepPage(byte page, unsigned int value);
        void commitEuclid();
        void writeAllStep(bool value);
        void setFirstStep (byte value) { firstStep = value; cuisine::storage::fram.write(_firstAddress(), value); setModifiedFlag(true);}//{ pattern.first = value; }
        void setLastStep (byte value) { lastStep = value; cuisine::storage::fram.write(_lastAddress(), value); setModifiedFlag(true); }
        byte getFirstStep() { return cuisine::storage::fram.read(_firstAddress()); }//{ return pattern.first; }
        byte getLastStep() { return cuisine::storage::fram.read(_lastAddress()); }//{ return pattern.last; }
        void setModifiedFlag(bool value) { cuisine::storage::fram.put(_modifiedFlagAddress(), value); }
        bool getModifiedFlag() { return cuisine::storage::fram.read(_modifiedFlagAddress()); }
        byte getPatternIndex() { return cuisine::storage::fram.read(_patternNumberAddress()); }
        //transport
        void reset();
        void setIndex( indexStruct value) { _index = value; }
        void setIndex( byte step, byte page) { _index.step = step; _index.page = page; }
        void setIndexPtr( indexStruct value) { _indexPtr = &value;}
        void setIndexPtrValue( byte step, byte page) { _indexPtr->step = step; _indexPtr->page = page;}
        // int index();
        // byte to struct
        static byte indexPack(indexStruct index) { return index.step | index.page << 4; }
        static byte indexPack(byte step, byte page) { return step | page << 4; }
        //struct to byte
        static indexStruct indexUnpack(byte index) { return {index & 0x0F, index >> 4}; }
        byte getPage() {return _indexPtr->page;}
        // get the origin to start from
        indexStruct getOrigin();
        
        //settings
        void setType( byte value) {preset.type = CLAMP(value, 0, 2); }  
        byte getType() { return preset.type; } 
        void setPlayMode(byte mode) { clock.setPlayMode(mode); }
        void setPlaySource(byte value) { clock.setPlaySource(value); }
        byte getPlayMode() { return clock.getPlayMode(); }
        bool isIndependentPlay() { return clock.isIndependentPlay(); }
        byte getPlaySource() { return clock.getPlaySource(); }
        // void setTranspose(bool value) { preset.transpose = value; } // transpose on/off
        // void setTranspose() { preset.transpose = !preset.transpose; } // transpose on/off
        bool getTranspose() { return true; }//preset.transpose; }
        noteStruct getTransposeOffset() { return _transposeOffset[_ID]; }
        // pointer to transpose offset
        noteStruct* getTransposeOffsetPtr() { return &_transposeOffset[_ID]; }
        void setTransposeOffset(noteStruct value)
        {
            // if (_transposeOffset[_ID] == value) { return; }
            _transposeOffset[_ID] = value;
            cuisine::storage::fram.put(transposeAdress(_ID), value);
        }
        static noteStruct getTransposeOffset(byte track) { return _transposeOffset[track]; }
        static void setTransposeOffset(byte track, noteStruct value)
        {
            // if (_transposeOffset[track] == value) { return; }
            _transposeOffset[track] = value;
            cuisine::storage::fram.put(transposeAdress(track), value);
        }
        void setTransport(bool value, bool forceReset = true);
        void setKeyboardFlag(bool value) { _keyboardFlag = value; }
        bool getKeyboardFlag() { return _keyboardFlag; }
        bool getTransport() { return clock.getTransportFlag(); }
        void resetSourceSet(byte select) { clock.setResetSource(CLAMP(select, 0, 2)); }
        void resetInputSet(byte select) { clock.setResetInput(select); }
        byte getResetSource() { return clock.getResetSource(); }
        byte getResetInput() { return clock.getResetInput(); }
        void clockSourceSet(byte select) { clock.setSource(CLAMP(select, 0, 2)); }
        void clockInputSet(byte select) { clock.setInput(select); } // still used?
        void setTimeDivision(byte value) { cuisine::storage::fram.put(_timeDivisionAddress(), value); setModifiedFlag(true); clock.setStepDivision(value); }
        byte getTimeDivision() {  return clock.getStepDivision(); }
        void initTimeDivision() { clock.setStepDivision(cuisine::storage::fram.read(_timeDivisionAddress())); }
        unsigned int getPPQN() { return clock.getStepDuration(); }
        
        OperatorSettings getOperators() { return operators; }
        void setOperatorSource(uint8_t slot, uint8_t source) {
            uint8_t* ptr = (slot < 4) ? &operators.gateSources[slot] : &operators.cvSources[slot - 4];
            if (*ptr != source) {
                *ptr = source;
                saveOperators();
            }
        }
        void loadOperators() { cuisine::storage::fram.get(operatorAdress(_ID), operators); }
        void saveOperators() { cuisine::storage::fram.put(operatorAdress(_ID), operators); }

        byte getTimeType() { return clock.getStepType(); }
        byte getClockSource() { return clock.getSource(); }
        byte getClockInput() { return clock.getInput(); }
        bool getTick() {return clock.getTick(); }
        uint32_t getClock() { return clock.read(); }
        void setClockOffset(int value) { clock.setClockOffset(value); }
        int getClockOffset() { return clock.getClockOffset(); }
        //output
        void setMIDIchannel(int value) { preset.MIDIchannel = value; }
        int getMIDIchannel() { return preset.MIDIchannel; }
        int getMIDIoutChannel() { return preset.MIDIchannel & 0xFF; }
        int getMIDIinChannel() { return (preset.MIDIchannel >> 8) & 0xFF; }
        void setMIDIoutChannel(byte ch) { preset.MIDIchannel = (preset.MIDIchannel & 0xFF00) | (ch & 0xFF); }
        void setMIDIinChannel(byte ch) { preset.MIDIchannel = (preset.MIDIchannel & 0x00FF) | ((int)ch << 8); }
        void setCVout(int value) { preset.CVout = value -1; }
        int getCVOut() { return preset.CVout; }
        void setGateout(int value) { if(value != preset.gateOut && preset.gateOut != 255) {gateOutClass::write( preset.gateOut, false);} preset.gateOut = value; }
        int getGateOut() { return preset.gateOut; }
        void setAccentOut(int value) { if(value != preset.accentOut && preset.accentOut != 255) {gateOutClass::write( preset.accentOut, false);} preset.accentOut = value; }
        int getAccentOut() { return preset.accentOut; }

        // gate specific:
        indexStruct lastGateIndex() { return _lastGateIndex; }
        bool getTrigStatus() { return _trigLatch; }
        void setTrigStatus() { _trigFlag = true; _trigLatch = true; }
        static bool getTrigStatus(byte track) { return bitRead(_allTrig, track) & 1; }
        static void latchPlayTrigs();
        static bool getOldTrigStatus(byte track) { return bitRead(_oldAllTrig, track) & 1; }
        static bool getGateStatus(byte track) { return bitRead(_allGate, track) & 1; }
        static bool getAccentStatus(byte track) { return bitRead(_allAccent, track) & 1; }
        static bool getNoteOffStatus(byte track) { return bitRead(_allNoteOff, track) & 1; }
        bool getNoteOffFlag() { return _noteOffLatch; }
        void clearStepFlags() { _trigLatch = false; _noteOffLatch = false; bitClear(_allNoteOff, _ID); }
        byte getGateDuration(indexStruct index, bool total = false);
        bool getGateState() { return _gateFlag; }
        bool getAccentState() { return _accentFlag; }
        int gateLenghtOffset = 127;
        byte modulation = 127;
        static byte modulationOut[TRACK_NUMBER];

        static byte bankIndex;
        // pitch specific
        byte getLastNote() { return _lastNote; }
        void writeLastNote(byte value) { _lastNote = value; }

        // mod specific
        void setCC(byte value) { preset.CC = value; }
        byte getCC() { return preset.CC; }

        //DRUM specific 
        byte getNoteSelected() { return _noteSelected; }
        void setNoteSelected(byte value) { _noteSelected = value;}
        void setDefaultNote(noteStruct value) { _defaultNote = value; }
        noteStruct getDefaultNote() { return _defaultNote; }
        byte getVelocity(indexStruct index) { return readValue(index) >> 1; }
        void setVelocity(indexStruct index, byte value) { writeValue(index, value << 1); }
        euclideanClass euclid;


        void setLowVelocity(byte value) { _lowVelocity = value; }
        void setHighVelocity(byte value) { _highVelocity = value; }
        byte getLowVelocity() { return _lowVelocity; }
        byte getHighVelocity() { return _highVelocity; }

        byte getSize() { return cuisine::storage::fram.read(_sizeAddress()) & 3; }
        void setSize(byte value) { cuisine::storage::fram.put(_sizeAddress(), value);  setModifiedFlag(true);}
        void setOverrideFirst(byte value, byte page) 
        { 
            _override.first = value; 
            _override.last = value;
            _override.flag = true;
            _override.index.page = page;
            _override.index.step = value;
            setIndexPtr(_override.index);            
        }
        void setOverrideLast(byte value) { _override.last = value; }
        void setOverrideJump(byte first)
        {
            _override.flag = true;
            _override.first = first; 
            _override.last = lastStep;
            setIndexPtr(_override.index);
        }
        void resetOverride() 
        { 
            _override.flag = false; 
            _override.loopWriteFlag = false; 
            _override.first = 0;
            _override.last = 0;
        }
        void setOverrideFlag(bool value) { _override.flag = value; }
        bool getOverrideFlag() {return _override.flag; }
        void setLoopWriteFlag(bool value) { _override.loopWriteFlag = value; }
        bool getLoopWriteFlag() { return _override.loopWriteFlag; }
        
        struct resetStruct
        {
            byte step;
            byte multiplier;
        };
        static resetStruct unpackStepReset(byte input)
        {
            resetStruct out;
            out.step = input & 0b1111;
            out.multiplier = input >> 4;
            return out;
        }
        static byte packStepReset(resetStruct input)
        {
            byte out = input.step;
            out |= input.multiplier << 4;
            return out;
        }
        //get step duration from memory
        byte getDuration(indexStruct index)
        {
            uint32_t adress =  _durationAddress(indexPack(index)); // DURATION_OFFSET + (STEP_NUMBER * _ID) + indexPack(index);
            return cuisine::storage::fram.read(adress);
        }
        void writeDuration(indexStruct index, byte value)
        {
            uint32_t adress =  _durationAddress(indexPack(index)); //DURATION_OFFSET + (STEP_NUMBER * _ID) + indexPack(index);
            cuisine::storage::fram.put(adress, value);

            
        }
        presetStruct preset;
        OperatorSettings operators;
        
        //memory live in storage.{h,cpp}; accessed as cuisine::storage::fram / cuisine::storage::flash
        static void initEEPROM();
        void savePattern(byte patternNumber, bool quicksave = false);
        void loadPattern(byte patternNumber);
        void savePattern();
        void loadPattern();
        void quicksave();
        void selectPattern(byte patternNumber) {_patternAdress = _getPatternAdress(patternNumber); cuisine::storage::fram.put(_patternNumberAddress(), patternNumber);}

        void savePreset();
        void loadPreset();
        static void saveBank(byte bank);
        static void loadBank(byte bank);
        
        void copyPattern() { cuisine::storage::fram.get(_patternAdress, _patternCopy); }
        void pastePattern(byte first, byte last, byte destination); // <--

        static bool setScale(byte note);
        static void resetScale();
        static unsigned int getScale(bool raw = false) { return raw ? _bankPreset.scale[0] : getAllChord(); }
        static uint16_t getChord(byte chordNumber, bool raw = false);
        static unsigned int getAllChord();
        static void setChord(byte number, byte note) {bitWrite(_bankPreset.chord[number], note, !bitRead(_bankPreset.chord[number], note));}
        static void setChordState(byte number) { bitWrite(_currentChord, number, !bitRead(_currentChord, number)); }
        static void setChordState(byte number, bool value) { bitWrite(_currentChord, number, value); }
        static unsigned int getChordState() {return _currentChord; }
        static bool getChordState(byte number) {return bitRead(_currentChord, number); }
        static byte getNoteScaleCount(bool raw = true);
        static noteStruct getNoteFromScaleValue(noteStruct value);

        static noteStruct scaleOffset;
        static noteStruct transposeNeutral() { return {0, 3, 0, 0}; }

    private :
        uint16_t _patternAdress;
        byte _patternNumber;
        const byte _ID;
        void _updateIndex();
        static patternStruct _patternCopy;
        overrideStruct _override;

        trackClock clock;
        uint16_t _lastProcessedTick = 0;
        indexStruct _index = {255,255};
        indexStruct *_indexPtr = &_index;
        byte firstStep = 0; // <-- can be static
        byte lastStep = 15;
        byte size = 0;
        byte *_firstPtr = &firstStep;
        byte *_lastPtr = &lastStep;

        static uint16_t _firstStepFlag;
        static uint16_t _oldFirstStepFlag;
        static uint16_t _oneShotArmed;
        static uint16_t _oneShotLeftStart;

        static indexStruct _allIndex[12];
        static indexStruct _allIndexOld[12];
        static uint16_t _allGate; 
        static uint16_t _oldAllGate;
        static uint16_t _allAccent;
        static uint16_t _oldAllAccent;
        static uint16_t _allTrig;
        static uint16_t _oldAllTrig;
        static uint16_t _playTrig;
        static uint16_t _allNoteOff;

        static uint16_t _recMute;  // per-track REC mute (RAM only)
        
        // scale
        // static unsigned int _scale[4];
        
        static byte _currentChord;
        
        static bankPresetStruct _bankPreset;

        //gate specific :
        
        unsigned int _sinceLastGate;         
        void _durationUpdate();
        unsigned long _gateDuration;
        indexStruct _lastGateIndex = {0,0};
        // byte _lastGatePage;         
        // bool _accent[16];
                  
        bool _gateFlag = false;
        bool _accentFlag = false;
        bool _trigFlag = false;
        bool _trigLatch = false;
        bool _noteOffLatch = false;
        static bool _keyboardFlag;
        static noteStruct _transposeOffset[TRACK_NUMBER];
        static bool _transposeStorageInitialized;
        static void _initTransposeStorage();

        //bool _mod; // pitch or modulation
        byte _lowVelocity = 80;
        byte _highVelocity = 127;
        //pitch specific
        byte _lastNote;
        noteStruct _defaultNote;
        // DRUM specific
        
        byte _noteSelected = 36;

        // Define memory map addresses for each attribute in the struct
        unsigned int _stepAddress(byte page) {
            return offsetof(patternStruct, step) + sizeof(uint16_t) * page + _patternAdress ;
        }
        unsigned int _accentAddress(byte page) {

            return offsetof(patternStruct, accent) + sizeof(uint16_t) * page + _patternAdress;
        }
        unsigned int _durationAddress(byte step) {
            return offsetof(patternStruct, duration) + sizeof(byte) * step + _patternAdress;
        }
        unsigned int _valueAdress(byte step) {

            return offsetof(patternStruct, value) + sizeof(byte) * step + _patternAdress;
        }

        unsigned int _firstAddress() {

            return offsetof(patternStruct, first) + _patternAdress;
        }

        unsigned int _lastAddress() {

            return offsetof(patternStruct, last) + _patternAdress;
        }

        unsigned int _sizeAddress() {

            return offsetof(patternStruct, size) + _patternAdress;
        }

        unsigned int _timeDivisionAddress() {

            return offsetof(patternStruct, timeDivision) + _patternAdress;
        }
        unsigned int _patternNumberAddress() {

            return offsetof(patternStruct, patternNumber) + _getPatternBufferAdress();
        }
        static unsigned int _patternNumberAddress(byte track) {
            return offsetof(patternStruct, patternNumber) + patternBufferAdress(track);
        }
        unsigned int _modifiedFlagAddress() {

            return offsetof(patternStruct, modifiedFlag) + _getPatternBufferAdress();
        }
        static unsigned int _modifiedFlagAddress(byte track) {
            return offsetof(patternStruct, modifiedFlag) + patternBufferAdress(track);
        }

        unsigned int _getPatternAdress(byte patternNumber)
        {
            return patternAdress(_ID, patternNumber);
            // return sizeof(patternStruct) * _ID + sizeof(patternStruct) * patternNumber;
        }
        unsigned int _getCurrentPatternAdress() { return _patternAdress;}
        unsigned int _getPatternBufferAdress(){ return patternBufferAdress(_ID); }
        unsigned int _getPresetAdress() { return presetAdress(_ID); }
        
        // static unsigned int _getPatternBufferAdress(byte ID){ return BUFFER_OFFSET + ( ID * sizeof(patternStruct) ); }
        static unsigned long _getBankAdress(unsigned long bank) { return bank << SECTOR_SHIFT; }
        static void _transferBank(byte bank, bool toFlash);
        static void _framCopy(uint16_t source, uint16_t destination, uint16_t length);
        // static unsigned int _getBankAdress(byte bank, byte track) { return bank * sizeof(bankStruct) + _getTrackAdress(track); }
        static unsigned int _getTrackAdress(byte track) { return trackAdress(track); }
        static unsigned int _getTrackModifiedFlagAdress(byte track) { return offsetof(trackStruct, pattern[0].modifiedFlag) + _getTrackAdress(track); }
};


#endif