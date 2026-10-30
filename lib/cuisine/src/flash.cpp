// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

// SPI NOR driver for the ZD25Q80C (1 MB) on the Cuisine board.
//
// Commands: read 0x03, page program 0x02, sector erase 0x20 (4 KB), block erase
// 0x52 (32 KB), write enable 0x06, read status 0x05, JEDEC ID 0x9F.
//
// Bus notes: the MCU is 5 V and the flash is 3.3 V, with 2.2k/3.3k dividers on
// SCK, MOSI and HOLD. F_CPU/2 is the fastest the AVR can drive SPI; the part
// itself allows 104 MHz. HOLD is driven high at init.
//
// This replaces LowPowerLab/SPIFlash, which is GPL-3.0.

#include <Arduino.h>
#include <SPI.h>

#include <storage.h>

namespace {

constexpr uint8_t kCmdWriteEnable = 0x06;
constexpr uint8_t kCmdReadStatus  = 0x05;
constexpr uint8_t kCmdRead        = 0x03;
constexpr uint8_t kCmdPageProgram = 0x02;
constexpr uint8_t kCmdSectorErase = 0x20;  // 4 KB
constexpr uint8_t kCmdBlockErase  = 0x52;  // 32 KB
constexpr uint8_t kCmdJedecId     = 0x9F;

constexpr uint8_t kStatusBusy = 0x01;
constexpr uint8_t kStatusWel  = 0x02;

constexpr uint16_t kPageSize   = 256;
constexpr uint32_t kSectorSize = 4096;
constexpr uint32_t kBlockSize  = 32768;

// Datasheet maxima with margin: page program 3 ms, 4 KB erase 400 ms,
// 32 KB block erase 1 s.
constexpr uint32_t kProgramTimeoutUs     = 5000;
constexpr uint32_t kSectorEraseTimeoutUs = 500000;
constexpr uint32_t kBlockEraseTimeoutUs  = 1000000;

SPISettings s_settings(F_CPU / 2, MSBFIRST, SPI_MODE0);

} // namespace

namespace cuisine {
 namespace storage {

void Flash::select()   { digitalWrite(_cs, LOW); }
void Flash::deselect() { digitalWrite(_cs, HIGH); }

uint8_t Flash::transfer(uint8_t value) { return SPI.transfer(value); }

void Flash::sendAddress(uint32_t address) {
    transfer((uint8_t)(address >> 16));
    transfer((uint8_t)(address >> 8));
    transfer((uint8_t)(address));
}

uint8_t Flash::status() {
    SPI.beginTransaction(s_settings);
    select();
    transfer(kCmdReadStatus);
    const uint8_t value = transfer(0xFF);
    deselect();
    SPI.endTransaction();
    return value;
}

bool Flash::writeEnable() {
    SPI.beginTransaction(s_settings);
    select();
    transfer(kCmdWriteEnable);
    deselect();
    SPI.endTransaction();
    return (status() & kStatusWel) != 0;
}

bool Flash::waitReady(uint32_t timeoutUs) {
    const uint32_t start = micros();
    while ((status() & kStatusBusy) != 0) {
        if ((uint32_t)(micros() - start) > timeoutUs) { return false; }
    }
    return true;
}

bool Flash::initialize() {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    pinMode(holdFlashPin, OUTPUT);
    digitalWrite(holdFlashPin, HIGH);

    SPI.begin();

    SPI.beginTransaction(s_settings);
    select();
    transfer(kCmdJedecId);
    const uint8_t manufacturer = transfer(0xFF);
    const uint8_t memoryType   = transfer(0xFF);
    const uint8_t density      = transfer(0xFF);
    deselect();
    SPI.endTransaction();

    (void)memoryType;
    _ready = false;
    _size  = 0;

    if (manufacturer == 0x00 || manufacturer == 0xFF) { return false; }  // floating bus
    if (density < 0x10 || density > 0x1F) { return false; }

    _size = 1UL << density;  // 0x14 -> 1 MB
    if (_size > 0x100000UL) { _size = 0x100000UL; }

    _ready = true;
    return true;
}

void Flash::readBytes(uint32_t address, uint8_t* buffer, uint16_t length) {
    if (!_ready || buffer == nullptr || length == 0) { return; }

    SPI.beginTransaction(s_settings);
    select();
    transfer(kCmdRead);
    sendAddress(address);
    for (uint16_t i = 0; i < length; i++) { buffer[i] = transfer(0xFF); }
    deselect();
    SPI.endTransaction();
}

void Flash::writeBytes(uint32_t address, const uint8_t* buffer, uint16_t length) {
    if (!_ready || buffer == nullptr) { return; }

    while (length > 0) {
        if (address >= _size) { return; }

        // A page program must not cross a 256 B page boundary.
        const uint32_t room  = kPageSize - (address % kPageSize);
        const uint16_t chunk = (length < room) ? length : (uint16_t)room;

        if (!writeEnable()) { return; }

        SPI.beginTransaction(s_settings);
        select();
        transfer(kCmdPageProgram);
        sendAddress(address);
        for (uint16_t i = 0; i < chunk; i++) { transfer(buffer[i]); }
        deselect();
        SPI.endTransaction();

        if (!waitReady(kProgramTimeoutUs)) { return; }

        address += chunk;
        buffer  += chunk;
        length  -= chunk;
    }
}

void Flash::eraseCommand(uint8_t command, uint32_t address,
                         uint32_t alignment, uint32_t timeoutUs) {
    if (!_ready || !writeEnable()) { return; }

    SPI.beginTransaction(s_settings);
    select();
    transfer(command);
    sendAddress(address & ~(alignment - 1));
    deselect();
    SPI.endTransaction();

    waitReady(timeoutUs);
}

void Flash::sectorErase(uint32_t address) {
    eraseCommand(kCmdSectorErase, address, kSectorSize, kSectorEraseTimeoutUs);
}

void Flash::blockErase32K(uint32_t address) {
    eraseCommand(kCmdBlockErase, address, kBlockSize, kBlockEraseTimeoutUs);
}

} // namespace storage
} // namespace cuisine
