// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <board.h>

// Storage layer: the app talks to these two objects, never to a chip driver.
// Both drivers are our own, so the firmware has no third-party dependency here.
namespace cuisine {
 namespace storage {

// FRAM: MB85RS256, 32 KB, byte addressable, no erase.
// Read 0x03, write 0x02, two address bytes.
class Fram
{
    public:
        explicit Fram(byte cs) : _cs(cs) {}

        void init();

        void readBytes(uint32_t address, void* buffer, uint16_t length);
        void writeBytes(uint32_t address, const void* buffer, uint16_t length);

        uint8_t read(uint32_t address);
        void    write(uint32_t address, uint8_t value);

        // Status register: bit 7 WPEN, bits 3..2 BP1/BP0. Non-volatile, so a
        // previous firmware can leave the chip write-protected.
        uint8_t status();
        void    writeStatus(uint8_t value);

        // Same convention as EEPROM_SPI_WE: fills the argument in place and
        // also returns it. Call sites ignore the return and read the argument,
        // so this must NOT take a const reference.
        template <typename T> T get(uint32_t address, T& value) {
            readBytes(address, &value, sizeof(T));
            return value;
        }
        template <typename T> void put(uint32_t address, const T& value) {
            writeBytes(address, &value, sizeof(T));
        }

        // Names kept from the previous wrapper.
        void readBlock(uint32_t address, void* buffer, uint16_t length) {
            readBytes(address, buffer, length);
        }
        void writeBlock(uint32_t address, const void* buffer, uint16_t length) {
            writeBytes(address, buffer, length);
        }

    private:
        byte _cs;
};

// SPI flash: ZD25Q80C, 1 MB, 256 B pages, 4 KB sectors, 32 KB blocks.
// F_CPU/2 (10 MHz at 20 MHz), HOLD driven high at init.
class Flash
{
    public:
        explicit Flash(byte cs) : _cs(cs) {}

        bool     initialize();
        void     readBytes(uint32_t address, uint8_t* buffer, uint16_t length);
        void     writeBytes(uint32_t address, const uint8_t* buffer, uint16_t length);
        void     blockErase32K(uint32_t address);
        void     sectorErase(uint32_t address);

        uint32_t size() const  { return _size; }
        bool     ready() const { return _ready; }

    private:
        void    select();
        void    deselect();
        uint8_t transfer(uint8_t value);
        void    sendAddress(uint32_t address);
        uint8_t status();
        bool    writeEnable();
        bool    waitReady(uint32_t timeoutUs);
        void    eraseCommand(uint8_t command, uint32_t address,
                             uint32_t alignment, uint32_t timeoutUs);

        byte     _cs;
        uint32_t _size  = 0;
        bool     _ready = false;
};

extern Fram  fram;
extern Flash flash;

} // namespace storage
} // namespace cuisine

#endif
