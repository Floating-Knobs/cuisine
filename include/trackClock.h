// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef TRACKCLOCK_H
#define TRACKCLOCK_H

#include <Arduino.h>
#include <clock.h>

// Per-track clock workflow constants. Application-side by design: these used
// to live in the SDK's clock.h, which meant cuisine.h defined MASTER, GATE,
// FREE and friends for every user. Same names here so the app is unchanged.
// TODO: turn into enums when the application can absorb the churn.
#define MASTER 0
#define TRIGGED 1
#define COUPLED 2
#define ADRESS 3
#define LOOPER 4

#define TRACK_FIRST_STEP 2
#define FREE 0

#define STANDARD 1
#define TRIPLET 0
#define DOTTED 2

#define NORMAL 0
#define GATE 1
#define ONE_SHOT 2
#define PLAYMODE_ONESHOT_BIT 0x80
#define PLAYMODE_GATE_BIT    0x01
#define PLAYMODE_SOURCE_MASK 0x7E

/**
 * Per-track clocking: which source drives this track, its division, offset,
 * play mode and phase.
 *
 * Application-side by design: the master timebase lives in `cuisine::clock`,
 * and everything here is workflow (master/coupled/triggered, gate/one-shot,
 * division and offset). One instance per track.
 *
 * Step 3 of the clock split: this is a move, not a rewrite. The master
 * statics are still read through clockClass until the next step relocates
 * them; the friend declaration is temporary.
 */
class trackClock
{
    public:
        void update(bool advanceMaster = true, bool trackReset = false);
        void advanceOneMasterTick();

        void setSource(byte select) { _source = select; }
        byte getSource() { return _source; }

        void setInput(byte select) { _input = select; }
        byte getInput() { return _input; }

        void setResetSource(byte select) { _resetSource = select; }
        byte getResetSource() { return _resetSource; }

        void setResetInput(byte select) { _resetInput = select; }
        byte getResetInput() { return _resetInput; }

        void setStepDivision(byte value) { _timeDivision = value; }
        byte getStepDivision() { return _timeDivision; }
        byte getStepType() { return _timeDivisionType; }
        unsigned int getStepDuration();
        unsigned int getTimeDivision(byte value);

        void setClockOffset(int value) { _clockOffset = value; }
        unsigned int getClockOffset() { return _clockOffset; }

        void setTransportFlag(bool value) {
            _oldTransportFlag = _transportFlag;
            _transportFlag = value;
        }
        bool getTransportFlag() {
            return isIndependentPlay() ? _transportFlag : clockClass::getMasterTransportFlag();
        }
        bool getTransportFlagTrig() {
            return isIndependentPlay()
                ? (_transportFlag & !_oldTransportFlag)
                : cuisine::clock::transportStarted();
        }

        bool getReset() {
            return (_resetFlag & !_oldResetFlag) || clockClass::getMasterResetFlag();
        }
        void reset() {
            _oldResetFlag = false;
            _resetFlag = true;
            _clock = -1;
        }
        uint32_t read() { return _clock; }
        bool getTick() { return _tick; }

        void setPlayMode(byte mode) {
            byte src = _playMode & PLAYMODE_SOURCE_MASK;
            if (mode == ONE_SHOT) { _playMode = src | PLAYMODE_ONESHOT_BIT; }
            else { _playMode = src | (mode & PLAYMODE_GATE_BIT); }
        }
        void setPlayModeRaw(byte mode) { _playMode = mode; }
        void setPlaySource(byte source) {
            _playMode = (_playMode & (PLAYMODE_GATE_BIT | PLAYMODE_ONESHOT_BIT))
                      | ((source << 1) & PLAYMODE_SOURCE_MASK);
        }
        byte getPlayMode() {
            if (_playMode & PLAYMODE_ONESHOT_BIT) { return ONE_SHOT; }
            return _playMode & PLAYMODE_GATE_BIT;
        }
        byte getPlaySource() { return (_playMode & PLAYMODE_SOURCE_MASK) >> 1; }
        byte getPlayModeRaw() { return _playMode; }
        bool isIndependentPlay() {
            return (_playMode & (PLAYMODE_ONESHOT_BIT | PLAYMODE_GATE_BIT)) != 0;
        }

    private:
        byte _playMode = GATE;
        int  _clock = -1;
        bool _tick = false;

        byte _source = MASTER;
        byte _input;
        byte _resetSource;
        byte _resetInput;
        bool _resetFlag;
        bool _oldResetFlag;
        byte _timeDivision = 5;  // default 1/16
        byte _timeDivisionType = 0;
        int  _clockOffset = 0;

        bool _transportFlag = false;
        bool _oldTransportFlag = false;
};

#endif
