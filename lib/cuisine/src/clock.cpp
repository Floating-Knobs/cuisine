// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <clock.h>
#include <midiMinimal.h>
#include <gateIn.h>
#include <timer.h>
#include <elapsed.h>
// to do: - condition update to mode

// MIDI is handled via midiMinimal
//volatile uint32_t timerClass::_ticks; //?? 
static volatile bool clockClass::timerTick = {0};
//static cuisine::Micros clockClass::internalClockPeriod = {0};
static byte clockClass::_masterSource = {0};
static bool clockClass::_oldMasterResetFlag = {0};
static bool clockClass::_masterResetFlag = {0};
static bool clockClass::_externalMIDItick = {0};
static bool clockClass::_firstMIDItick = {0};
static bool clockClass::_masterTransportFlag = {0};
static bool clockClass::_oldMasterTransportFlag = {0};
static byte clockClass::_midiBPMindex{0};
static byte clockClass::_clockSelect{0};
static int clockClass::_midiBPM[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static long clockClass::_sincecLastMidiTickTable[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static bool clockClass::_masterTick = {0};
static volatile uint16_t clockClass::_masterTickCounter = {0};
static byte clockClass::_masterInput = {0};

static cuisine::Micros clockClass:: _sinceLastMidiTick = 0;
static bool clockClass::_halfMidiPeriodFlag = 0;

static void clockClass::init()
{
  // Omni mode to allow multi-channel routing in interface.
  cuisine::midi::init(0);
  cuisine::midi::onClock(clockHandler);
  cuisine::midi::onStart(startHandler);
  cuisine::midi::onStop(stopHandler);
  timerClass::init();
  // timerClass::setBPM(1270);
  // timerClass::setBPM(1450);
  // timerClass::setBPM(1490);
  // timerClass::setBPM(1500);
  setBPM(1270);

  _masterTransportFlag = false;
}
void clockClass::clockHandler()
{
  if(_clockSelect == cuisine::clock::kInternal || _clockSelect == cuisine::clock::kModular) { return; }

  _externalMIDItick = true;
  _halfMidiPeriodFlag = false;
  //is the first tick we get ?
  if(!_firstMIDItick) 
  { 
    _firstMIDItick = true; 
    
  }
  else
  {
    // Calculate BPM
    _sincecLastMidiTickTable[_midiBPMindex] = _sinceLastMidiTick;
    // Serial.println(_sinceLastMidiTickAverage());
    _midiBPM[_midiBPMindex] = (600000000 / 24 / _sinceLastMidiTick);
    _midiBPMindex++;
    _midiBPMindex %= 8;

    // Serial.println(_midiBPMaverage());
    // ((60000000 / _sinceLastMidiTick) / 24 )
    if(!_midiBPMindex)(timerClass::setBPM(_midiBPMaverage()));
  }
  // Serial.println("clock");
  // reset timer to zero
  _sinceLastMidiTick = 0;
  timerClass::reset();
}
void clockClass::stopHandler()
{
  // _clock = -1;
  _oldMasterTransportFlag = _masterTransportFlag;
  _masterTransportFlag = false;

  _oldMasterResetFlag = _masterResetFlag;
  _masterResetFlag = true;

  // _MIDItick = false;
  // timerClass::stop();
  cuisine::midi::sendStop();
}
void clockClass::startHandler()
{
  _oldMasterResetFlag = _masterResetFlag;
  _masterResetFlag = false;
  // _masterClock = -1;
  _oldMasterTransportFlag = _masterTransportFlag;
  _masterTransportFlag = true;
  // timerClass::start();
  // SerialMidi.sendStart();
}
static void clockClass::setBPM(unsigned int BPM)
{
  timerClass::setBPM(BPM);
}
namespace cuisine {
namespace clock {

uint16_t ticksPerStep(uint8_t division) {
    // Bits 7..1 are the octave offset; mask so corrupt data cannot shift UB.
    const uint8_t offset = (division >> 1) & 0x07;
    if (division & 1) { return (uint16_t)(3u << offset); }  // straight
    return (uint16_t)(2u << offset);                        // triplet
}

} // namespace clock
} // namespace cuisine

static void clockClass::masterUpdate()
{
 //_clockSelect = cuisine::clock::kMidi;
 _masterResetFlag = false;
  switch (_clockSelect)
  {
    case cuisine::clock::kInternal:
    {
      if (timerClass::tick()) 
      { 
        // _clock++; 
        _masterTick = true;
        _masterTickCounter++;
      }
      else { _masterTick = false; }
      break;
    }
    case cuisine::clock::kMidi:
    {
      if (_externalMIDItick || _halfMidiPeriod() ) //timerClass::tick()
      { 
        _externalMIDItick = false;
        _masterTick = true;
        _masterTickCounter++;
      }
      else { _masterTick = false; }
      break;
    }
    case cuisine::clock::kModular:
    {
      if ( cuisine::gate_in::rising(0) && _masterTransportFlag) 
      { 
        // _clock++; 
        _masterTick = true;
        _masterTickCounter++;
      }
      else { _masterTick = false; }
      break;
    }
  }
  // if(_clock > 2303) { _clock = 0; } // 2304 = LCM of 384, 576, 256
  _oldMasterTransportFlag = _masterTransportFlag;
}
void clockClass::setMasterSource(byte select) // set source of MASTER clock : Internal, MIDI, or gate
{
  _masterSource = select;
}
void clockClass::setMasterInput(byte select) // set input of MASTER clock : A, B, C, D
{
  _masterInput = select;
}
bool clockClass::_halfMidiPeriod()
{ 
  if(!_halfMidiPeriodFlag && _sinceLastMidiTick >= _sinceLastMidiTickAverage() / 2)
  {
    _halfMidiPeriodFlag = true;
    return true;
  }
  else { return false; }
}
uint16_t clockClass::getMasterTickCounter()
{
  return _masterTickCounter;
}
namespace {
void (*s_hardwareTickHook)() = nullptr;
}

namespace cuisine {
namespace clock {

void onHardwareTick(void (*handler)()) { s_hardwareTickHook = handler; }

} // namespace clock
} // namespace cuisine

// The timer interrupt belongs to the SDK: every application using the clock
// needs it, and an app that did not define the vector would fall into the
// default handler.
ISR(TCA0_OVF_vect)
{
    timerClass::setTick();
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
    if (s_hardwareTickHook) { s_hardwareTickHook(); }
}