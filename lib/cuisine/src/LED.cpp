// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <LED.h>

// Private names for the 2-bit colour mask, used only inside this driver.
namespace {
constexpr uint8_t ON     = 1;
constexpr uint8_t OFF    = 0;
constexpr uint8_t RED    = 1;
constexpr uint8_t GREEN  = 2;
constexpr uint8_t YELLOW = 3;

// Driver-only constants, previously kBlinkTime and kPwm macros in the header.
constexpr uint16_t kBlinkTime = 500;  // ms
constexpr uint16_t kPwm       = 0b1000010000010000;

// F_CPU/2 is the AVR's SPI ceiling. Used through transactions so the shift
// registers cannot silently leave the bus at a different divider.
SPISettings s_ledSettings(F_CPU / 2, MSBFIRST, SPI_MODE0);
}

// #include <elapsed.h>
// cuisine::Micros pwmTimer; // cuisine::Micros object to manage kPwm timing
// bool pwm(byte value) {
  
//   static int pwmDuration = 0; // Duration of kPwm HIGH state in microseconds

//   // Calculate the duration of the kPwm HIGH state based on the value (0 to 255)
//   pwmDuration = map(4, 0, PWM_RESOLUTION, 0, PWM_PERIOD);

//   // Check if the kPwm timer exceeds the kPwm period
//   if (pwmTimer >= PWM_PERIOD) {
//     // Reset the kPwm timer
//     pwmTimer -= PWM_PERIOD;
//   }

//   // Return true if the kPwm timer is within the kPwm HIGH duration, else false
//   return (pwmTimer < pwmDuration);
// }

// static bool LEDclass::_LEDtable[16][2] = {true};
static unsigned int LEDclass::_redLED = {0};
static unsigned int LEDclass::_greenLED = {0};
// static unsigned int LEDclass::_orangeLED = {0};
static unsigned int LEDclass::_dimLED = {0};

static unsigned int LEDclass::_oldRedLED = {0};
static unsigned int LEDclass::_oldGreenLED = {0};
// static unsigned int LEDclass::_oldOrangeLED = {0};
static unsigned int LEDclass::_oldDimLED = {0};
static bool LEDclass::_updateFlagStep = {false};
static byte LEDclass::_transportLED = {0};
static byte LEDclass::_oldTransportLED = {0};
static cuisine::Millis LEDclass::_blink = {0};
static cuisine::Millis LEDclass::_blinkLow = {0};
static cuisine::Millis LEDclass::_shortBlink = {0};
static unsigned int LEDclass::_PWM = {kPwm};

static void LEDclass::setAll(unsigned int data, byte color, bool dim = false)
{
  color = color & 3;

  // bitWrite(_dimLED, position, dim);
  
  // Set LEDs based on color using bitwise operations
  _redLED = (color & RED) ? 0xFFFF & data : 0;
  _greenLED = (color & GREEN) ? 0xFFFF & data : 0;
  _dimLED = dim ? 0xFFFF & data : 0;
}
static void LEDclass::setSome(unsigned int data, byte color, bool dim = false)
{

  color = color & 3;

  _redLED &= ~data;
  _greenLED &= ~data;
  _dimLED = dim ? _dimLED & ~data : _dimLED;
  // bitWrite(_dimLED, position, dim);
  
  // Set LEDs based on color using bitwise operations
  _redLED = (color & RED) ? _redLED | data : _redLED;
  _greenLED = (color & GREEN) ? _greenLED | data : _greenLED;
  _dimLED = dim ? _dimLED | data : _dimLED;


  // for (int y = 0; y < LEDnumber; y++)
  // {
  //   if (bitRead(data, y)) { setOne(y, color, dim); }
  // }
}
static void LEDclass::setOne(byte position, byte color, bool dim = false)
{
  if(position > 15) {return; }
  _updateFlagStep = true;

  color = color & 3;
  bitWrite(_dimLED, position, dim);
  
  // Set LEDs based on color using bitwise operations
  bitWrite(_redLED, position, (color & RED));
  
  bitWrite(_greenLED, position, (color & GREEN));
  // Serial.println(_greenLED, BIN);
  // _redLED |= (color & RED) << position;
  // _greenLED |= (((color & GREEN) >> 1)) << position;
}
static void LEDclass::setPage(byte size, byte selected, byte maxPage, byte current, bool special = false)
{
  for (byte i = 0; i < maxPage; i++)
  {
    byte adress = i + REC_LED + 2;
    (i <= size) ? LEDclass::setTransport(adress, ON) : LEDclass::setTransport(adress, OFF);
    
    (i == current) && !special ? LEDclass::setTransport(adress, blinkLow()) : (void)0;
    (i == selected) ? LEDclass::setTransport(adress, blink()) : (void)0;
  }
  for( byte i = maxPage; i < 4; i++)
  {
    byte adress = i + REC_LED + 2;
    LEDclass::setTransport(adress, OFF);
  }
}
// static void LEDclass::resetAll() { memset(_LEDtable, 0, sizeof(_LEDtable, 0)); }
// static void LEDclass::resetOne(byte position) { memset(_LEDtable[position], 0, sizeof(_LEDtable[position], 0)); }

