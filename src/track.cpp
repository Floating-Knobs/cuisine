// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

// #include <cstdint>
#include <track.h>
#include <Arduino.h>
#include <structDefinition.h>
// #include <clock.h>
#include <gateIn.h>
#include <elapsed.h>
#include <pot.h>
#include <euclid.h>

static uint16_t trackClass::_allGate = 0;
static uint16_t trackClass::_allAccent = 0;
static uint16_t trackClass::_oldAllAccent = 0;
static uint16_t trackClass::_allTrig = 0;
static uint16_t trackClass::_oldAllTrig = 0;
static uint16_t trackClass::_playTrig = 0;
static uint16_t trackClass::_allNoteOff = 0;
static uint16_t trackClass::_oldAllGate = 0; 
static indexStruct trackClass::_allIndex[12] = {0};
static indexStruct trackClass::_allIndexOld[12] = {0};
static uint16_t trackClass::_firstStepFlag = 0;
static uint16_t trackClass::_oldFirstStepFlag = 0;
static uint16_t trackClass::_oneShotArmed = 0;
static uint16_t trackClass::_oneShotLeftStart = 0;
uint16_t trackClass::_recMute = 0;
static patternStruct trackClass::_patternCopy;
static noteStruct trackClass::scaleOffset = {0,3};
noteStruct trackClass::_transposeOffset[TRACK_NUMBER] = {
  {0,3,0,0}, {0,3,0,0}, {0,3,0,0}, {0,3,0,0},
  {0,3,0,0}, {0,3,0,0}, {0,3,0,0}, {0,3,0,0},
  {0,3,0,0}, {0,3,0,0}, {0,3,0,0}, {0,3,0,0}
};
bool trackClass::_transposeStorageInitialized = false;
static byte trackClass::_currentChord = 0;
static byte trackClass::modulationOut[TRACK_NUMBER] = {0};

static bankPresetStruct trackClass::_bankPreset;
static byte trackClass::bankIndex = 255;

static bool trackClass::_keyboardFlag = false;
byte countSetBits(unsigned int num) {
  byte count = 0;
  while (num != 0) {
    count += num & 1;
    num >>= 1;
  }
  return count;
}
static byte trackClass::getNoteScaleCount(bool raw) {

  if(raw) {return countSetBits(_bankPreset.scale[0]);}

  unsigned int output = 0;
  if(_currentChord)
  {
    for(int i = 0; i < 4; i++)
    {
      if(bitRead(_currentChord, i))
      {
        output |= _bankPreset.chord[i];
      }
    }
  }
  if(output)
  {
    return countSetBits(output);
  }
  else
  {
    return countSetBits(_bankPreset.scale[0]);
  }
}
// static EEPROM_SPI_WE eeprom;
// The storage objects live in storage.cpp (Phase 1.4).
int wrap(int kX, int kLowerBound, int kUpperBound)
{
  // if(kLowerBound < 0)
  int range_size = kUpperBound - kLowerBound;

  if (kX < kLowerBound)
  {
    kX += range_size * ((kLowerBound - kX) / range_size + 1);
  }

  return kLowerBound + (kX - kLowerBound) % range_size;
}
indexStruct indexWrap(indexStruct index, byte first, byte last, byte size)
{
  if (last > first) 
  {
    
    index.step = (index.step + 1) % (last + 1);
    if(index.step < first) { index.step = first; }
    if (index.step == first) {
      index.page++;
    }
    if (index.page > size) { 
        index.page = 0;
      }
  } 
  else 
  {
    // index.step = index.step == 255 ? last - 1 : index.step;
    index.step = index.step - 1;
    if(index.step < last || index.step > first) { index.step = first; }
    if (index.step == first) {
      index.page--;
    }
    // Serial.println(index.page);
    if (index.page > size) { 
      index.page = size;
    }
  }
  
  return index;
}
void trackClass::init()
{
  _initTransposeStorage();
  cuisine::storage::fram.get(_patternNumberAddress(), _patternNumber);
  selectPattern(_patternNumber);
  initTimeDivision();

  loadPreset();
  loadOperators();
  bankPresetInit();
  _lastProcessedTick = clockClass::getMasterTickCounter();
}
void trackClass::reinit() 
{ 
  setFirstStep(0);
  setLastStep(15);
  
  // Initialize Operators to OFF (0xFF)
  memset(&operators, 0xFF, sizeof(OperatorSettings));
  saveOperators();

  for (byte i = 0; i < STEP_NUMBER; i++) {
    indexStruct index = indexUnpack(i);
    writeDuration(index, 0);
    writeValue(index, 0);
    writeStep(index, 0);

    // if(ID == 1)
    // {
    //   Serial.print("ID :");
    //   Serial.println(_ID);
    //   Serial.print("adress step :");
    //   Serial.println(_stepAddress(0));
    //   Serial.print("adress duration :");
    //   Serial.println(_durationAddress(0));
    //   Serial.print("adress value :");
    //   Serial.println(_valueAdress(0));
    //   Serial.print("adress first :");
    //   Serial.println(_firstAddress());
    //   Serial.print("adress last :");
    //   Serial.println(_lastAddress());
    //   Serial.print("adress size :");
    //   Serial.println(_sizeAddress());
    //   Serial.print("adress division :");
    //   Serial.println(_timeDivisionAddress());
    //   Serial.print("adress preset No :");
    //   Serial.println(_presetAddress());
    //   Serial.print("adress Low velo :");
    //   Serial.println(_lowVelocityAddress());
    //   Serial.print("adress high velo :");
    //   Serial.println(_highVelocityAddress());
    // }
  }

}

