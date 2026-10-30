// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <trackClock.h>
#include <gateIn.h>

// Bodies moved verbatim from clockClass, step 3 of the clock split.

unsigned int trackClock::getStepDuration()
{
  return getTimeDivision(_timeDivision);
}

unsigned int trackClock::getTimeDivision(byte value)
{
  _timeDivisionType = value & 1; // keep only first bit
  return cuisine::clock::ticksPerStep(value);
}

void trackClock::advanceOneMasterTick()
{
  if (_source != MASTER || !getTransportFlag()) return;
  const unsigned int stepDuration = getStepDuration();
  _clock++;
  _clock %= stepDuration;
  int phase = _clock;
  #ifdef OFFSET
  phase = wrap(phase - _clockOffset, 0, ((stepDuration + 1) * 2) - 1);
  #endif
  _tick = (phase % stepDuration) == 0;
}

void trackClock::update(bool advanceMaster, bool trackReset)
{
  _oldResetFlag = _resetFlag;
  _resetFlag = false;
  if (cuisine::clock::transportStopped()) { setTransportFlag(false); }
  else if (cuisine::clock::transportStarted() && getPlayMode() == NORMAL) { setTransportFlag(true); }

  switch (_resetSource)
  {
    // Note: the MASTER reset-source case used to compute a division and
    // discard it — it never reset anything. Removed rather than left looking
    // implemented; a master-resolution reset is still an open feature.
    case TRIGGED:
    {
      // Track triggers are resolved by the owning track and passed in as
      // trackReset; only external gate inputs are handled here.
      if(trackReset) { reset(); }
      else if(_resetInput >= 12 && _resetInput < 16)
      {
        byte input = _resetInput - 12;
        if (cuisine::gate_in::rising(input)) { reset(); }
      }
      break;
    }
    default: break;
  }
  if(!getTransportFlag()) { _clock = -1; }

  switch (_source)
  {
    case MASTER:
    {
      if (advanceMaster && cuisine::clock::tick() & getTransportFlag())
      {
        advanceOneMasterTick();
      }
      break;
    }
    case COUPLED:
    {
      //nothing to do here, happens in trackClass::update()
      break;
    }
    case TRIGGED:
    {
      //nothing to do here, happens in trackClass::update()
      break;
    }
    case LOOPER:
    default: { break; }
  }
}
