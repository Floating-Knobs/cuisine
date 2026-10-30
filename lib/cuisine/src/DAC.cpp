// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <arduino.h>
#include <DAC.h>
#include <SPI.h>
SPISettings DACclass::_spi_settings = SPISettings(F_CPU, MSBFIRST, SPI_MODE0);

static int DACclass::_DACvalue[4] = {0, 0, 0, 0};
static int DACclass::_oldDACvalue[4] = {0, 0, 0, 0};

#define mV_PER_NOTE 83333
void DACclass::init()
{
    pinMode(SSDAC0, OUTPUT);
    pinMode(SSDAC1, OUTPUT);
    digitalWriteFast(SSDAC0, HIGH);
    digitalWriteFast(SSDAC1, HIGH);
    _spi_settings = SPISettings(F_CPU, MSBFIRST, SPI_MODE0);
    SPI.begin();
    DACclass::_write(1, 0);
    DACclass::_write(0, 0);
    DACclass::_write(2, 0);
    DACclass::_write(3, 0);
}
void DACclass::_write(int channel, unsigned int value)
{
    if (channel < 0 || channel > DAC_CHANNELS) { return; }

    bool DACchip = bitRead(channel, 1);
    bool DACnumber = bitRead(channel, 0);

    bitWrite(value, 15, DACnumber);
    bitWrite(value, 12, true);

    if (DACchip) { digitalWriteFast(SSDAC1, LOW); }
    else { digitalWriteFast(SSDAC0, LOW); }
    SPI.beginTransaction(_spi_settings);
    SPI.transfer16(value);
    if (DACchip) { digitalWriteFast(SSDAC1, HIGH); }
    else { digitalWriteFast(SSDAC0, HIGH); }
    SPI.endTransaction();
}
void DACclass::writeVoct(int channel, byte value)
{
    long temp = (long)value * mV_PER_NOTE;
    unsigned int Voct = (unsigned int)(temp / 2000);
    if (Voct > 4095) Voct = 4095;
    DACclass::write(channel, Voct);
}
void DACclass::write(int channel, unsigned int value)
{
    if(channel > DAC_CHANNELS || channel < 0) {return;}
    _DACvalue[channel] = value;
}
void DACclass::update()
{
    for(int i = 0; i < DAC_CHANNELS; i++)
    {
        if(_DACvalue[i] != _oldDACvalue[i])
        {
            DACclass::_write(i, _DACvalue[i]);
            _oldDACvalue[i] = _DACvalue[i];
        }
    }
}
