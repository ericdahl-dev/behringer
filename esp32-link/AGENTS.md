# esp32-link — Agent Guide

## What this firmware is

**Ableton Link → XR18 FX tempo sync.** This ESP32 firmware joins an Ableton
Link session over WiFi as a native peer and writes the session BPM to a delay
FX slot on a Behringer XR18 mixer via OSC UDP.

## What this firmware is NOT

**This is not the X32 emulator.** The X32 emulator lives in `esp32/` and is
tracked with `ESP-` prefixed tasks. That firmware emulates an X32 mixer. This
firmware is a Link ↔ OSC bridge — a completely different role running as
separate firmware on the same physical board (XIAO ESP32-S3).

Do not edit files in `esp32/X32_emulator/` when working on this project.
Do not edit files in `esp32-link/X32Link/` when working on the X32 emulator.

## Firmware scope

| Item              | Value                                      |
|-------------------|--------------------------------------------|
| Sketch dir        | `esp32-link/X32Link/`                      |
| Board             | Any ESP32-S3 (XIAO ESP32-S3, Super Mini, etc.) |
| FQBN              | `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default` |
| Upload port       | `/dev/ttyACM0` at 921600 baud              |
| Link SDK          | `esp32-link/lib/link/` (git submodule)     |
| Target mixer      | Behringer XR18 (OSC port 10024)            |
| OSC write target  | `/fx/{slot}/par/02 ,f {normalized}`        |
| Task prefix       | `LNK-`                                     |

## Architecture

```
[DAW / iOS / Link peers]
      ↕  Ableton Link (multicast UDP 224.76.78.75:20808)
[XIAO ESP32-S3]  — native Link peer, no intermediary
      ↕  OSC UDP :10024
[XR18]  →  /fx/{slot}/par/02 ,f {val}
```

## Key files (once built)

```
esp32-link/
  lib/link/            Ableton Link SDK (git submodule)
  X32Link/
    X32Link.ino        Arduino main sketch
    config.h           WiFi creds, XR18 IP, FX slot, thresholds
    link_bridge.h      C interface to Link SDK (abl_link wrapper)
    link_bridge.cpp    C++ impl — the only .cpp file in the sketch
```

## BPM → OSC math

```c
// Delay time normalized to [0.0, 1.0] where 1.0 = 3000 ms max
float bpm_to_normalized(float bpm) {
    float ms = 60000.0f / bpm;
    if (ms > 3000.0f) ms = 3000.0f;
    return ms / 3000.0f;
}
```

This is the same formula as `src/x32tap_logic.c:ms_to_normalized()` —
inlined here because the sketch cannot directly share the `src/` C files.

## Ordna tasks

Tasks for this firmware are in `esp32-link/tasks/` with `LNK-` IDs.

See the root `AGENTS.md` for the full Ordna task format and CLI reference.
