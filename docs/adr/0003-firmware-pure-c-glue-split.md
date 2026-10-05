# 3. ESP32 firmware splits pure-C logic from thin Arduino glue

Date: 2026-07-03
Status: accepted

## Context

The ESP32-S3 firmware (`esp32/X32Link`, `esp32/X32MidiClock`) has to run real
hardware — WiFi/UDP, USB-MIDI, NVS, a LovyanGFX touch LCD — but the interesting
behavior is logic: Ableton Link gossip parsing, ping/pong clock-offset math, BPM
change/threshold detection, bar/phase math, config validation, touch hit-testing.
Arduino code can only be compiled for the board and is painful to test on the dev
box, so any logic trapped inside `.cpp` glue is effectively untested until it's
flashed. Several real bugs this cycle (torn tempo reads, Link phase never holding
valid, MIDI BPM waver) were logic bugs that a host test would have caught.

## Decision

Every non-trivial module is split in two:

- `foo.c` — **pure logic**, no Arduino headers. Compiled BOTH into the sketch and
  into a Unity `test_foo.c` suite under `esp32/test/` (run on the dev box via
  `make`). On-device-only concerns (e.g. portMUX) are `#if defined(ARDUINO) ||
  defined(ESP_PLATFORM)` guarded, no-op on host.
- `foo_io.cpp` / `foo.cpp` — **thin glue**: WiFiUDP, GPIO, NVS, LovyanGFX. As
  close to no branching logic as possible.

Examples: `link_measurement.c` (TLV build/parse + median/offset) vs
`link_measurement_io.cpp` (unicast WiFiUDP); `touch_ui.c` (hit-testing / keypad
buffer) vs `touch_display.cpp` (LovyanGFX); `app_config.c` vs `app_config_nvs.cpp`;
`tempo_snapshot.c` (atomic tempo hand-off) and `bar.c` (beats-per-bar) as pure
seams with their own suites.

Shared C files are **real in `X32Link/` and symlinked into `X32MidiClock/`**
(Arduino requires flat sketch dirs); the `midi_*` files are the reverse. Edit the
real file. `check_docs.sh` (in the test target) asserts every source named in
`esp32/AGENTS.md` exists, so the module map can't rot.

## Consequences

- New logic lands test-first: a `.c` + a wired-in `test_*.c` before it's called
  from a sketch. Behavior is reproducible on the dev box before a board is plugged
  in — which matters because native-USB-S3 serial is flaky after flashing (the web
  `/status` endpoint, not serial, is the reliable live readout).
- The symlink sharing keeps one source of truth but is invisible in a flat Arduino
  dir — a reader can mistake a symlink for a copy and edit the wrong end.
- A machine-readable version of this (plus STACK/PATTERNS/TRADEOFFS/PHILOSOPHY for
  the whole monorepo) is stored in the codebase-memory graph ADR. That copy lives
  in the graph DB and is rebuilt/wiped on re-index; this file is the durable,
  git-tracked source of record.