void trackClass::updateClock(bool advanceMaster) {
  // Reset by a track trigger is decided here; the clock only knows about
  // external gate inputs now (SDK Phase 1.3).
  bool trackReset = false;
  if (clock.getResetSource() == TRIGGED)
  {
    const byte input = clock.getResetInput();
    if (input >= 16)     { trackReset = getTrigStatus(input - 16) && getAccentStatus(input - 16); }
    else if (input < 12) { trackReset = getTrigStatus(input); }
  }

  clock.update(advanceMaster, trackReset);
  
  // check if reset is triggered by a track first step
  bool firstStepFlag = 0;
  if(clock.getResetSource() == TRACK_FIRST_STEP) 
  { 
    byte input = clock.getResetInput();
    firstStepFlag = bitRead(_firstStepFlag, input) & !bitRead(_oldFirstStepFlag, input);
    if(firstStepFlag) { reset();}
  }
}

void trackClass::updateGate(bool masterTickOccurred) {
  if (_gateFlag || _trigFlag) {
    if(masterTickOccurred)
    {
      _sinceLastGate++;
    }
    if(_trigFlag)
    {
      _durationUpdate(); 
    }

    _trigFlag = false;
    if (_sinceLastGate >= _gateDuration) {
      if (_gateFlag) _noteOffLatch = true;
      _gateFlag = false;
      _accentFlag = false;
    } 
  }


}

void trackClass::_updateIndex() 
{
  if (!clock.getTransportFlag()) {
    _index.step = 255;
    _index.page = 255;
    return;
  }
  if(getClockSource() != COUPLED && getClockSource() != ADRESS)
  {   
    byte first = firstStep;
    byte last = lastStep;
    
    _indexPtr = &_index;
    

    _index = indexWrap(_index, first, last, size);

    if(_override.flag)
    {
      _indexPtr = &_override.index;
      // Increment override.index based on override.first, override.last, and override.page
      if (_override.last > _override.first) {

        _override.index.step = (_override.index.step + 1) % (_override.last + 1);
        if (_override.index.step < _override.first) {  _override.index.step = _override.first; }
      } 
      else if (_override.last < _override.first) {
        _override.index.step = (_override.index.step - 1) % (_override.first + 1);
        if (_override.index.step < _override.last) { 
          _override.index.step = _override.first;
        }
      }
    }
  }
  // keep original index to avoid clock issues but to test, why not?
  _allIndex[_ID] = *_indexPtr;

  
  
  // should euclid be overrided?
  bool euclidStep = euclid.isOn() && euclid.getStep(*_indexPtr);

  if ((readStep(*_indexPtr) || euclidStep) && !getTrackMute(_ID)) 
  {
    if(readAccent(*_indexPtr)) { _accentFlag = true;}
    _trigFlag = true;
    _trigLatch = true;
    _gateFlag = true;
    _sinceLastGate = 0;
    _lastGateIndex = *_indexPtr;
  }
  
}
  

