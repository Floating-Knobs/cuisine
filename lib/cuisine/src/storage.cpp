// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>
#include <SPI.h>

#include <storage.h>

namespace {

// The bus is shared and level-shifted (5 V MCU, 3.3 V memories). F_CPU/2 is the
// fastest the AVR can drive SPI; the part itself allows 25 MHz.
SPISettings s_settings(F_CPU / 2, MSBFIRST, SPI_MODE0);

constexpr uint8_t kFramRead  = 0x03;
constexpr uint8_t kFramWrite = 0x02;
constexpr uint8_t kFramWren  = 0x06;
constexpr uint16_t kFramPage = 16;

} // namespace

namespace cuisine {
 namespace storage {

void Fram::init() {
    // HOLD must be high, or every transfer pauses mid-frame. The application
    // used to do this; the driver does it now so init() is enough.
    pinMode(holdFramPin, OUTPUT);
    digitalWrite(holdFramPin, HIGH);

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
}

void Fram::readBytes(uint32_t address, void* buffer, uint16_t length) {
    if (buffer == nullptr || length == 0) { return; }
    uint8_t* out = static_cast<uint8_t*>(buffer);

    SPI.beginTransaction(s_settings);
    digitalWrite(_cs, LOW);
    SPI.transfer(kFramRead);
    SPI.transfer((uint8_t)(address >> 8));
    SPI.transfer((uint8_t)address);
    for (uint16_t i = 0; i < length; i++) { out[i] = SPI.transfer(0xFF); }
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void Fram::writeBytes(uint32_t address, const void* buffer, uint16_t length) {
    if (buffer == nullptr || length == 0) { return; }
    const uint8_t* in = static_cast<const uint8_t*>(buffer);

    // This part does not accept a WRITE without a preceding WREN, even though
    // the Fujitsu datasheet says WREN is optional. Verified on the bench: the
    // status register was 0x80 (WPEN) and every write was silently dropped.
    while (length > 0) {
        const uint16_t room  = kFramPage - (address % kFramPage);
        const uint16_t chunk = (length < room) ? length : room;

        SPI.beginTransaction(s_settings);
        digitalWrite(_cs, LOW);
        SPI.transfer(kFramWren);
        digitalWrite(_cs, HIGH);
        SPI.endTransaction();

        SPI.beginTransaction(s_settings);
        digitalWrite(_cs, LOW);
        SPI.transfer(kFramWrite);
        SPI.transfer((uint8_t)(address >> 8));
        SPI.transfer((uint8_t)address);
        for (uint16_t i = 0; i < chunk; i++) { SPI.transfer(in[i]); }
        digitalWrite(_cs, HIGH);
        SPI.endTransaction();

        // Wait for WIP to clear, bounded so a dead chip cannot hang the app.
        for (uint16_t guard = 0; guard < 1000; guard++) {
            if ((status() & 0x01) == 0) { break; }
        }

        address += chunk;
        in      += chunk;
        length  -= chunk;
    }
}

uint8_t Fram::read(uint32_t address) {
    uint8_t value = 0;
    readBytes(address, &value, 1);
    return value;
}

void Fram::write(uint32_t address, uint8_t value) {
    writeBytes(address, &value, 1);
}

uint8_t Fram::status() {
    SPI.beginTransaction(s_settings);
    digitalWrite(_cs, LOW);
    SPI.transfer(0x05);
    const uint8_t value = SPI.transfer(0xFF);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    return value;
}

void Fram::writeStatus(uint8_t value) {
    // Write enable, then write the status register.
    SPI.beginTransaction(s_settings);
    digitalWrite(_cs, LOW);
    SPI.transfer(0x06);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();

    SPI.beginTransaction(s_settings);
    digitalWrite(_cs, LOW);
    SPI.transfer(0x01);
    SPI.transfer(value);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();

    // The status write takes a few ms on FRAM.
    delay(5);
}

Fram  fram(SSFram);
Flash flash(SSFlash);

} // namespace storage
} // namespace cuisine