static void LEDclass::init()
{
  pinMode(SSLedtransportPin, OUTPUT);
  pinMode(SSLedPin, OUTPUT);

  pinMode(PWM_RED, OUTPUT);
  pinMode(PWM_GREEN, OUTPUT);
  
  digitalWriteFast(SSLedtransportPin, HIGH);
  digitalWriteFast(SSLedPin, HIGH);

  
  // digitalWriteFast(PWM_RED, HIGH);
  // digitalWriteFast(PWM_GREEN, HIGH);

  // analogWriteFrequency(8);
  // analogWrite(PWM_RED, 0);

  // analogWrite(PWM_GREEN, 0);

  SPI.begin();
  // SPI.setClockDivider(SPI_CLOCK_DIV2);
  setAll(0b1111111111111111, OFF);
  update(1);
}
static void LEDclass::update(byte brightness, bool forceUpdate = false) // max 31
{
  //update of the transport LED if new value
  if((_transportLED != _oldTransportLED) || forceUpdate) 
  { 

    _oldTransportLED = _transportLED;
    digitalWriteFast(SSLedtransportPin, LOW);
    SPI.beginTransaction(s_ledSettings);
    SPI.transfer(_transportLED);
    SPI.endTransaction();
    digitalWriteFast(SSLedtransportPin, HIGH);

    
  }

  
  // global brightness frame:
  brightness = brightness | 0b11100000;

  if(!dim())
  {
    _redLED &=  ~_dimLED;
    _greenLED &= ~_dimLED;
  }
  // update of the 16 LED
  if(_oldDimLED != _dimLED
    || _oldRedLED != _redLED
    || _oldGreenLED != _greenLED
    || forceUpdate
  )
  {
    _oldDimLED = _dimLED;
    _oldRedLED = _redLED;
    _oldGreenLED = _greenLED;
    // _updateFlagStep = false;
    unsigned int redLED = _redLED;
    unsigned int greenLED = _greenLED;
    
    SPI.beginTransaction(s_ledSettings);
    // Send data to first register (green)
    
    // SPI.transfer16(_greenLED & !_orangeLED);
    // int out = dim() ? _greenLED : (_greenLED & 0);
    SPI.transfer16(greenLED);
    // SPI.transfer(255);
    // SPI.transfer(255);
    // SPI.transfer(255);
    // SPI.transfer(255);

    // Send data to second register (red)
    // out = dim() ? _redLED : (_redLED & 0);
    SPI.transfer16(redLED);
    
    digitalWriteFast(SSLedPin, HIGH);
    digitalWriteFast(SSLedPin, LOW);
    SPI.endTransaction();

    // while(true)
    // {
    //   digitalWriteFast(PWM_RED, HIGH);
    //   digitalWriteFast(PWM_GREEN, HIGH);
    //   delay(5);
    //   digitalWriteFast(PWM_RED, LOW);
    //   digitalWriteFast(PWM_GREEN, LOW);
    //   delay(5);
    // }
  }
  

}

bool LEDclass::blink()
{
  if (_blink >= kBlinkTime) { _blink -= kBlinkTime;}
  return ((_blink < (kBlinkTime / 2)) ? true : false);
}
#define BLONK_LOW_TIME 80
bool LEDclass::blinkLow()
{
  if (_blinkLow >= BLONK_LOW_TIME) { _blinkLow -= BLONK_LOW_TIME;}
  return ((_blinkLow < (BLONK_LOW_TIME / 12)) ? true : false);
}
#define SHORT_BLINK_TIME 8
bool LEDclass::shortBlink()
{
  if (_shortBlink >= SHORT_BLINK_TIME) { _shortBlink -= SHORT_BLINK_TIME;}
  return ((_shortBlink < (SHORT_BLINK_TIME / 8)) ? true : false);
}
static bool LEDclass::dim()
{
  bool out = bitRead(_PWM, 0);
  _PWM = _PWM >> 1;
  if (_PWM == 0) { _PWM = kPwm; }
  return out;
}