// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H
#include <structDefinition.h>
#include <arp.h>

// ── Event State ──────────────────────────────────────────────────────
// Unified per-track snapshot used across all pipeline phases.
struct RecordIndex {
    uint8_t step;
    uint8_t page;
};

struct EventState {
    noteStruct note;           // Pitch data (NOTE tracks): note = scale index
    uint8_t    value;          // Raw 0-255 (MOD value / DRUM velocity)
    RecordIndex recordIndex;   // Index where this event was recorded (for duration tracking)
    uint16_t   duration;         // Duration of the note in ticks
    
    // Packed booleans for RAM optimization
    uint8_t    gate : 1;       // Current gate level
    uint8_t    trig : 1;       // Rising edge: new note-on this tick
    uint8_t    keyTrig : 1;    // Trigger specifically from keyboard/button
    uint8_t    noteOff : 1;    // Falling edge: note-off this tick
    uint8_t    accent : 1;     // Accent flag
    uint8_t    active : 1;     // Live input is valid / held
    uint8_t    recorded : 1;   // Flag: note-on was recorded this input event
    uint8_t    override : 1;   // Flag: override mode is active for this track
    // velocity field removed - computed on demand in dispatch()
};

// ── Event Manager ────────────────────────────────────────────────────
// Static class owning the three pipeline buffers and the
// Resolve / Dispatch entry points.

class trackClass; // forward declaration

class eventManagerClass {
public:
    static void init();

    // ── Pipeline Phases ──────────────────────────────────────────────
    // 1. resolve: reads sequencer engine state and merges with liveInput[] into baseState[]
    //    Also handles recording when rec is true.
    //    recIndex: current step index for SH-101 style step recording (when transport stopped)
    //    transportRunning: true if master transport is running
    static void resolve(trackClass* tracks,
                        bool override, bool rec,
                        uint8_t selectedTrack,
                        uint16_t& recIndex,
                        bool transportRunning);
    // 3. applyOperators: This transforms baseState (Base) into a new finalState buffer
    static void applyOperators(trackClass* tracks); 
    // 4. dispatch: writes finalState[] to hardware outputs.
    static void dispatch(trackClass* tracks);

    // ── Per-Track Buffers ────────────────────────────────────────────
    static EventState liveInput[TRACK_NUMBER];
    static EventState baseState[TRACK_NUMBER];   // Base State (Seq + Live)
    static EventState finalState[TRACK_NUMBER];  // Final processed state
    static EventState prevFinalState[TRACK_NUMBER]; // For Sample & Hold rising edge detection

    // ── Live Input Helpers (called by Generators) ────────────────────
    static void setLiveNote(trackClass* track, noteStruct note, bool gate);
    static void setLiveNotePot(trackClass* track, noteStruct note, bool active);
    static void setLiveValue(trackClass* track, uint8_t value, bool gate);
    static void setLiveAccent(trackClass* track, bool accent, bool active);
    static void clearLive(uint8_t trackId);

    // New Helpers for MOD/DRUM logic
    static void handleLiveModOn(trackClass* track, uint8_t value);
    static void handleLiveModOff(trackClass* track, uint8_t value);
    static void handleStepInteraction(trackClass* track, indexStruct index, bool isNewPress);

    // ── Drum Trigger Timer ───────────────────────────────────────────
    static void updateDrumTriggers();
    static void startDrumTrig(uint8_t trackIndex, uint8_t gatePin);

    // ── Note Conversion ──────────────────────────────────────────────
    // Converts internal noteStruct (scale-index based) to MIDI note 0-127.
    // Applies per-track transpose via track->getTransposeOffset() when enabled.
    static uint8_t noteToMidi(noteStruct input, bool transpose, trackClass* track = nullptr);

private:
    static unsigned long _drumTrigStart[TRACK_NUMBER];
    static uint8_t       _drumTrigPin[TRACK_NUMBER];
    static bool          _drumTrigActive[TRACK_NUMBER];
    static constexpr uint8_t DRUM_TRIG_MS = 5;
    
    // MIDI note state tracking for polyphonic output
    static uint8_t _prevArpCount[TRACK_NUMBER];  // Previous arp note count per track
    static uint8_t _prevArpMidi[TRACK_NUMBER][ARP_MAX_NOTES]; // Previous arp MIDI notes per track

    // Sequencer note tracking: true when a sequencer-driven MIDI note is on and not yet note-off'd
    static bool _seqNoteActive[TRACK_NUMBER];
    
    // When true, sequencer gate/noteOff for this NOTE track are suppressed
    // while live performance (override OFF) owns the track. This prevents
    // the sequencer gate from reopening and retriggering after live release.
    static bool _seqSuppressedByLive[TRACK_NUMBER];

    // Armed on live release while suppression is active. Suppression clears
    // on the next real sequencer trigger instead of waiting for gate low.
    static bool _seqResumeOnNextTrig[TRACK_NUMBER];
    static bool _liveAccentOverrideValue[TRACK_NUMBER];
    static bool _liveAccentOverrideActive[TRACK_NUMBER];
};

#endif
