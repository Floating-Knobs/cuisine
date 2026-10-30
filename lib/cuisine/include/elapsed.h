// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef ELAPSED_H
#define ELAPSED_H

#include <Arduino.h>

namespace cuisine {

/// Milliseconds since construction or since the last assignment.
///
///   cuisine::Millis since = 0;      // start now
///   if (since >= 500) { ... }       // half a second passed
///   since = 0;                      // restart
///
/// Wrap-safe: `millis()` and the stored base are both unsigned, so the
/// subtraction is correct across the 49-day rollover. Replaces the
/// elapsedMillis library with the same semantics, so no call site changes.
class Millis {
    public:
        Millis() : _base(millis()) {}
        Millis(uint32_t fromStart) : _base(millis() - fromStart) {}

        Millis& operator=(uint32_t fromStart) {
            _base = millis() - fromStart;
            return *this;
        }

        operator uint32_t() const { return millis() - _base; }

        /// Advance the timer by a value (elapsed grows by `by`).
        Millis& operator+=(uint32_t by) { _base -= by; return *this; }
        /// Rewind the timer (elapsed shrinks by `by`), used to keep blink
        /// phases from drifting.
        Millis& operator-=(uint32_t by) { _base += by; return *this; }

    private:
        uint32_t _base;
};

/// Microseconds, for the few places that need finer resolution (MIDI clock
/// period estimation). Same semantics as Millis.
class Micros {
    public:
        Micros() : _base(micros()) {}
        Micros(uint32_t fromStart) : _base(micros() - fromStart) {}

        Micros& operator=(uint32_t fromStart) {
            _base = micros() - fromStart;
            return *this;
        }

        operator uint32_t() const { return micros() - _base; }

        Micros& operator+=(uint32_t by) { _base -= by; return *this; }
        Micros& operator-=(uint32_t by) { _base += by; return *this; }

    private:
        uint32_t _base;
};

} // namespace cuisine

#endif
