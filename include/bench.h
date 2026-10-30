// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dorian Largen

#ifndef BENCH_H
#define BENCH_H

// Serial bench protocol, compiled only with -DBENCH (see [env:bench]).
// In the production build both functions are empty and cost a few bytes.

namespace bench {

void init();  // opens Serial at 115200
void poll();  // call once per loop; consumes queued lines and pending ticks

} // namespace bench

#endif
