// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <tap.h>

volatile unsigned long tap::_lastTapTime = 0;
volatile unsigned int tap::_tapIntervals[MAX_TAP_BUFFER];
volatile byte tap::_tapIndex = 0;
volatile byte tap::_tapCount = 0;
volatile bool tap::_resetFlag = false;
volatile bool tap::_tapPageFlag = false;

void tap::start()
{
    reset();
}

void tap::reset()
{
    _tapCount = 0;
    _tapIndex = 0;
    _lastTapTime = 0;
    _resetFlag = false;
}

void tap::addTap()
{
    unsigned long now = millis();
    unsigned long interval = now - _lastTapTime;
    _resetFlag = false;

    // Timeout or First Tap logic
    // If no previous tap (lastTapTime == 0) or interval is very long (e.g., > 2 seconds),
    // treat this as the start/anchor of a new sequence.
    if (_lastTapTime == 0 || interval > 2000)
    {
        _lastTapTime = now;
        _tapCount = 0;
        _tapIndex = 0;
        // If it was a timeout (not explicit reset), we effectively reset the sequence
        return;
    }

    // Check Junk Filter if we have previous intervals
    if (_tapCount > 0)
    {
        // Get the last added interval
        // index points to next write, so (index - 1) is last write
        byte lastIndex = (_tapIndex == 0) ? MAX_TAP_BUFFER - 1 : _tapIndex - 1;
        unsigned int lastInterval = _tapIntervals[lastIndex];

        if (interval > (unsigned long)2 * lastInterval)
        {
            // Junk filter triggered: Reset and use this tap as new anchor
            reset();
            _lastTapTime = now;
            _resetFlag = true;
            return;
        }
    }

    // Valid interval, add to buffer
    _tapIntervals[_tapIndex] = (unsigned int)interval;
    _tapIndex++;
    if (_tapIndex >= MAX_TAP_BUFFER)
    {
        _tapIndex = 0;
    }

    if (_tapCount < MAX_TAP_BUFFER)
    {
        _tapCount++;
    }

    _lastTapTime = now;
}

unsigned int tap::getBPM()
{
    if (_tapCount == 0) return 0;

    unsigned long totalInterval = 0;
    for (int i = 0; i < _tapCount; i++)
    {
        // When buffer is full/wrapping, we need to read the correct values
        // If full, we read all MAX_TAP_BUFFER values.
        // If not full, we read _tapCount values starting from 0?
        // Wait, circular buffer logic:
        // If _tapCount < MAX_TAP_BUFFER, indices are 0 to _tapCount-1.
        // If _tapCount == MAX_TAP_BUFFER, indices are 0 to 15 (all).
        // Since we overwrite at _tapIndex, the values are always valid in the array 
        // regardless of order for averaging (sum doesn't care about order).
        totalInterval += _tapIntervals[i];
    }

    unsigned long averageInterval = totalInterval / _tapCount;
    
    // BPM = 60000ms / interval_ms
    // We want BPM * 10 (for 1 decimal place).
    // So 600000 / interval.
    
    if (averageInterval == 0) return 0; // Avoid division by zero
    
    return 600000 / averageInterval;
}

int tap::getLastTapsNumber()
{
    return _tapCount;
}

bool tap::isReset()
{
    return _resetFlag;
}
void tap::setTapPageFlag(bool value)
{
    _tapPageFlag = value;
}