void trackClass::_processOneTick()
{
  clock.advanceOneMasterTick();
  if (clock.getTick())
  {
    _updateIndex();
    if(_override.flag && _override.loopWriteFlag) 
    {  
      writeStep(_index, readStep(_override.index));
      getType() == NOTE ? writeNote(_index, readNote(_override.index)) : writeValue(_index, readValue(_override.index));
      writeDuration(_index, getDuration(_override.index));
    }
  }
  updateGate(true);
}

void trackClass::reset()
{
  indexStruct origin = getOrigin();
  setIndex(origin);
  clock.reset();
}

void trackClass::setTransport(bool value, bool forceReset)
{
  clock.setTransportFlag(value);
  const uint16_t m = (uint16_t)1u << _ID;
  if (value && getPlayMode() == ONE_SHOT) {
    _oneShotArmed |= m;
    _oneShotLeftStart &= (uint16_t)~m;
  } else if (!value) {
    _oneShotArmed &= (uint16_t)~m;
    _oneShotLeftStart &= (uint16_t)~m;
  }
  if (forceReset) { reset(); }
}

void trackClass::latchPlayTrigs()
{
  _playTrig = _allTrig;
  _allTrig = 0;
}

void trackClass::update() {
  if(preset.patternSource > 0 && preset.patternSource <= TRACK_NUMBER && getTransport())
  {
    byte pattern = modulationOut[preset.patternSource - 1] >> 4;
    pattern = pattern < 12 ? pattern : 11;
    selectPattern(pattern);
  }
  byte source = getPlaySource() - 1;
  const byte mode = getPlayMode();
  if (mode && source != 255 && !_keyboardFlag)
  {
    bool level;
    bool trig;
    if (source >= 16)
    {
      level = trackClass::getAccentStatus(source - 16);
      trig  = trackClass::getTrigStatus(source - 16) && level;
    }
    else if (source >= 12)
    {
      level = cuisine::gate_in::high(source - 12);
      trig  = level;
    }
    else
    {
      level = trackClass::getGateStatus(source);
      trig  = trackClass::getTrigStatus(source);
    }

    if (mode == GATE)
    {
      setTransport(level, false);
      trig ? reset() : void(0);
    }
    else if (!getTransport())
    {
      bool shot = trig;
      if (source >= 16) { shot = bitRead(_playTrig, source - 16) && level; }
      else if (source < 12) { shot = bitRead(_playTrig, source); }
      if (shot) { setTransport(true); }
    }
  }
  if(!getTransport()) 
  { 
    _index.page = 255; 
    _index.step = 255;
    bitClear(_oneShotArmed, _ID);
    bitClear(_oneShotLeftStart, _ID);
  }
  _allIndexOld[_ID] = _allIndex[_ID];
  bitWrite(_oldAllGate, _ID, bitRead(_allGate, _ID));
  bitWrite(_allGate, _ID, _gateFlag);
  bitWrite(_oldAllAccent, _ID, bitRead(_allAccent, _ID));
  bitWrite(_allAccent, _ID, _gateFlag & _accentFlag);
  firstStep = getFirstStep();
  lastStep = getLastStep();
  size = getSize();
  bool tick = 0;

  if (getClockSource() == MASTER)
  {
    updateClock(false);
    if (clock.getReset()) { _index = getOrigin(); }
    if(_override.flag) 
    {
      _lastPtr = &_override.last;
      _firstPtr = &_override.first;
      _indexPtr = &_override.index;
    }
    else
    {
      _lastPtr = &lastStep;
      _firstPtr = &firstStep;
      _indexPtr = &_index;
    }
    uint16_t current = clockClass::getMasterTickCounter();
    uint16_t delta = current - _lastProcessedTick;
    const uint8_t MAX_CATCHUP = 4;
    if (delta > MAX_CATCHUP) { delta = MAX_CATCHUP; }
    for (uint8_t i = 0; i < delta; i++) { _processOneTick(); }
    _lastProcessedTick = current;
  }
  else
  {
    updateClock();
    tick = clock.getTick();
    if (getClockSource() == COUPLED && getTransport()) 
    {
      byte input = getClockInput();
      setIndex(_allIndex[input]);
      tick = indexPack(_allIndex[input]) != indexPack(_allIndexOld[input]);
    } 
    else if (getClockSource() == TRIGGED && getTransport()) 
    {
      byte input = getClockInput();
      if(input >= 16)
      {
        input = input - 16;
        tick = trackClass::getTrigStatus(input) & trackClass::getAccentStatus(input);
      }
      else if(input >= 12)
      {
        input = input - 12;
        tick = cuisine::gate_in::rising(input);  
      }
      else
      {
        tick = getTrigStatus(input);
      }
    }
    if (clock.getReset()) { _index = getOrigin(); }
    if(_override.flag) 
    {
      _lastPtr = &_override.last;
      _firstPtr = &_override.first;
      _indexPtr = &_override.index;
    }
    else
    {
      _lastPtr = &lastStep;
      _firstPtr = &firstStep;
      _indexPtr = &_index;
    }
    if (tick) 
    {
      _updateIndex();
      if(_override.flag && _override.loopWriteFlag) 
      {  
        writeStep(_index, readStep(_override.index));
        getType() == NOTE ? writeNote(_index, readNote(_override.index)) : writeValue(_index, readValue(_override.index));
        writeDuration(_index, getDuration(_override.index));
      }
    }
    updateGate(cuisine::clock::tick());
  }
  // fist step flag update
  bitWrite(_oldFirstStepFlag, _ID, bitRead(_firstStepFlag, _ID));
  if(_allIndex[_ID].step == firstStep && getPage() == 0) 
  { 
    bitSet(_firstStepFlag, _ID);
  }
  else { bitClear(_firstStepFlag, _ID); }

  // ONE_SHOT: stop after one full loop
  if (bitRead(_oneShotArmed, _ID) && getPlayMode() == ONE_SHOT)
  {
    if (!bitRead(_firstStepFlag, _ID)) { bitSet(_oneShotLeftStart, _ID); }
    else if (bitRead(_oneShotLeftStart, _ID) && !bitRead(_oldFirstStepFlag, _ID))
    {
      setTransport(false);
    }
  }

  bitWrite(_oldAllTrig, _ID, bitRead(_allTrig, _ID));
  bitWrite(_allTrig, _ID, _trigLatch);
  bitWrite(_allNoteOff, _ID, _noteOffLatch);
}
void trackClass::_durationUpdate()
{
  
  if(isEmpty() && !euclid.isOn()) {return;}
  if(getType() == DRUM) {_gateDuration = 2; return;}

  byte nextGate = 0;

  byte first = *_firstPtr;
  byte last = *_lastPtr;
  const byte size = getSize();
  indexStruct nextGateIndex = _lastGateIndex;

  do
  {
    nextGate++;
    nextGateIndex = indexWrap(nextGateIndex, first, last, size);
  }while (!readStep(nextGateIndex.step, nextGateIndex.page) 
      && indexPack(nextGateIndex) != indexPack(_lastGateIndex) 
      && !euclid.getStep(nextGateIndex));


  byte gateDuration = getDuration(_lastGateIndex);
  const uint16_t stepDur = clock.getStepDuration();
  
  if (gateDuration) 
  {
    _gateDuration = (gateDuration * stepDur * 16) / 255;
  }
  else 
  {
    //add one step to tie to the next one :
    if (gateLenghtOffset == 255) { nextGate++; }
    _gateDuration = nextGate * stepDur;

    // Integer scaling: _gateDuration *= gateLenghtOffset / 255.0f;
    _gateDuration = ((uint32_t)_gateDuration * gateLenghtOffset) / 255;
  }
  if ( _gateDuration < 2) { _gateDuration = 2; }
}

