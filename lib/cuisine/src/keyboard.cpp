// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <keyboard.h>

volatile byte keyboardClass::_multiplexerChannel = 0;
volatile keyStruct keyboardClass::_step;
volatile keyStruct keyboardClass::_command;
volatile bool keyboardClass::_armSwitch = 0;

// Scan one multiplexer channel - called by ISR
void keyboardClass::scanChannel()
{
    // Read the channel that was set in the PREVIOUS interrupt
    // (multiplexer has had time to settle)
    // read first switch 1 time out of 2 to have more stable reading
    byte channelToRead = _multiplexerChannel & 1 ? 0 : _multiplexerChannel >> 1;
    unsigned int bit = (1 << channelToRead);

    bool commandState = digitalReadFast(commandPin);
    bool keyState = digitalReadFast(keysPin);
    
    // Update bitmasks for the channel we just read
    _command.oldScan = _command.scan;
    if (commandState) {
        _command.scan |= bit;
    } else {
        _command.scan &= ~bit;
    }
    _command.scan &= 0b0011111111111111; // clear unused bits
    
    _command.pendingRise |= (_command.scan & ~_command.oldScan);
    _command.pendingFall |= (~_command.scan & _command.oldScan);
    _command.pendingHigh |= _command.oldScan;
    _command.pendingLow  |= ~_command.oldScan;

    _step.oldScan = _step.scan;
    if (keyState) {
        _step.scan |= bit;
    } else {
        _step.scan &= ~bit;
    }
    
    _step.pendingRise |= (_step.scan & ~_step.oldScan);
    _step.pendingFall |= (~_step.scan & _step.oldScan);
    _step.pendingHigh |= _step.oldScan;
    _step.pendingLow  |= ~_step.oldScan;
    
    // Read arm switch (not multiplexed)
    _armSwitch = digitalReadFast(armPin);
    
    // Advance to next channel and set multiplexer for NEXT interrupt
    _multiplexerChannel++;
    if (_multiplexerChannel >= 32) 
    {
        _multiplexerChannel = 0;
    }
    
    // Set multiplexer channel for NEXT interrupt (will settle during interrupt period)
    PORTE_OUT = _multiplexerChannel & 1 ? 0 : _multiplexerChannel >> 1;
}

// ISR for TCB0 - scans one multiplexer channel per interrupt
ISR(TCB0_INT_vect)
{
    keyboardClass::scanChannel();
    // Clear interrupt flag
    TCB0.INTFLAGS = TCB_CAPT_bm;
}


void keyboardClass::init()
{
  // Set multiplexer control PORT as out
  PORTE_DIR |= 0xF;
  // Set switch pin as input 
  pinMode(keysPin, INPUT_PULLUP);
  pinMode(commandPin, INPUT_PULLUP);
  pinMode(armPin, INPUT_PULLUP);

  //invert input (low = high)
  PORTD_PIN0CTRL = PORTD_PIN0CTRL | 0b10000000;
  PORTD_PIN1CTRL = PORTD_PIN1CTRL | 0b10000000;

  // Initialize multiplexer to channel 0
  _multiplexerChannel = 0;
  PORTE_OUT = 0;
  
  // Configure TCB0 for a periodic interrupt. CLK_PER/2 is 10 MHz at 20 MHz, so
  // CCMP = 600 gives 60 us. The old comment said 150 us; that was 16 MHz.
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; // Periodic interrupt mode (set before CTRLA)
  TCB0.CCMP = 600; // 60 us at 20 MHz with /2 prescaler
  TCB0.CNT = 0; // Reset counter to 0
  TCB0.INTCTRL = TCB_CAPT_bm; // Enable interrupt
  TCB0.INTFLAGS = TCB_CAPT_bm; // Clear any pending flags
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV2_gc; // Enable last, CLK_PER/2
  // Timer starts counting immediately after enable
  // First scan cycle will complete in ~2.4ms (16 interrupts * 150us)
}
void keyboardClass::update()
{
    // Update values synchronously with main loop for edge detection
    // This ensures values represent transitions since the last update() call
    cli(); // Disable interrupts briefly
    _step.rise = _step.pendingRise;
    _step.fall = _step.pendingFall;
    _step.high = _step.pendingHigh;
    _step.low  = _step.pendingLow;
    
    _step.pendingRise = 0;
    _step.pendingFall = 0;
    _step.pendingHigh = _step.scan;
    _step.pendingLow  = ~_step.scan;

    _command.rise = _command.pendingRise;
    _command.fall = _command.pendingFall;
    _command.high = _command.pendingHigh;
    _command.low  = _command.pendingLow;
    
    _command.pendingRise = 0;
    _command.pendingFall = 0;
    _command.pendingHigh = _command.scan;
    _command.pendingLow  = ~_command.scan;
    sei(); // Re-enable interrupts
}
unsigned int keyboardClass::stepRead()
{
  return _step.high;
}
unsigned int keyboardClass::stepReadTrig()
{
  return _step.rise;
}
unsigned int keyboardClass::stepReadTrigOff()
{
  return _step.fall;
}
unsigned int keyboardClass::commandRead()
{
  return _command.high;
}
unsigned int keyboardClass::commandReadTrig()
{
  return _command.rise;
}
bool keyboardClass::stepReadOne(byte number)
{
  return bitRead(_step.high, number);
}
bool keyboardClass::stepReadOneTrig(byte number)
{
  return bitRead(_step.rise, number);
}
bool keyboardClass::stepReadOneTrigOff(byte number)
{
  return bitRead(_step.fall, number);
}
bool keyboardClass::commandReadOne(unsigned int number)
{
  return number & _command.high;
}
bool keyboardClass::commandReadOneTrig(unsigned int number)
{
  return number & _command.rise;
}
bool keyboardClass::commandReadOneTrigOff(unsigned int number)
{
  return number & _command.fall;
}
bool keyboardClass::commandReadNo(byte number)
{
  return bitRead(_command.high, number);
}
bool keyboardClass::arm()
{
  return _armSwitch;
}