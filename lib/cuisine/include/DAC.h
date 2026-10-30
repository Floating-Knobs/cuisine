// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <SPI.h>
#include <board.h>

#ifndef DAC_H
#define DAC_H

class DACclass
{
    public:
        static void init();
        static void write(int channel, unsigned int value);
        
        static void writeVoct(int channel, byte value);
        static void update();
    private:
        static void _write(int channel, unsigned int value);
        static SPISettings _spi_settings;
        static int _DACvalue[4];
        static int _oldDACvalue[4];
};

namespace cuisine {
namespace cv {

/// Raw DAC counts, 0..4095. Channel 0..3.
inline void setRaw(uint8_t channel, uint16_t counts) {
    DACclass::write(channel, counts);
}

/// 1 V/oct semitone, using the DAC's note scaling. Channel 0..3.
inline void setNote(uint8_t channel, uint8_t midiNote) {
    DACclass::writeVoct(channel, midiNote);
}

} // namespace cv
} // namespace cuisine

#endif
