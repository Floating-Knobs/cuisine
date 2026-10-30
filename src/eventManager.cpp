// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <eventManager.h>
#include <track.h>
#include <gateOut.h>
#include <gateIn.h>
#include <DAC.h>
#include <midiMinimal.h>

// ── Static Member Definitions ────────────────────────────────────────
EventState eventManagerClass::liveInput[TRACK_NUMBER];
EventState eventManagerClass::baseState[TRACK_NUMBER];
EventState eventManagerClass::finalState[TRACK_NUMBER];
EventState eventManagerClass::prevFinalState[TRACK_NUMBER];

unsigned long eventManagerClass::_drumTrigStart[TRACK_NUMBER];
uint8_t       eventManagerClass::_drumTrigPin[TRACK_NUMBER];
bool          eventManagerClass::_drumTrigActive[TRACK_NUMBER];

uint8_t       eventManagerClass::_prevArpCount[TRACK_NUMBER];
uint8_t       eventManagerClass::_prevArpMidi[TRACK_NUMBER][ARP_MAX_NOTES];
bool          eventManagerClass::_seqNoteActive[TRACK_NUMBER];
bool          eventManagerClass::_seqSuppressedByLive[TRACK_NUMBER];
bool          eventManagerClass::_seqResumeOnNextTrig[TRACK_NUMBER];
bool          eventManagerClass::_liveAccentOverrideValue[TRACK_NUMBER];
bool          eventManagerClass::_liveAccentOverrideActive[TRACK_NUMBER];

// ── Init ─────────────────────────────────────────────────────────────
void eventManagerClass::init()
{
    for (uint8_t i = 0; i < TRACK_NUMBER; i++)
    {
        memset(&liveInput[i],   0, sizeof(EventState));
        memset(&baseState[i],   0, sizeof(EventState));
        memset(&finalState[i],  0, sizeof(EventState));
        memset(&prevFinalState[i], 0, sizeof(EventState));
        _drumTrigStart[i]  = 0;
        _drumTrigPin[i]    = 255;
        _drumTrigActive[i] = false;
        _prevArpCount[i]   = 0;
        memset(&_prevArpMidi[i], 0, ARP_MAX_NOTES);
        _seqNoteActive[i]  = false;
        _seqSuppressedByLive[i] = false;
        _seqResumeOnNextTrig[i] = false;
        _liveAccentOverrideValue[i] = false;
        _liveAccentOverrideActive[i] = false;
    }
}

// ── Live Input Helpers ───────────────────────────────────────────────
void eventManagerClass::setLiveNote(trackClass* track, noteStruct note, bool gate)
{
    if (!track) return;
    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) return;
    
    if (gate) 
    {
        /// Note-on: add to arp polyphonic pool
        uint8_t countBefore = track->arp.getNumKeysPressed();
        track->arp.addNote(note, track->getIndex());
        uint8_t countAfter = track->arp.getNumKeysPressed();
        
        // Always update state to keep in sync with physical key state
        // Even if note was duplicate, we need to ensure active/gate reflect key state
        if (countAfter > countBefore) {
            // New note added
            liveInput[trackId].note    = track->arp.getCurrentNote();
            liveInput[trackId].active  = true;
            liveInput[trackId].gate    = true;
            liveInput[trackId].trig    = true;
            liveInput[trackId].keyTrig = true;
            liveInput[trackId].noteOff = false;
        } else if (countAfter > 0) {
            // Duplicate note, but still need to ensure state is active
            // This handles case where same note pressed multiple times
            liveInput[trackId].active  = true;
            liveInput[trackId].gate    = true;
            liveInput[trackId].noteOff = false;
            // Don't set trig to avoid duplicate triggers
        }
    } 
    else 
    {
        // Note-off: remove from arp pool
        uint8_t countBefore = track->arp.getNumKeysPressed();
        track->arp.removeNote(note);
        uint8_t countAfter = track->arp.getNumKeysPressed();
        liveInput[trackId].trig = false;
        liveInput[trackId].keyTrig = false;

        // Update state (MIDI will be sent in dispatch())
        if (countAfter == 0) 
        {
            // All keys released
            liveInput[trackId].noteOff = true;
            liveInput[trackId].active = false;
            liveInput[trackId].gate = false;
        } 
        else {
            liveInput[trackId].note    = track->arp.getCurrentNote();
            liveInput[trackId].active  = true;
            liveInput[trackId].gate    = true;
            liveInput[trackId].noteOff = false;
        }
    }
}

void eventManagerClass::setLiveNotePot(trackClass* track, noteStruct note, bool active)
{
    if (!track) return;
    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) return;
    
    liveInput[trackId].note = note;
    
    bool keysPressed = (track->arp.getNumKeysPressed() > 0);
    bool nextActive = active || keysPressed;
    
    if (liveInput[trackId].active && !nextActive) {
        liveInput[trackId].noteOff = true;
    }
    
    liveInput[trackId].active = nextActive;
    // Don't explicitly clear trig, let resolve() handle it so keyboard triggers persist
}