bool trackClass::getTrackMute(byte track)
{
  bool output = 0;
  for (int i = 0; i < 4; i ++) { output |= (bitRead(_bankPreset.muteGroup[i], track) & bitRead(_bankPreset.muteState, i)); }
  return output || (track < TRACK_NUMBER && (_recMute & (1u << track)));
}

void trackClass::setRecMute(byte track, bool state)
{
  if (track >= TRACK_NUMBER) return;
  if (state) _recMute |= (1u << track);
  else _recMute &= ~(1u << track);
}

void trackClass::clearRecMuteAll()
{
  _recMute = 0;
}
void trackClass::initEEPROM()
{
  // Both drivers bring up their own chip select and HOLD pin; this only has to
  // initialise them.
  cuisine::storage::fram.init();
  cuisine::storage::flash.initialize();
}
static const uint8_t COPY_CHUNK = 64;

void trackClass::_framCopy(uint16_t source, uint16_t destination, uint16_t length)
{
  byte chunk[COPY_CHUNK];
  while (length)
  {
    const uint8_t n = length < COPY_CHUNK ? (uint8_t)length : COPY_CHUNK;
    cuisine::storage::fram.readBlock(source, chunk, n);
    cuisine::storage::fram.writeBlock(destination, chunk, n);
    source += n;
    destination += n;
    length -= n;
  }
}
void trackClass::savePattern(byte patternNumber, bool quicksave = false)
{
  unsigned int source = _patternAdress;
  // save to buffer if quicksave:
  unsigned int destination = quicksave ? _getPatternBufferAdress() : _getPatternAdress(patternNumber);

  _framCopy(source, destination, sizeof(patternStruct));

  if(quicksave) 
  { 
    //update modified flag :
    cuisine::storage::fram.put(_modifiedFlagAddress(_ID), false);
  }
  // update pattern number :
  else { selectPattern(patternNumber);}
  
  savePreset(); // <-- to reactivate when ready
}
void trackClass::quicksave() 
{ 
  savePattern(_patternNumber, true);
}
void trackClass::savePattern()
{
  savePattern(_patternNumber);
}
void trackClass::loadPattern(byte patternNumber)
{

    //update modified flag :
    cuisine::storage::fram.put(_modifiedFlagAddress(), false);

    _framCopy(_getPatternBufferAdress(), _patternAdress, sizeof(patternStruct));
}
void trackClass::loadPattern()
{
  // get current pattern number :
  unsigned int patternNumber = cuisine::storage::fram.get(_patternNumberAddress(true), patternNumber);
  loadPattern(patternNumber);
}

