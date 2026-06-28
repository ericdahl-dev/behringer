# esp32 — Agent Guide

## What this firmware is

**Tempo → XR18/X32 FX delay sync.** One ESP32 firmware that reads a musical
tempo from one of two sources and writes the matching delay time to an FX slot
on a Behringer XR18 / X32 mixer over OSC UDP. The tempo source is chosen at
runtime from the web UI:

- **Ableton Link** — joins a Link session over WiFi as a native peer (multicast
  UDP `224.76.78.75:20808`), reading session BPM from the gossip timeline.
- **USB MIDI clock** — acts as a USB-MIDI device and derives BPM from 24-PPQN
  clock pulses from a DAW.

Also contains the **X32 emulator** (`X32_emulator/`) used for on-device testing.

## What this firmware is NOT

Not the CLI tools (`cli/`, root `T-` tasks). This is the on-board tempo→OSC
bridge and emulator only. Don't edit `cli/` when working here.

## Build / flash / test

| Action | Command |
|---|---|
| Compile | `arduino-cli compile --fqbn esp32:esp32:adafruit_qtpy_esp32s3_n4r2 X32Link` |
| Flash   | `arduino-cli upload  --fqbn esp32:esp32:adafruit_qtpy_esp32s3_n4r2 -p /dev/ttyACM0 X32Link` |
| Host tests | `cd test && make`  (Unity; pure-logic suites, runs on the dev box) |

Notes:
- **Board:** ESP32-S3 Super Mini (and XIAO). We compile against the
  `adafruit_qtpy_esp32s3_n4r2` FQBN — it's a generic S3 profile that boots
  these boards with USB-CDC serial. The beat **LED is a plain LED on GPIO48,
  active-HIGH** on the Super Mini (`X32Link.ino`: `LED_PIN_NUM`; define
  `LED_ACTIVE_LOW` for XIAO, `LED_RGB` for a WS2812 board).
- Native-USB boards re-enumerate `/dev/ttyACM0` on reset; serial capture is
  flaky right after flashing. The web UI `/status` endpoint is the reliable
  way to read live BPM.

## Mixer output

- OSC target: `/fx/{slot}/par/01 ,f {normalized}` (delay-time parameter).
- Port: **XR18 → 10024**, **X32 → 10023** (`app_config.c:config_model_port`).
- Normalization lives in `osc_out.c:bpm_to_normalized()` — delay ms = 60000/BPM,
  clamped to 3000 ms, scaled to 0.0–1.0.

## Modules (roles — the directory is the source of truth for the file list)

| Module | Job |
|---|---|
| `X32Link.ino` | app core: setup/loop, WiFi + AP fallback, FreeRTOS bpm/led tasks, factory reset |
| `tempo_source.{h,cpp}` | the input **seam** — one interface, dispatches to Link or MIDI by `input_source` |
| `link_listener.*` + `link_protocol.*` | Link adapter; `link_protocol.c` is our own ~100-line gossip parser (the vendored `lib/link/` SDK is **not** used at runtime) |
| `midi_clock.*` · `midi_bpm.*` · `midi_bpm_calc.*` | USB-MIDI adapter; `midi_bpm_calc` is the pure, host-tested BPM math (symlinked from `X32MidiClock/`) |
| `bpm_tracker.*` | change/threshold detection |
| `osc_out.*` / `osc_sender.*` | OSC packet build / UDP send to the mixer |
| `app_config.*` · `app_config_nvs.cpp` | config struct, validation, NVS persistence (incl. `input_source`) |
| `web_config.*` | rack-panel config web UI + captive portal + `/status` live-BPM endpoint |
| `config.h` | per-firmware constants (Link/MIDI timing, first-boot defaults) |
| `X32_emulator/` | X32 on-device emulator for integration tests |

Shared C files are real in `X32Link/` and **symlinked** into `X32MidiClock/`
(Arduino sketches must be flat dirs); the `midi_*` files are the reverse.

## Key facts that must stay exact (cross-check against code if editing)

- WiFi setup must call `WiFi.setSleep(false)` — modem power-save drops the
  buffered multicast and Link silently never receives. (`X32Link.ino`)
- USB MIDI must enumerate **before** WiFi; Link joins multicast **after**.
  Captured by `tempo_source_pre_net()` / `tempo_source_begin()`.

## Ordna tasks

`esp32/tasks/`, prefixes: `LNK-` (Link firmware), `MCK-` (standalone MIDI clock),
`TSV-` (ToastSaver hardware), `ESP-` (emulator), `FDR-` (X32FaderDisp — fader dB
on scribble strips, X32-only), `MUT-` (X32SafeMutes — locked-mute guard),
`ITT-` (MidiOscIttt — MIDI↔OSC rules bridge). The `FDR-`/`MUT-`/`ITT-` control
firmwares depend on `LNK-013` (shared `osc_in` receive/subscribe module — the
client-side `/xremote` path X32Link does not yet have). OSC node references for
them live in `docs/xr18-xair-osc-cheatsheet.md` and `docs/x32-osc-protocol.md`.
See root `AGENTS.md` for the Ordna format/CLI.
