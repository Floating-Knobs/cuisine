// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <elapsed.h>
#include <timer.h>
#ifndef CLOCK_H
#define CLOCK_H

extern int wrap(int value, int min, int max);
class clockClass
{
    public:
        // Master timebase only. Per-track clocking lives in trackClock; the
        // public surface is cuisine::clock in this header.
        static  void setBPM(unsigned int BPM);
        static unsigned int getBPM() {return timerClass::getBPM(); }
        static void init();
        // to do rename variable
        static void setClock(byte value) { _clockSelect = value; }
        static byte getClock() { return _clockSelect; }

        
        static bool getMasterTick() { return _masterTick; }
        static void setMasterTick(bool value) { _masterTick = value; }
        static uint16_t getMasterTickCounter();

        //MIDI specific
        static void clockHandler();
        static void startHandler();
        static void stopHandler();
        
        //internal specific
        static void timerHandler() { timerTick = true; }
        static volatile bool timerTick;

        //Master specific
        static void masterUpdate();
        static void setMasterSource(byte select);
        static void setMasterInput(byte select);
        // static uint32_t getMasterCount() { return _masterClock; }
        static bool getMasterTransportFlag() { return _masterTransportFlag; }
        static bool getMasterTransportStarted() { return _masterTransportFlag & !_oldMasterTransportFlag; }
        static bool getMasterTransportStopped() { return _oldMasterTransportFlag & !_masterTransportFlag; }

        static bool getMasterResetFlag() { return  _masterResetFlag & !_oldMasterResetFlag; }

    private:
        // Master specific
        static bool _masterTick;
        static volatile uint16_t _masterTickCounter;
        static byte _masterSource; // MIDI, internal, USB, gate input A-D
        static byte _masterInput;
        static bool _masterResetFlag;
        static bool _oldMasterResetFlag;
        static bool _externalMIDItick;
        static bool _firstMIDItick;
        static bool _masterTransportFlag;
        static bool _oldMasterTransportFlag;  
        static byte _clockSelect; 
        static long _sincecLastMidiTickTable[8];
        static long _sinceLastMidiTickAverage() { long sum = 0; for(int i = 0; i < 8; i++) { sum += _sincecLastMidiTickTable[i]; } return sum / 8; }
        static int _midiBPM[8];
        static byte _midiBPMindex;
        static int _midiBPMaverage() { int sum = 0; for(int i = 0; i < 8; i++) { sum += _midiBPM[i]; } return sum / 8; }
        static bool _halfMidiPeriodFlag;
        static bool _halfMidiPeriod();
        static cuisine::Micros _sinceLastMidiTick;
};

namespace cuisine {
namespace clock {

/// Ticks in one step, for the stored division byte.
///
/// Encoding, as saved in FRAM: bit 0 picks the family (1 = straight,
/// 0 = triplet); bits 7..1 are the octave offset.
/// The grid is 48 ticks per quarter (see timer.cpp), so:
///   division 5 -> 12 ticks -> a 1/16.
/// Dotted divisions are not representable in this encoding: the third family
/// the old code checked for was unreachable. Adding them means a new FRAM
/// format, not a code change.
/// Pure: reads no clock state, safe to call from anywhere.
uint16_t ticksPerStep(uint8_t division);

// ── Master timebase ──────────────────────────────────────────────────
// One per device. Everything per-track (source, division, offset, play
// mode, coupling) is application-side; see src/trackClock.h.

// Source indices, replaced by an enum in the last step of the split.
constexpr uint8_t kInternal = 0;
constexpr uint8_t kMidi     = 1;
constexpr uint8_t kModular  = 2;

inline void    setSource(uint8_t source) { clockClass::setClock(source); }
inline uint8_t source()                  { return clockClass::getClock(); }

inline void     setTempoTenths(uint16_t bpm) { clockClass::setBPM(bpm); }
inline uint16_t tempoTenths()                { return clockClass::getBPM(); }

inline void start()     { clockClass::startHandler(); }
inline void stop()      { clockClass::stopHandler(); }
inline bool started()   { return clockClass::getMasterTransportFlag(); }
inline bool takeReset() { return clockClass::getMasterResetFlag(); }

/// One-poll edges on the master transport.
inline bool transportStarted() { return clockClass::getMasterTransportStarted(); }
inline bool transportStopped() { return clockClass::getMasterTransportStopped(); }

/// Advance the master timebase. Called once per poll by the application.
inline void poll() { clockClass::masterUpdate(); }

/// True if a master tick happened during the last poll().
inline bool tick() { return clockClass::getMasterTick(); }

/// Master tick counter: 48 ticks per quarter, on every source.
inline uint16_t ticks() { return clockClass::getMasterTickCounter(); }

/// The timer interrupt belongs to the SDK — every app needs it, and an app
/// that does not define the vector would jump to the default handler. This
/// lets an app hook the tick after the interrupt has latched it.
void onHardwareTick(void (*handler)());

} // namespace clock
} // namespace cuisine

#endif