static const uint16_t BANK_CHUNK = 256;

__attribute__((noinline)) void trackClass::_transferBank(byte bank, bool toFlash)
{
  byte buffer[BANK_CHUNK];
  const unsigned long flashStart = _getBankAdress(bank);

  if (toFlash) { cuisine::storage::flash.blockErase32K(flashStart); }

  for (uint16_t offset = 0; offset < FRAM_SIZE; offset += BANK_CHUNK)
  {
    if (toFlash)
    {
      cuisine::storage::fram.readBytes(offset, buffer, BANK_CHUNK);
      cuisine::storage::flash.writeBytes(flashStart + offset, buffer, BANK_CHUNK);
    }
    else
    {
      cuisine::storage::flash.readBytes(flashStart + offset, buffer, BANK_CHUNK);
      cuisine::storage::fram.writeBytes(offset, buffer, BANK_CHUNK);
    }
  }
}
static void trackClass::saveBank(byte destinationAdress)
{
  bankIndex = destinationAdress;
  _transferBank(destinationAdress, true);
}
static void trackClass::loadBank(byte sourceAdress)
{
  bankIndex = sourceAdress;
  _transferBank(sourceAdress, false);
  for(int y = 0; y < TRACK_NUMBER; y++)
  {
    cuisine::storage::fram.put(_getTrackModifiedFlagAdress(y), false);
  }
  bankPresetInit();
}