void eventManagerClass::setLiveValue(trackClass* track, uint8_t value, bool gate)
{
    if (!track) return;
    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) return;

    liveInput[trackId].value = value;
    
    bool hasKeys = track->arp.getNumKeysPressed() > 0;
    bool active = gate || hasKeys;

    liveInput[trackId].gate    = gate;
    liveInput[trackId].active  = active;
    if (gate) liveInput[trackId].trig = true; 
    liveInput[trackId].noteOff = !active;
}

void eventManagerClass::setLiveAccent(trackClass* track, bool accent, bool active)
{
    if (!track) return;
    uint8_t trackId = track->getID();
    if (trackId >= TRACK_NUMBER) return;
    _liveAccentOverrideValue[trackId] = accent;
    _liveAccentOverrideActive[trackId] = active;
    liveInput[trackId].accent = accent;
}

void eventManagerClass::clearLive(uint8_t trackId)
{
    if (trackId >= TRACK_NUMBER) return;
    liveInput[trackId].active  = false;
    liveInput[trackId].gate    = false;
    liveInput[trackId].noteOff = true;
    liveInput[trackId].trig    = false;
    liveInput[trackId].keyTrig = false;
    _liveAccentOverrideActive[trackId] = false;
    // Note: arp reset should be handled by caller if needed
}

/**
 * @brief Handles a MOD/DRUM value trigger (Note On).
 * Updates track modulation buffer and populates Arp for cycling.
 */
void eventManagerClass::handleLiveModOn(trackClass* track, uint8_t value) {
    track->modulation = value;
    // Add value to arp pool for resolution cycle (packed into noteStruct)
    track->arp.addNote(arpClass::packValue(value), track->getIndex());
    // Update liveInput buffer for immediate dispatch
    setLiveValue(track, value, true);
    liveInput[track->getID()].keyTrig = true;
}

/**
 * @brief Handles a MOD/DRUM value release (Note Off).
 * Removes value from Arp and clears live input if no keys remain.
 */
void eventManagerClass::handleLiveModOff(trackClass* track, uint8_t value) {
    track->arp.removeNote(arpClass::packValue(value));
    // Only kill the gate if all related keys are released
    if (track->arp.getNumKeysPressed() == 0) {
        setLiveValue(track, 0, false);
    }
}

/**
 * @brief Handles step button interaction for MOD/DRUM tracks.
 * Performs "Pick-up" (Value -> Buffer) if step exists, or "Write" (Buffer -> Value) if empty.
 */
void eventManagerClass::handleStepInteraction(trackClass* track, indexStruct index, bool isNewPress) {
    // if (!isNewPress) return;

    if (track->readStep(index)) {
        // Step is active: "Pick-up" value from memory to the track buffer
        track->modulation = track->readValue(index);
    } else {
        // Step is inactive: "Write" current buffer value to this step
        track->writeValue(index, track->modulation);
        track->writeStep(index, true);
    }
}

// ── Note Conversion ──────────────────────────────────────────────────
// Converts noteStruct (scale-index) to chromatic MIDI note 0-127.
// Mirrors interfaceClass::_getMIDInote logic and per-track transpose.
uint8_t eventManagerClass::noteToMidi(noteStruct input, bool transpose, trackClass* track)
{
    if(transpose)
    {
        noteStruct offset = track->getTransposeOffset();
        input.note   += offset.note;
        input.octave += offset.octave;
        input.octave = CLAMP(input.octave, -4, 4);
    }

    unsigned int scale = trackClass::getScale();
    if (scale == 0)
    {
        uint8_t out = input.note + ((input.octave + 4) * 12);
        return CLAMP(out, 0, 127);
    }

    // Wrap note within scale count
    uint8_t scaleCount = trackClass::getNoteScaleCount(false);
    if (scaleCount > 0 && input.note >= scaleCount)
    {
        input.octave += (input.note / scaleCount);
        input.note   %= scaleCount;
    }

    // Map scale index to chromatic note
    uint8_t chromaticNote = input.note; // fallback
    int count = 0;
    for (int i = 0; i < 12; ++i)
    {
        if ((scale & (1 << i)) != 0)
        {
            if (count == input.note)
            {
                chromaticNote = i;
                break;
            }
            count++;
        }
    }
    uint8_t out = chromaticNote + (input.octave + 4) * 12;
    return CLAMP(out, 0, 127);
}

static inline uint16_t scaleDuration(uint8_t raw, uint16_t ppqn)
{
    if (raw == 0) return 0;
    return (uint16_t)((uint32_t)raw * ppqn * 16 / 255);
}

