# Changelog

Notable changes to the cuisine SDK. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**1.0.0 is the first stable release.** The API described in
[`docs/api.md`](docs/api.md) is frozen for the 1.x series: a breaking change to
`#include <cuisine.h>` or to any documented call requires a 2.0.0. Additions
are backwards compatible and ship in minor versions.

## [Unreleased]

Nothing yet.

## [1.0.0] — 2026-10-29

First stable release. Hardware layer only: the SDK knows the panel, the
outputs, the clock and the memories, and nothing about patterns or tracks.

### Added

- **Eleven hardware blocks**, each with a namespaced facade:
  `clock`, `keys`, `pot`, `gate_in`, `gate_out`, `cv`, `led`, `display`,
  `midi`, `storage`, `board`.
- **`cuisine::begin()` and `cuisine::poll()`** — one call to bring the hardware
  up, one to advance it. `poll()` latches key edges, drains MIDI, advances the
  master timebase and flushes gates and CV; it never blocks.
- **`clock::onHardwareTick()`** — the timer interrupt belongs to the SDK. An
  application that does not define the vector can no longer fall into the
  default handler at the first tick.
- **`clock::ticksPerStep()`** — the division table as a pure function, with the
  encoding documented and the unreachable dotted branch removed.
- **`storage::fram` and `storage::flash`** — own drivers for the MB85RS256 and
  the ZD25Q80C at 10 MHz, no third-party code. FRAM writes issue `WREN` before
  every operation.
- **`board::`** — every pin in one namespace, plus `CommandKey` names and
  `kStepKeyCount`.
- Reference application: a complete sequencer in `src/`, about 47 KB of the
  48,640 B usable, covering tracks, patterns, banks, scales, arpeggiator and
  Euclidean rhythms.

### Licence

GNU General Public License v3.0 or later. See [../../LICENSE](../../LICENSE).