void trackClass::_initTransposeStorage()
{
  if (_transposeStorageInitialized) { return; }
  _transposeStorageInitialized = true;

  const byte transposeStorageMagic = 0xA7;
  byte magic = cuisine::storage::fram.read(transposeMetaAdress());
  noteStruct neutral = transposeNeutral();

  if (magic != transposeStorageMagic)
  {
    // One-time migration (Option B): seed per-track offsets from legacy global scaleOffset.
    for (byte i = 0; i < TRACK_NUMBER; i++)
    {
      presetStruct presetRead;
      cuisine::storage::fram.get(presetAdress(i), presetRead);
      noteStruct migrated = presetRead.transpose ? scaleOffset : neutral;
      _transposeOffset[i] = migrated;
      cuisine::storage::fram.put(transposeAdress(i), migrated);
    }
    cuisine::storage::fram.put(transposeMetaAdress(), transposeStorageMagic);
  }
  else
  {
    for (byte i = 0; i < TRACK_NUMBER; i++)
    {
      cuisine::storage::fram.get(transposeAdress(i), _transposeOffset[i]);
    }
  }
}

void trackClass::savePreset()
{
  unsigned int adress = _getPresetAdress();
  preset._playMode = clock.getPlayModeRaw();
  preset.clockSource = clock.getSource();
  preset.clockInput = clock.getInput();
  preset.resetSource = clock.getResetSource();
  preset.resetInput = clock.getResetInput();
  cuisine::storage::fram.put(adress, preset);
  cuisine::storage::fram.put(transposeAdress(_ID), _transposeOffset[_ID]);
}
void trackClass::loadPreset()
{
  _initTransposeStorage();
  unsigned int adress = _getPresetAdress();
  cuisine::storage::fram.get(adress, preset);
  cuisine::storage::fram.get(transposeAdress(_ID), _transposeOffset[_ID]);

  clock.setPlayModeRaw(preset._playMode);
  clock.setSource(preset.clockSource);
  clock.setInput(preset.clockInput);
  clock.setResetInput(preset.resetInput);
  clock.setResetSource(preset.resetSource);
}
void trackClass::pastePattern(byte first, byte last, byte destination)
{
  for(byte y = first; y <= last; first <= last ? y++ : y--)
  {
    indexStruct source = indexUnpack(y);

    writeValue(indexUnpack(destination), _patternCopy.value[y]); // (i & 0x0F)??
    writeStep(indexUnpack(destination), bitRead(_patternCopy.step[source.page], source.step));
    writeAccent(indexUnpack(destination), bitRead(_patternCopy.accent[source.page], source.step));
    writeDuration(indexUnpack(destination), _patternCopy.duration[y]);
    
    destination = first <= last ? destination + 1 : destination - 1;

    destination = destination % (STEP_NUMBER + 1);
  }
  

}
bool trackClass::isEmpty() // 
{
  bool flag = false;
  for(byte i = 0; i < VARIATION_NUMBER; i++)
  {
    flag |= readStepPage(i);
  }
  return !flag;
}
void trackClass::writeNote(indexStruct index, noteStruct note) 
{ 
  byte out = packNote(note); 
  writeAccent(index, note.accent);
  writeValue(index, out);
}
void trackClass::writeValue( indexStruct index, byte value) 
{ 
  cuisine::storage::fram.put(_valueAdress(indexPack(index)), value);
  setModifiedFlag(true);

}
void trackClass::writeStep( indexStruct index)
{
  unsigned int output = readStepPage(index);
  bitWrite(output, index.step, !bitRead(output, index.step));

  cuisine::storage::fram.put(_stepAddress(index.page), output);
  setModifiedFlag(true);

}
void trackClass::writeStep( indexStruct index, bool value)
{
  unsigned int output = readStepPage(index);
  bitWrite(output, index.step, value);
  cuisine::storage::fram.put(_stepAddress(index.page), output);
  setModifiedFlag(true);

}
void trackClass::writeStepPage(byte page, unsigned int value)
{
  cuisine::storage::fram.put(_stepAddress(page), value);
  setModifiedFlag(true);
}
void trackClass::writeAccent( indexStruct index)
{
  unsigned int output = readAccentPage(index);
  bitWrite(output, index.step, !bitRead(output, index.step));

  cuisine::storage::fram.put(_accentAddress(index.page), output);
  setModifiedFlag(true);
}
void trackClass::writeAccent( indexStruct index, bool value)
{
  unsigned int output = readAccentPage(index);
  bitWrite(output, index.step, value);
  cuisine::storage::fram.put(_accentAddress(index.page), output);
  setModifiedFlag(true);
}
void trackClass::writeAllAccent(bool value)
{
  unsigned int output = value ? 0xFFFF : 0;
  for(byte i = 0; i < VARIATION_NUMBER; i++)
  {
    cuisine::storage::fram.put(_accentAddress(i), output);
    setModifiedFlag(true);
  }
}
void trackClass::writeAllStep(bool value)
{
  unsigned int output = value ? 0xFFFF : 0;
  for(byte i = 0; i < VARIATION_NUMBER; i++)
  {
    cuisine::storage::fram.put(_stepAddress(i), output);
    setModifiedFlag(true);
  }
}
byte trackClass::packNote(noteStruct note)
{
  int8_t  in = CLAMP(note.octave, -4, 4);
  uint8_t out = (uint8_t)in & 0x0F;
  out = (uint8_t)(out << 4) | (note.note & 0x0F);
  return out;
}