// ── Phase 1: Resolve ─────────────────────────────────────────────────
// Reads sequencer engine state and merges with liveInput[] into baseState[].
// Also handles recording when rec is true.
// Called AFTER trackClass::update() has advanced each track.
void eventManagerClass::resolve(trackClass* tracks,
                                bool override, bool rec,
                                uint8_t selectedTrack,
                                uint16_t& recIndex,
                                bool transportRunning)
{
    for (uint8_t i = 0; i < TRACK_NUMBER; i++)
    {
        EventState& a    = baseState[i];
        EventState& live = liveInput[i];
        uint8_t type     = tracks[i].getType();
        indexStruct gateIdx = tracks[i].lastGateIndex();
        uint16_t ppqn       = tracks[i].getPPQN();

        // Read sequencer state directly (was populateSeqState)
        a.trig    = tracks[i].getTrigStatus();
        a.noteOff = tracks[i].getNoteOffFlag();
        a.gate    = tracks[i].getGateState();
        // Base accent comes from sequencer state OR live accent override while pressed
        bool liveAccentOverride = _liveAccentOverrideActive[i] && _liveAccentOverrideValue[i];
        a.accent  = tracks[i].getAccentState() || liveAccentOverride;
        a.override = override;

        // Resume sequencer ownership only on a fresh sequencer trig.
        if (_seqResumeOnNextTrig[i] && a.trig)
        {
            _seqSuppressedByLive[i] = false;
            _seqResumeOnNextTrig[i] = false;
        }

        // Always read current values (needed for continuous MOD output)
        switch (type)
        {
            case NOTE:
                if (a.trig)
                {
                    a.note     = tracks[i].readNote(gateIdx);
                    a.accent   = a.note.accent || a.accent;
                    a.duration = scaleDuration(tracks[i].getDuration(gateIdx),
                                               ppqn);
                }
                break;
            case MOD:
                // Read value every cycle for continuous output
                a.value = tracks[i].readValue(gateIdx);
                if (a.trig)
                {
                    // Set duration for MOD track triggers
                    a.duration = scaleDuration(tracks[i].getDuration(gateIdx),
                                               ppqn);
                }
                break;
            case DRUM:
                a.value = tracks[i].readValue(tracks[i].lastGateIndex());
                break;
        }

        // ── Empty pattern: bypass sequencer when live input active ───
        // For MOD tracks, if pattern is empty and live input is active, use live input directly
        if (type == MOD && tracks[i].isEmpty() && live.active)
        {
            a.value   = live.value;
            a.gate    = live.gate;
            a.trig    = live.trig;
            a.noteOff = live.noteOff;
            a.accent  = live.note.accent;
        }
        // ── Override ON + Live input active ──────────────────────────
        else if (override && live.active)
        {
            switch (type)
            {
                case NOTE:
                    // Armed: note cycles via arp on sequencer trig, gate always from sequencer
                    if (a.trig && tracks[i].arp.getNumKeysPressed() > 0) {
                        // Advance arp on sequencer trig
                        a.note = tracks[i].arp.getNextNote();
                    } else if (tracks[i].arp.getNumKeysPressed() > 0) {
                        // Peek current note without advancing
                        a.note = tracks[i].arp.getCurrentNote();
                    } else {
                        a.note = live.note; // fallback
                    }
                    // Effective accent: note accent OR live accent override while pressed
                    a.accent   = a.note.accent || (_liveAccentOverrideActive[i] && _liveAccentOverrideValue[i]);
                    // Gate, trig, noteOff already set from sequencer state above
                    break;

                case MOD:
                    // Armed: value cycles via arp on sequencer trig
                    if (tracks[i].arp.getNumKeysPressed() > 0)
                    {
                        if (a.trig) {
                            a.value = tracks[i].arp.getNextValue();
                            // Advance arpeggiator
                        } else {
                            a.value = tracks[i].arp.getCurrentValue();
                        }
                        
                    }
                    else
                    {
                        a.value = live.value; // Use pot value
                        a.gate    = true;                // Force HIGH for bypass/override
                        a.trig    = a.trig || live.trig; // Trigger on steps OR pot movement
                    }
                    // NoteOff already set from sequencer state above

                    break;

                case DRUM:
                    // Armed velocity: value cycles via arp on sequencer trig, timing from sequencer
                    if (a.trig && tracks[i].arp.getNumKeysPressed() > 0) {
                        // Advance arp on sequencer trig
                        a.value = tracks[i].arp.getNextValue();
                    } else if (tracks[i].arp.getNumKeysPressed() > 0) {
                        // Peek current value without advancing
                        a.value = tracks[i].arp.getCurrentValue();
                    } else {
                        a.value = live.value; // fallback
                    }
                    // Gate, trig, noteOff already set from sequencer state above
                    break;
            }
        }
        // update modulation value on trig :
        if (a.trig && (type == MOD || type == DRUM) && override) {
            tracks[i].modulation = a.value;
        }
        // ── Override OFF + Live input active (Performance) ───────────
        else if (!override && live.active)
        {
            switch (type)
            {
                case NOTE:
                    // Note from arp current, gate follows key input
                    if (tracks[i].arp.getNumKeysPressed() > 0) {
                        a.note   = tracks[i].arp.getCurrentNote();
                        a.gate   = true;  // HIGH when keys held
                        a.trig   = live.trig;
                        // Live performance is owning the track while sequencer gate is high:
                        // mark sequencer as suppressed so its gate cannot retrigger later.
                        if (tracks[i].getGateState()) {
                            _seqSuppressedByLive[i] = true;
                            _seqResumeOnNextTrig[i] = false;
                        }
                    } else {
                        // No keys held
                        a.note   = live.note;
                        // Effective accent when keys released: live note accent OR pressed override
                        a.accent = live.note.accent || (_liveAccentOverrideActive[i] && _liveAccentOverrideValue[i]);
                        a.gate   = false;
                        a.trig   = false;
                        a.noteOff = true;
                        a.duration = 0;
                    }
                    break;

                case MOD:
                    // MOD from arp current, gate follows key input
                    if (tracks[i].arp.getNumKeysPressed() > 0) {
                        a.value   = tracks[i].arp.getCurrentValue();
                        a.gate    = true;  // HIGH when keys held
                        a.trig    = live.trig;
                        // Live performance is owning the track while sequencer gate is high:
                        // mark sequencer as suppressed so its gate cannot retrigger later.
                        if (tracks[i].getGateState()) {
                            _seqSuppressedByLive[i] = true;
                            _seqResumeOnNextTrig[i] = false;
                        }
                    } else {
                        // No keys held
                        a.value   = live.value;
                        a.gate    = live.gate;
                        a.trig    = live.trig;
                        a.noteOff = live.noteOff;
                    }
                    break;

                case DRUM:
                    a.value    = live.value;
                    a.gate     = live.gate;
                    a.trig     = live.trig;
                    a.noteOff  = live.noteOff;
                    break;
            }
        }
        // else: a = seq (default)

        // Momentary accent override:
        // When accent ON/OFF is pressed, force the effective accent value so it can also be recorded.
        if (_liveAccentOverrideActive[i])
        {
            a.accent = _liveAccentOverrideValue[i];
            if (type == NOTE)
            {
                a.note.accent = a.accent;
            }
        }

        // ── Suppress sequencer gate/noteOff while live owns NOTE track ─
        // When _seqSuppressedByLive is true for a NOTE track with override OFF,
        // we completely ignore sequencer gate/duration/noteOff and let only live
        // input drive the logical state, to avoid sequencer retriggers or hangs.
        if (_seqSuppressedByLive[i] && (type == NOTE || type == MOD) && !override)
        {
            bool liveKeysHeld = (tracks[i].arp.getNumKeysPressed() > 0);

            if (liveKeysHeld)
            {
                // Keep the live-owned gate state computed above and ignore
                // sequencer note-off while keys are still physically held.
                a.noteOff = false;
            }
            else
            {
                // After live release, keep the interrupted sequencer gate closed
                // until suppression is cleared by the next sequencer trig.
                a.gate    = false;
                a.noteOff = live.noteOff;
            }

            a.duration = 0;
        }

        // ── Handle live note-off (key released) ──────────────────────
        // When a live key is released, noteOff is set but active becomes false.
        // Propagate noteOff to baseState. Only force gate LOW for NOTE tracks
        // when there is no sequencer trig (pure keyboard note-off). MOD/DRUM
        // must not have their sequencer gate zeroed here, or they get stuck in
        // HOLD with no value/gate updates.
        if (live.noteOff && !live.active)
        {
            a.noteOff = true;
            if (_seqSuppressedByLive[i] && (type == NOTE || type == MOD) && !override)
            {
                _seqResumeOnNextTrig[i] = true;
            }
        }

        // ── Recording ────────────────────────────────────────────────
        if (rec && (live.active || _liveAccentOverrideActive[i]))
        {
            indexStruct index = tracks[i].getIndex();
            indexStruct seqIndex = index; // sequencer timing index for accent-only recording
            if (override)
            {
                // In override mode, record overriden notes/values at sequencer timing
                if (a.trig && live.active)
                {
                    switch (type)
                    {
                        case NOTE:
                            tracks[i].writeNote(index, a.note);
                            // tracks[i].writeStep(index, true);
                            break;
                        case MOD:
                            tracks[i].writeValue(index, a.value);
                            // if no keys pressed, write step to the value : overridez only via pot movement
                            if(tracks[i].arp.getNumKeysPressed() == 0)
                            {
                                tracks[i].writeStep(index, a.value);
                            }
                            break;
                        case DRUM:
                            tracks[i].writeValue(index, a.value);
                            // tracks[i].writeStep(index, true);
                            break;
                    }
                }
            }
            else // !override (Standard live recording)
            {
                
                // SH-101 style step recording when transport stopped
                if (!transportRunning)
                {
                    index = tracks[i].indexUnpack(recIndex);
                }
                
                // Record on note-on (trig)
                if (live.trig && live.active)
                {
                    // Clock threshold quantization: record to next step if past halfway
                    uint32_t clock = tracks[i].getClock();
                    uint32_t threshold = ppqn / 2;
                    if (transportRunning && clock > threshold)
                    {
                        index = indexWrap(index, 
                                        tracks[i].getFirstStep(), 
                                        tracks[i].getLastStep(), 
                                        tracks[i].getSize());
                    }
                    
                    switch (type)
                    {
                        case NOTE:
                            tracks[i].writeDuration(index, 0);
                            tracks[i].writeNote(index, a.note);
                            tracks[i].writeStep(index, true);
                            break;
                        case MOD:
                            tracks[i].writeValue(index, a.value);
                            if (live.keyTrig) tracks[i].writeStep(index, true);
                            break;
                        case DRUM:
                            tracks[i].writeValue(index, a.value);
                            tracks[i].writeStep(index, true);
                            break;
                    }
                    
                    // Store the record index for duration calculation on note-off
                    live.recordIndex.step = index.step;
                    live.recordIndex.page = index.page;
                    live.recorded = true;
                    live.duration = 0;  // Initialize duration counter

                    // Advance recIndex for SH-101 style step recording
                    if (!transportRunning)
                    {
                        recIndex++;
                    }
                }
                // Update duration tick count for active recorded notes
                if (live.recorded && clockClass::getMasterTick())
                {
                    // Only increment on master clock ticks (96 PPQN)
                    if (live.duration < 65535) // prevent overflow
                    {
                        live.duration++;
                    }
                }
            }

            // Accent-only recording: when REC is armed and a NOTE step is actively triggering,
            // store the pressed accent state into that step's accent bit.
            if (type == NOTE && a.trig && _liveAccentOverrideActive[i])
            {
                tracks[i].writeAccent(seqIndex, a.accent);
            }
        }
        // Handle note-off: write duration
        if (live.noteOff && live.recorded && !override)
        {
            uint32_t stepDuration = ppqn; // ticks per step
            uint16_t durationValue = (live.duration * 255) / (stepDuration * 16);
            if (durationValue > 255) durationValue = 255;
            indexStruct writeIndex = {live.recordIndex.step, live.recordIndex.page, 0};
            tracks[i].writeDuration(writeIndex, (byte)durationValue);
            
            // Reset duration counter
            live.duration = 0;
            live.recorded = false;
        }
        // Clear single-tick flags on liveInput
        liveInput[i].trig    = false;
        liveInput[i].keyTrig = false;
        // liveInput[i].noteOff = false; // Always clear so it doesn't stick

        // Only clear noteOff if active is true (no pending note-off)
        // If active is false, noteOff needs to persist until processed
        if (liveInput[i].active) {
            liveInput[i].noteOff = false;
        }
    }
}

