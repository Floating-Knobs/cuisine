// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

// Serial bench protocol: a hardware-in-the-loop test surface for the refactor.
// Enabled with -DBENCH, used by [env:bench] in platformio.ini.
//
// Design notes:
//  - no String, no printf, no heap: flash is the scarce resource here
//  - deterministic time: "t<n>" queues n master ticks that the TCA0 ISR path
//    would normally produce, and stops the real timer while it does so
//  - every command is one line, terminated by '\n'

#include <Arduino.h>

#include "bench.h"

#ifdef BENCH

#include <clock.h>
#include <gateOut.h>
#include <keyboard.h>
#include <interface.h>
#include <storage.h>
#include <timer.h>
#include <track.h>

namespace {

constexpr uint8_t kLineMax = 24;

char     s_line[kLineMax];
uint8_t  s_len = 0;
uint16_t s_pendingTicks = 0;
bool     s_timerStopped = false;
bool     s_hold = false;

// ── tiny parsers ─────────────────────────────────────────────────────
uint16_t parseDecimal(const char*& p) {
    uint16_t value = 0;
    while (*p >= '0' && *p <= '9') {
        value = (uint16_t)(value * 10 + (*p - '0'));
        p++;
    }
    return value;
}

uint32_t parseHex(const char*& p) {
    uint32_t value = 0;
    for (;;) {
        const char c = *p;
        uint8_t digit;
        if (c >= '0' && c <= '9')      { digit = (uint8_t)(c - '0'); }
        else if (c >= 'a' && c <= 'f') { digit = (uint8_t)(c - 'a' + 10); }
        else if (c >= 'A' && c <= 'F') { digit = (uint8_t)(c - 'A' + 10); }
        else { break; }
        value = (value << 4) | digit;
        p++;
    }
    return value;
}

void printHexByte(uint8_t value) {
    const char* digits = "0123456789ABCDEF";
    Serial.write(digits[value >> 4]);
    Serial.write(digits[value & 0x0F]);
}

// ── commands ─────────────────────────────────────────────────────────
void cmdStatus() {
    Serial.print(F("S "));
    Serial.print(cuisine::clock::started() ? 1 : 0);
    Serial.print(' ');
    Serial.print(cuisine::clock::ticks());
    Serial.print(' ');
    Serial.print(cuisine::clock::tempoTenths());
    Serial.print(' ');
    Serial.print(trackClass::getIndex(0).step);
    Serial.print(' ');
    Serial.print(cuisine::gate_out::mask());
    Serial.println();
}

void cmdTicks(const char* p) {
    s_pendingTicks = parseDecimal(p);
    if (s_pendingTicks && !s_timerStopped) {
        timerClass::stop();
        s_timerStopped = true;
    }
}

void cmdFramDump(const char* p) {
    const uint32_t address = parseHex(p);
    if (*p == ',') { p++; }
    uint16_t length = parseDecimal(p);
    if (length > 64) { length = 64; }

    uint8_t buffer[16];
    while (length) {
        const uint8_t chunk = (length < 16) ? (uint8_t)length : 16;
        cuisine::storage::fram.readBlock(address, buffer, chunk);
        for (uint8_t i = 0; i < chunk; i++) {
            printHexByte(buffer[i]);
            Serial.write(' ');
        }
        Serial.println();
        length = (uint16_t)(length - chunk);
    }
    Serial.println(F("."));
}

// FRAM scratch area: past the transpose table (26075). The top of FRAM is
// unused, so 0x7000 is safe.
constexpr uint16_t kFramScratch = 0x7000;

void cmdFramTest() {
    Serial.print(F("R st="));
    Serial.println(cuisine::storage::fram.status());

    const uint8_t pattern[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    uint8_t back[4] = { 0, 0, 0, 0 };

    cuisine::storage::fram.writeBytes(kFramScratch, pattern, sizeof(pattern));
    cuisine::storage::fram.readBytes(kFramScratch, back, sizeof(back));

    bool ok = true;
    for (uint8_t i = 0; i < 4; i++) {
        if (back[i] != pattern[i]) { ok = false; }
    }

    Serial.print(ok ? F("R PASS ") : F("R FAIL "));
    for (uint8_t i = 0; i < 4; i++) {
        printHexByte(back[i]);
        Serial.write(' ');
    }
    Serial.println();
}

void cmdFramUnprotect() {
    Serial.print(F("C st="));
    Serial.print(cuisine::storage::fram.status());
    cuisine::storage::fram.writeStatus(0x00);
    Serial.print(F(" -> "));
    Serial.println(cuisine::storage::fram.status());
}

void cmdDivisionTable() {
    Serial.print(F("D"));
    for (uint8_t d = 0; d < 16; d++) {
        Serial.write(' ');
        Serial.print(d);
        Serial.write(':');
        Serial.print(cuisine::clock::ticksPerStep(d));
    }
    Serial.println();
}

void execute(char* line) {
    if (line[0] == 0) { return; }

    const char* p = line + 1;
    switch (line[0]) {
        case '?':
            Serial.println(F("?s t p o r h u d R C K D"));
            break;
        case 's':
            cmdStatus();
            break;
        case 't':
            cmdTicks(p);
            Serial.print(F("T "));
            Serial.println(s_pendingTicks);
            break;
        case 'p':
            cuisine::clock::start();
            Serial.println(F("p"));
            break;
        case 'o':
            cuisine::clock::stop();
            Serial.println(F("o"));
            break;
        case 'r':
            cuisine::clock::stop();
            cuisine::clock::start();
            Serial.println(F("r"));
            break;
        case 'h':
            timerClass::stop();
            s_hold = true;
            Serial.println(F("h"));
            break;
        case 'u':
            s_hold = false;
            timerClass::start();
            Serial.println(F("u"));
            break;
        case 'd':
            cmdFramDump(p);
            break;
        case 'K':
            interfaceClass::benchToggleStep();
            break;
        case 'R':
            cmdFramTest();
            break;
        case 'C':
            cmdFramUnprotect();
            break;
        case 'D':
            cmdDivisionTable();
            break;
        default:
            Serial.println(F("?"));
            break;
    }
}

} // namespace

void bench::init() {
    Serial.begin(115200);
    Serial.println(F("cuisine bench"));
}

void bench::poll() {
    // Deterministic ticks: one per loop, so the timer stays out of the way.
    if (s_pendingTicks) {
        timerClass::setTick();
        s_pendingTicks--;
        if (s_pendingTicks == 0 && s_timerStopped) {
            s_timerStopped = false;
            if (!s_hold) { timerClass::start(); }
        }
    }

    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c == '\r') { continue; }
        if (c == '\n') {
            s_line[s_len] = 0;
            s_len = 0;
            execute(s_line);
            continue;
        }
        if (s_len < kLineMax - 1) { s_line[s_len++] = c; }
    }
}

#else

void bench::init() {}
void bench::poll() {}

#endif