struct noteStruct trackClass::readNote(indexStruct index)
{
  noteStruct out = {0,0,0};
  unsigned int valueRead = cuisine::storage::fram.get(_valueAdress(indexPack(index)), valueRead);

  out.note = valueRead & 0b00001111;
  uint8_t packed = (uint8_t)(valueRead & 0xFF);
  uint8_t o4 = (packed >> 4) & 0x0F;
  out.octave = (o4 & 0x08) ? (int8_t)(o4 - 16) : (int8_t)o4;
  out.octave = CLAMP(out.octave, -4, 4);
  out.accent = readAccent(index);
  return out;
}
byte trackClass::getMidiNote( noteStruct note)
{
  noteStruct outNote = note;
  noteStruct transposeOffset = getTransposeOffset();
  outNote.note = note.note + (getTranspose() ? transposeOffset.note : 0);
  outNote.octave = note.octave + (getTranspose() ? (transposeOffset.octave) : 0) + 4;

  
  
  outNote.octave = CLAMP(outNote.octave, 0, 9);
  byte out = 0;
  // if (getNoteScaleCount())
  // {
  //   // outNote.note = getNoteFromScaleValue(outNote);
    
  // }
  outNote.octave = outNote.octave + (outNote.note / 12);
  outNote.note = outNote.note % 12;
  out = outNote.note + (outNote.octave * 12);
  return CLAMP(out, 0, 127);
}
static uint16_t trackClass::getChord(byte chordNumber, bool raw)
{
  unsigned int output = 0;
  byte scaleCount = getNoteScaleCount();
  if(raw) 
  {
    output = _bankPreset.chord[chordNumber];
    for(int i = 15; i >= scaleCount; i--)
    {
      bitClear(output, i);
    }
    return output;
  }

  if (_currentChord)
  {
    byte chord = _bankPreset.chord[chordNumber];
    unsigned int scale = _bankPreset.scale[0];
    byte count = 0;
    byte index = 0;
    for (int i = 0; i < scaleCount; ++i)
    {
      if (bitRead(chord, i))
      {
        while (count <= i && scale && count <= scaleCount) 
        {
          if(count == i && (scale & 1))
          {
            bitWrite(output, (index), true);
          }
          count += scale & 1;
          index++;
          scale >>= 1;
        }
      }
    }
    return output;
  }
  return _bankPreset.scale[0];
}
static unsigned int trackClass::getAllChord()
{
  unsigned int output = _currentChord ? 0 : _bankPreset.scale[0];
  uint8_t bits = _currentChord & 0x0F;
  while (bits)
  {
    byte i = (byte)__builtin_ctz(bits);
    bits &= (uint8_t)(bits - 1);
    output |= getChord(i);
  }
  if (output == 0) { return _bankPreset.scale[0]; }
  return output;
}
static bool trackClass::setScale(byte note)
{
  unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, scale[0]);
  if (getNoteScaleCount() < 8)
  {
    bitWrite(_bankPreset.scale[0], note, !bitRead(_bankPreset.scale[0], note));
    cuisine::storage::fram.put(adress, _bankPreset.scale[0]);
    return false;
  }
  else if (bitRead(_bankPreset.scale[0], note))
  {
    bitWrite(_bankPreset.scale[0], note, !bitRead(_bankPreset.scale[0], note));
    cuisine::storage::fram.put(adress, _bankPreset.scale[0]);
    return false;
  }
  else { return true; }
}
static void trackClass::resetScale()
{
  unsigned int adress = bankPresetAdress() + offsetof(bankPresetStruct, scale[0]);
  _bankPreset.scale[0] = 0;
  cuisine::storage::fram.put(adress, _bankPreset.scale[0]);
}