// ── Phase 3: Apply Operators ─────────────────────────────────────────
// Logic Matrix transformation: Base State -> Final State
void eventManagerClass::applyOperators(trackClass* tracks)
{
    for (uint8_t i = 0; i < TRACK_NUMBER; i++)
    {
        // 1. Snapshot previous state for edge detection
        prevFinalState[i] = finalState[i];
        
        // 2. Start with the Base State
        finalState[i] = baseState[i];
        bool resultGate = baseState[i].gate;
        
        // 3. Fetch cached settings from RAM
        OperatorSettings ops = tracks[i].getOperators(); 

        // 4. Process Gate Logic Slots (0: AND, 1: OR, 2: XOR, 3: NAND)
        for (uint8_t slot = 0; slot < 4; slot++)
        {
            uint8_t sourceIdx = ops.gateSources[slot];
            if (sourceIdx == 255) continue; // Slot is OFF

            bool sourceGate = false;
            if (sourceIdx < 12) {
                // Internal Source: Read from Base State of current tick (Instantaneous)
                sourceGate = baseState[sourceIdx].gate;
            } else if (sourceIdx >= 12 && sourceIdx <= 15) {
                // External Source: Gate In A-D
                sourceGate = cuisine::gate_in::high(sourceIdx - 12);
            }

            // Apply Boolean Logic
            switch (slot) {
                case 0: resultGate &= sourceGate; break; // AND
                case 1: resultGate |= sourceGate; break; // OR
                case 2: resultGate ^= sourceGate; break; // XOR
                case 3: resultGate = !(resultGate && sourceGate); break; // NAND
            }
        }

        // 5. Update Final Gate State
        finalState[i].gate = resultGate;
        
        // 6. Detect Edges
        bool risingEdge = (finalState[i].gate && !prevFinalState[i].gate);
        
        // 7. Determine if we should trigger (Sample & Hold + Note On)
        // Trigger only if the logical gate result allows it
        uint8_t type = tracks[i].getType();
        bool isModType = (type == MOD || type == DRUM);
        
        // Triggers and continuous updates are only allowed if the final gate is open (or on rising edge)
        bool shouldTrig = risingEdge || (baseState[i].trig && finalState[i].gate);

        // For MOD tracks, we also want to allow continuous value updates even without a new trigger
        bool continuousUpdate = isModType && finalState[i].gate; 

        if (shouldTrig || continuousUpdate) {
            // Update CV/Pitch only on trigger or for continuous MOD
            finalState[i].note     = baseState[i].note;
            finalState[i].value    = baseState[i].value;
            finalState[i].accent   = baseState[i].accent;
            finalState[i].trig     = shouldTrig; 

            // Process CV Arithmetic Logic Slots (4: ADD, 5: SUB, 6: MULT, 7: MIX)
            noteStruct destffset = tracks[i].getTransposeOffset();
            int16_t calcNote   = finalState[i].note.note + destffset.note;
            int16_t calcOctave = finalState[i].note.octave + destffset.octave;
            int16_t calcValue  = finalState[i].value;

            for (uint8_t slot = 4; slot < 8; slot++)
            {
                uint8_t sourceIdx = ops.cvSources[slot - 4];
                if (sourceIdx == 255) continue; // Slot is OFF
                if (sourceIdx >= 12)  continue; // Sources are tracks 0-11

                EventState& source = baseState[sourceIdx];
                noteStruct sourceOffset = tracks[sourceIdx].getTransposeOffset();
                int16_t sourceNote   = source.note.note + sourceOffset.note;
                int16_t sourceOctave = source.note.octave + sourceOffset.octave;
                uint8_t sourceType = tracks[sourceIdx].getType();

                if (sourceType == NOTE) {
                    // Arithmetic with another NOTE track (Scale Index + Octave)
                    switch (slot) {
                        case 4: // ADD
                        {
                            calcNote   += sourceNote;
                            calcOctave += sourceOctave;
                            calcValue  += source.value;
                            break;
                        }
                        case 5: // SUB
                        {
                            calcNote   -= sourceNote;
                            calcOctave -= sourceOctave;
                            calcValue  -= source.value;
                            break;
                        }
                        case 6: // MIN
                        {
                            byte note = noteToMidi(finalState[i].note, false, &tracks[i]);
                            byte sNote = noteToMidi(source.note, true, &tracks[sourceIdx]);
                            if(note < sNote)
                            {
                                calcNote = finalState[i].note.note;
                                calcOctave = finalState[i].note.octave;
                            }
                            else
                            {
                                calcNote = sourceNote;
                                calcOctave = sourceOctave;
                            }
                        }
                        break;
                        case 7: // MAX
                        {

                            byte note = noteToMidi(finalState[i].note, false, &tracks[i]);
                            byte sNote = noteToMidi(source.note, true, &tracks[sourceIdx]);
                            if(note > sNote)
                            {
                                calcNote = finalState[i].note.note;
                                calcOctave = finalState[i].note.octave;
                            }
                            else
                            {
                                calcNote = sourceNote;
                                calcOctave = sourceOctave;
                            }
                        }
                        break;
                    }
                } else { 
                    // Arithmetic with a MOD/DRUM track (Value 0-255)
                    switch (slot) {
                        case 4: // ADD
                            calcNote   += (source.value >> 4);
                            calcOctave += (int16_t(source.value >> 5) - 4); // +/- 4 Octaves
                            calcValue  += source.value;
                            break;
                        case 5: // SUB
                            calcNote   -= (source.value >> 4);
                            calcOctave -= (int16_t(source.value >> 5) - 4);
                            calcValue  -= source.value;
                            break;
                        case 6: // MIN
                            calcNote   = min(calcNote, (int16_t)(source.value >> 4));
                            calcValue  = min(calcValue, (int16_t)source.value);
                            break;
                        case 7: // MAX
                            calcNote   = max(calcNote, (int16_t)(source.value >> 4));
                            calcValue  = max(calcValue, (int16_t)source.value);
                            break;
                    }
                }
            }

            // Final clamp and update
            finalState[i].note.note   = (calcNote < 0) ? 0 : (calcNote > 16 ? 16 : calcNote);
            finalState[i].note.octave = CLAMP(calcOctave, -4, 4);
            finalState[i].value        = CLAMP(calcValue, 0, 255);
        } else {
            // HOLD: Maintain previous physical values to prevent glitching
            finalState[i].note     = prevFinalState[i].note;
            finalState[i].value    = prevFinalState[i].value;
            finalState[i].accent   = prevFinalState[i].accent;
            finalState[i].trig     = false;
        }

        // 8. Final Note-Off logic
        // Trigger Note-Off if base says so (and logic allows it to close), 
        // OR if final logic gate just closed physically
        finalState[i].noteOff = (baseState[i].noteOff && !finalState[i].gate) || 
                                (!finalState[i].gate && prevFinalState[i].gate);
    }
}

