// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#include <Arduino.h>

#include <bench.h>
#include <cuisine.h>
#include <interface.h>

interfaceClass interface;

void setup() {
  if(BOD.INTFLAGS & BOD_VLMIF_bm ) {
    BOD.INTFLAGS = 0;
  }

  cuisine::begin();
  interface.init();
  bench::init();
}

void loop() {
  interface.update();
}