static noteStruct trackClass::getNoteFromScaleValue(noteStruct value) {
  // if (_scale[0] == 0) { return -1;}
  // octave shift if bigger than scale count
  noteStruct out = value;
  // out.octave += value.note / getNoteScaleCount();
  out.note = value.note % getNoteScaleCount();
  
  //find the selected note corresponding to the value
  for (int i = 0; i < 12; i++) {
    // check the closest higher note :
    int quantizedNote = (i + out.note);
    if (bitRead(_bankPreset.scale[0], quantizedNote % 12 )) 
    {
      out.note = quantizedNote % 12;
      out.octave += quantizedNote / 12;
      return out;
    }
    // check the closest lower note :
    quantizedNote = (out.note - i);
    if(quantizedNote < 0) 
    {
      quantizedNote += 12;
      out.octave--;
    }
    if (bitRead(_bankPreset.scale[0], quantizedNote % 12 )) 
    {
      out.note = quantizedNote % 12;
      return out;
    }
    
  }
}


// void trackClass::recordGateLength(byte gateIndex, unsigned long gateStart, unsigned long gateEnd, unsigned long tickDuration)
// {
//   if (gateIndex >= pattern.first && gateIndex <= pattern.last) {
//     unsigned long duration = gateEnd - gateStart;
//     byte gateLength = (duration / tickDuration) * 255;
//     pattern.duration[gateIndex] = gateLength;
//   }
// }
void trackClass::commitEuclid()
{
  for(int i = 0; i < VARIATION_NUMBER; i++)
  {
    unsigned int value = 0;
    value |= euclid.getPattern(i) | readStepPage(i);
    writeStepPage(i, value);
  }
  euclid.beats = 0;
  euclid.update();
}
indexStruct trackClass::getOrigin()
{
  indexStruct origin = {0, 0, 0};
  origin.step = (lastStep > firstStep) ? (firstStep - 1) : (firstStep + 1);
  return origin;
}