// ── Phase 4: Dispatch ────────────────────────────────────────────────
// Reads finalState[] and writes to hardware.
void eventManagerClass::dispatch(trackClass* tracks)
{
    for (uint8_t i = 0; i < TRACK_NUMBER; i++)
    {
        EventState& a = finalState[i];
        uint8_t type      = tracks[i].getType();
        uint8_t gatePin   = tracks[i].getGateOut();
        int     cvOut     = tracks[i].getCVOut();
        uint8_t channel   = tracks[i].getMIDIoutChannel();
        uint8_t accentPin = tracks[i].getAccentOut();
        byte velocity = a.accent ? tracks[i].getHighVelocity() : tracks[i].getLowVelocity();
        // ── Accent output ────────────────────────────────────────────
        if (a.accent && a.trig)
            cuisine::gate_out::set(accentPin, HIGH);

        if (a.noteOff)
            cuisine::gate_out::set(accentPin, LOW);

        // ── Per-type dispatch ────────────────────────────────────────
        switch (type)
        {
            case NOTE:
            {
                // ── Polyphonic MIDI Output ──────────────────────────────
                // Compare current arp state with previous to detect note changes
                uint8_t currentCount = tracks[i].arp.getNumKeysPressed();
                uint8_t prevCount = _prevArpCount[i];
                bool liveHeld = (!a.override && liveInput[i].active && currentCount > 0);

                // If live input takes over (override OFF), kill any pending sequencer note
                if (!a.override && liveInput[i].active && _seqNoteActive[i])
                {
                    uint8_t midiNote = tracks[i].getLastNote();
                    cuisine::midi::sendNoteOff(midiNote, 0, channel);
                    // Tie gate life to this manual MIDI-off:
                    a.noteOff = true;      // so gateOut LOW happens below
                    a.gate    = false;     // optional but makes the state explicit
                    _seqNoteActive[i] = false;
                }

                if (!a.override)
                {
                    // If logical gate is closed, act as if no keys are pressed for MIDI output
                    uint8_t effectiveCount = a.gate ? currentCount : 0;

                    // Pre-calculate current MIDI notes to avoid redundant noteToMidi calls
                    uint8_t currentMidi[ARP_MAX_NOTES];
                    uint8_t prevN = prevCount < ARP_MAX_NOTES ? prevCount : ARP_MAX_NOTES;
                    uint8_t currN = effectiveCount < ARP_MAX_NOTES ? effectiveCount : ARP_MAX_NOTES;

                    for (uint8_t j = 0; j < currN; j++) {
                        currentMidi[j] = noteToMidi(tracks[i].arp.notes[j].note, tracks[i].getTranspose(), &tracks[i]);
                    }

                    // Single-pass diff using bitmasks
                    uint8_t prevMask = (prevN > 0) ? (uint8_t)((1u << prevN) - 1u) : 0;
                    uint8_t currMask = (currN > 0) ? (uint8_t)((1u << currN) - 1u) : 0;

                    // Match prev -> current (clear matched bits in both masks)
                    for (uint8_t j = 0; j < prevN; j++)
                    {
                        uint8_t prevMidi = _prevArpMidi[i][j];
                        for (uint8_t k = 0; k < currN; k++)
                        {
                            if (prevMidi == currentMidi[k])
                            {
                                prevMask &= (uint8_t)~(1u << j);  // matched, don't send note-off
                                currMask &= (uint8_t)~(1u << k);  // matched, don't send note-on
                                break;
                            }
                        }
                    }

                    // Send note-off for unmatched prev (bits still set in prevMask)
                    while (prevMask)
                    {
                        uint8_t j = __builtin_ctz(prevMask);
                        prevMask &= (uint8_t)(prevMask - 1);
                        uint8_t prevMidi = _prevArpMidi[i][j];
                        cuisine::midi::sendNoteOff(prevMidi, 0, channel);
                    }

                    // Send note-on for unmatched current (bits still set in currMask)
                    while (currMask)
                    {
                        uint8_t k = __builtin_ctz(currMask);
                        currMask &= (uint8_t)(currMask - 1);
                        uint8_t velocity = tracks[i].arp.notes[k].note.accent
                            ? tracks[i].getHighVelocity()
                            : tracks[i].getLowVelocity();
                        cuisine::midi::sendNoteOn(currentMidi[k], velocity, channel);
                    }

                    // Update previous arp state for next tick
                    _prevArpCount[i] = currN;
                    for (uint8_t j = 0; j < currN; j++)
                    {
                        _prevArpMidi[i][j] = currentMidi[j];
                    }
                }
                
                // ── Sequencer MIDI Output ────────────────────────────
                // When arp is empty OR override is ON, sequencer drives MIDI timing
                if ((currentCount == 0 || a.override) && !liveHeld)
                {
                        if (a.trig)
                        {
                            uint8_t midiNote = noteToMidi(a.note, false, &tracks[i]);
                        uint8_t velocity = a.accent ? tracks[i].getHighVelocity() : tracks[i].getLowVelocity();
                        cuisine::midi::sendNoteOn(midiNote, velocity, channel);
                        _seqNoteActive[i] = true;
                    }
                    //duration update
                    else if (a.gate && clockClass::getMasterTick())
                    {
                        if (a.duration > 0)
                        {
                            --a.duration;
                            if (a.duration == 0)
                            {
                                a.gate = false;
                                a.noteOff = true;
                            }
                        }
                    }
                    if (a.noteOff)
                    {
                        uint8_t midiNote = tracks[i].getLastNote();
                        cuisine::midi::sendNoteOff(midiNote, 0, channel);
                        _seqNoteActive[i] = false;
                    }
                }
                // ── Handle live note-off even when other notes are held ─────
                // This ensures note-off is sent even if note wasn't found in arp
                else if (a.noteOff && !liveInput[i].active && currentCount > 0)
                {
                    // Send note-off for the note that was released
                    // Use the note from liveInput which was set when note-off occurred
                    uint8_t midiNote = noteToMidi(liveInput[i].note, tracks[i].getTranspose(), &tracks[i]);
                    cuisine::midi::sendNoteOff(midiNote, 0, channel);
                }
                // ── Monophonic CV/Gate Output ────────────────────────────
                // CV/Gate follows finalState (from sequencer or arp)
                if (a.trig)
                {
                    uint8_t midiNote = noteToMidi(a.note, false, &tracks[i]);
                    tracks[i].writeLastNote(midiNote);

                    // CV/Gate output (monophonic, follows arp cycling)
                    cuisine::gate_out::set(gatePin, HIGH);
                    cuisine::cv::setNote(cvOut, midiNote);
                }
                
                // Note-off: gate LOW
                if (a.noteOff)
                {
                    cuisine::gate_out::set(gatePin, LOW);
                }
                break;
            }
            case MOD:
            {
                // 1. Triggered Output (New step or key press or pot move or value change)
                if (a.trig || a.value != prevFinalState[i].value)
                {
                    // Update CV (0-255 scaled to 12-bit)
                    cuisine::cv::setRaw(cvOut, (unsigned int)a.value << 4);
                    // Send MIDI CC (0-255 scaled to 7-bit)
                    cuisine::midi::sendControlChange(tracks[i].getCC(), a.value >> 1, channel);
                    
                    if (a.trig) cuisine::gate_out::set(gatePin, HIGH);
                    _seqNoteActive[i] = true;
                }
                // 2. Duration Timer (Gate length)
                else if (a.gate && clockClass::getMasterTick())
                {
                    if (a.duration > 0)
                    {
                        --a.duration;
                        if (a.duration == 0)
                        {
                            a.gate = false;
                            a.noteOff = true;
                        }
                    }
                }

                // 3. Gate Off
                if (a.noteOff)
                {
                    cuisine::gate_out::set(gatePin, LOW);
                    _seqNoteActive[i] = false;
                }

                break;
            }
            case DRUM:
            {
                if (a.trig)
                {
                    uint8_t note = tracks[i].getNoteSelected();
                    cuisine::cv::setRaw(cvOut, (unsigned int)a.value << 4);
                    uint8_t velocity = a.value >> 1;
                    cuisine::midi::sendNoteOn(note, velocity, channel);

                    // 5ms fixed trigger pulse
                    cuisine::gate_out::set(gatePin, HIGH);
                    _drumTrigStart[i]  = millis();
                    _drumTrigPin[i]    = gatePin;
                    _drumTrigActive[i] = true;
                }
                if (a.noteOff && !_drumTrigActive[i])
                {
                    uint8_t note = tracks[i].getNoteSelected();
                    cuisine::midi::sendNoteOff(note, 0, channel);
                    cuisine::gate_out::set(gatePin, LOW);
                }
                break;
            }
        }

        // ── Internal Modulation Matrix ───────────────────────────────
        // Update current value so other tracks can see it as a source
        tracks[i].modulationOut[i] = a.value;
    }
}

// ── Drum Trigger Timer ───────────────────────────────────────────────
void eventManagerClass::updateDrumTriggers()
{
    unsigned long now = millis();
    for (uint8_t i = 0; i < TRACK_NUMBER; i++)
    {
        if (_drumTrigActive[i] && (now - _drumTrigStart[i] >= DRUM_TRIG_MS))
        {
            cuisine::gate_out::set(_drumTrigPin[i], LOW);
            _drumTrigActive[i] = false;
        }
    }
}

void eventManagerClass::startDrumTrig(uint8_t trackIndex, uint8_t gatePin)
{
    cuisine::gate_out::set(gatePin, HIGH);
    _drumTrigStart[trackIndex]  = millis();
    _drumTrigPin[trackIndex]    = gatePin;
    _drumTrigActive[trackIndex] = true;
}
