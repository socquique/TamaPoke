# Changelog

All notable changes to TamaPoke are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Version
numbers match `FW_VERSION` in `TamaPoke.ino`, shown on the settings screen and printed
over serial at boot.

## [Unreleased]

### Changed

- Reorganized the sketch's own headers and sources into `include/` and `src/`, following
  Arduino's `src` sketch convention (compiled recursively, not shown as IDE tabs).
  `pin_config.h`, `dex.h`, `species.h`, `pet.h`, `audio.h`, `rtcbat.h`, `sdmon.h` and
  `i18n.h` are now `include/*.hpp`; their `.cpp` counterparts moved to `src/*.cpp`.
  `TamaPoke.ino` stays at the sketch root, as required by the Arduino build.
- Updated `tools/gen_dex.py` and `tools/sprites.py` to write their generated headers to
  `include/dex.hpp` and `include/species.hpp`.

## [1.4] - 2026-08-07

### Fixed

- Bond was being lost about 12x faster than it could be gained from care, cooling much
  more than a day of good care could offset. Rebalanced the care-mistake bond penalty.

## [1.3] - 2026-08-07

### Fixed

- The egg could hatch on its own while the player was still choosing a starter on first
  run, skipping the starter choice.

## Earlier versions

Firmware releases before 1.3 (including the initial 1.0–1.2 line) predate this
changelog; see the git history for details.
