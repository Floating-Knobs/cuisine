// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

/**
 * cuisine.h — public entry point of the Cuisine hardware SDK.
 *
 * Hardware blocks only: the panel, the outputs, the clock and the memories.
 * The application owns patterns, tracks and all musical decisions.
 *
 * Every block has a namespaced facade in cuisine:: (pot::read, keys::steps,
 * gate_out::set, clock::start, ...). The module classes behind them are
 * implementation detail; new code should use the namespaces.
 */
#ifndef CUISINE_H
#define CUISINE_H

#include <Arduino.h>

#include <board.h>
#include <clock.h>
#include <DAC.h>
#include <display.h>
#include <gateIn.h>
#include <gateOut.h>
#include <keyboard.h>
#include <LED.h>
#include <midiMinimal.h>
#include <pot.h>
#include <storage.h>
#include <timer.h>

namespace cuisine {

/// Bring up every hardware block: LEDs, key matrix, CV, gates, pot, gate
/// inputs, clock and MIDI. Call once, before anything else.
///
/// Includes the keyboard scan settle (one full matrix pass), so the first
/// key edges after begin() are valid and no delay is needed by the caller.
/// Storage contents are not touched; that belongs to the application.
void begin();

/// Advance the hardware by one slice. Call once per loop.
///
/// Latches key edges, drains MIDI, advances the master timebase and flushes
/// gate and CV outputs that changed. Never blocks. LED output is deliberately
/// not flushed here: brightness is an application choice, so call
/// led::flush(brightness) yourself when the frame is drawn.
void poll();

} // namespace cuisine

#endif
