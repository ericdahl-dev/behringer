# LNK-026 Handoff — Link phase misbehavior on the touch board

**Date:** 2026-07-03
**Branch:** `fix/lnk-026-phase-restart` (off `master`)
**Status:** one of two bugs fixed + verified; the other root-caused and reproducible, not yet fixed.
**Board:** Waveshare ESP32-S3-Touch-LCD-1.47 @ `192.168.0.74`, flashed with the fix + a temporary `/phasedbg` diagnostic.

---

## TL;DR

What the user reported as one bug ("phase wheel dot moves erratically / ends up 180° off when the
Link host stops") turned out to be **two independent bugs**, isolated with live `/phasedbg`
instrumentation against Ableton Note (phone) + Ableton Live (laptop) as Link peers:

1. **Erratic garbage during re-sync (transport stop/start)** — **FIXED & VERIFIED.**
2. **Static ~2-beat phase offset** — **root-caused as a device bug, NOT yet fixed.** This is the
   remaining work.

---

## Bug 1 — erratic garbage on transport stop/start  ✅ FIXED

### Root cause (confirmed on hardware)
When the Link host restarts transport, Ableton **re-origins the session**: it republishes the
timeline with a fresh ghost-time epoch, so `beat_origin`/`time_origin` jump (observed: `borig`
7431517234→~0, `torig` 510898878→~416793). `link_protocol.c` updates those instantly from gossip,
but the committed measurement `intercept_us` (host→ghost clock offset) only refreshes on the next
periodic re-measurement (up to ~2s later). During that window phase is computed from a **new
timeline origin + stale clock offset = garbage** (observed `beats` spiking to ~1046), and
`tempo_source_phase_valid()` never dropped — so the wheel *rendered* the garbage. That is the
"moves randomly / erratically" symptom.

### The fix
- **New pure, host-tested helper** `link_phase_timeline_epoch_reset(prev_torig, new_torig)`
  (`link_phase.c/.h`): true when `time_origin` jumps backward > 1s (a real re-origin; small
  backward steps = UDP reordering, ignored). Tests in `test/test_link_phase.c`
  (`test_epoch_reset_*`), all green (11 tests in that suite).
- **Wired into the measurement pump** `link_measurement_io.cpp::check_epoch_reset()` (called at
  the top of `link_measurement_io_poll()`): on a detected reset it calls
  `link_measurement_reset()` (invalidates the committed xform → `phase_valid` goes false → UI shows
  "syncing"), forces an immediate re-measure (`s_have_ref=false`, `s_next_measure_us=0`), so it
  re-locks cleanly against the new epoch instead of rendering garbage.

### Verification (on hardware)
- Steady play: `time_origin` strictly monotonic forward, `xvalid` never drops → **no false
  triggers / no flicker regression**.
- Stop/start: the temporary `resets` counter on `/phasedbg` incremented (3→10 across several
  stop/starts), `xvalid` dropped to `false` ("syncing") then recovered to smooth sweeping — the
  `beats≈1046` garbage spikes are gone.

**This part is good to land.**

---

## Bug 2 — static ~2-beat phase offset  ⚠️ OPEN (the real remaining work)

### Symptom
With **Ableton Live + Note both in the session and agreeing with each other**, the **device wheel
is exactly 2 beats behind**: the musical "1" crosses the device wheel at **6:00** in quantum 4
(phase 2 of 4) and at **9:00** in quantum 8 (phase 6 of 8). Two quanta, same absolute **2-beat**
offset — so it's an **integer-beat** error, not a fuzzy clock-offset.

### Why it's a device bug (not Ableton)
Ableton **Live is a reference-grade Link peer** (it aligns its bar-1 to Link phase-0). Live and
Note agree; the device disagrees with both by 2 beats. If the device disagrees with reference-truth
Live, the device is wrong.

### Why it's NOT the same as Bug 1
`/phasedbg` showed **`resets=0`** while the device was 2 beats off — i.e. **no re-origin had
happened since the last boot**, yet the offset was present. So this offset exists **from fresh
measurement acquisition**, independent of any stop/start. Bug 1's re-origin handling does not
address it. (Changing quantum reboots the device via "Write & Reboot", which is why `resets` was 0.)

Note: in the very first hardware pass (step 1 of LNK-020) the wheel "seemed on" by eye — a sweeping
dot without a precise reference is forgiving. Live is not. The offset may have been present then too,
or it may be **intermittent per-acquisition** (sometimes the committed measurement lands right,
sometimes 2 beats off). **Determining static-vs-intermittent is the first task on resume.**

### Hypotheses to chase (integer-beat offset)
The offset is an exact integer number of beats and enters at/after measurement acquisition, so look
on the **beat-count / beat-origin** side, not the clock-offset magnitude:
1. **`beat_origin_micro` interpretation** — `link_phase_beats_now()` uses
   `beat_origin_micro/1e6 + (ghost - time_origin_us)/micros_per_beat`. Confirm Ableton's gossiped
   Timeline encoding order/units (tempo, beatOrigin microbeats, timeOrigin µs) matches
   `link_protocol.c:121-129`. A 2-beat (2e6 microbeat) constant would show here.
2. **Measurement ghost-time reference** — does the ping/pong `GHostTime` (LNK-018) share the exact
   same ghost-time zero as the gossiped `time_origin`? A fixed reference mismatch between the two
   ghost clocks would inject a constant beat offset. `intercept_us` was *stable* across the session
   (~429,595,xxx) — stable but possibly stably-wrong.
3. **Per-acquisition ambiguity** — if the committed `intercept_us` can settle ~an integer beat off
   depending on ping/pong timing, the offset would vary run-to-run (explains step-1-looked-ok).

### Next diagnostic (fastest path to the exact number)
`/phasedbg` already dumps `borig, torig, icept, ghost, beats, q, resets`. On resume:
- With Live playing, at a **known Live bar:beat** (e.g. pause Live exactly on bar 5 beat 1), read
  `/phasedbg` `beats` and compute `beats mod q` vs Live's known phase → measures the **exact**
  offset (expect ~+2.0 beats). Then trace whether the 2 beats live in `beat_origin_micro` (compare
  to Live's session beat) or in `ghost`/`icept` (the measurement).
- Consider logging the device's absolute `beats` alongside Live's transport position to a couple of
  marked instants; the delta pins it to beatOrigin vs measurement.

### Fallback / product option
If Link phase-0 legitimately can't be pinned to a given app's visual "1" in all cases, a **manual
"downbeat offset" (rotate wheel by N beats)** setting is a reasonable device-side feature — but only
after confirming it's not a plain device math bug (it currently looks like a real bug: device
disagrees with reference-truth Live).

---

## Repo / board state on handoff

### Git (`fix/lnk-026-phase-restart`)
Committed on the branch (WIP): the Bug-1 fix, the temporary diagnostic, and task-file updates.
**NOT committed / must stay local:** `esp32/X32Link/build_opt.h` (`-DBOARD_WAVESHARE_S3_TOUCH_LCD_147`,
per AGENTS.md keep empty at HEAD). Unrelated iOS/CLI working changes were left untouched.

### ⚠️ Temporary diagnostic to REMOVE before the final PR
These were added only to instrument the bug and **must be reverted** before landing:
- `X32Link/web_config.cpp`: `/phasedbg` handler + `handle_phasedbg` + the weak
  `phasedbg_json` hook + both `server.on("/phasedbg", ...)` registrations.
- `X32Link/X32Link.ino`: the `phasedbg_json()` function, the diagnostic `#include`s
  (`link_protocol.h`, `link_measurement.h`, `link_measurement_io.h`, `link_phase.h`, `esp_timer.h`),
  and the `resets` field.
- `X32Link/link_measurement_io.{h,cpp}`: the `s_epoch_resets` counter +
  `link_measurement_io_epoch_resets()` getter (the `check_epoch_reset()` LOGIC stays — that's the fix).

**Keep for the fix:** `link_phase.{c,h}` (`link_phase_timeline_epoch_reset` + tests),
`link_measurement_io.cpp::check_epoch_reset()` + its `s_have_last_origin`/`s_last_time_origin_us`
state and the `check_epoch_reset()` call in `_poll()`, and the `link_phase.h` include.

### Build / test
- Host suite green: `cd esp32/test && make` (test_link_phase now 11 tests incl. 3 new).
- Firmware compiles clean for `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi`
  at **86% flash / 24% RAM** (display build via `-DBOARD_WAVESHARE_S3_TOUCH_LCD_147`).
- Flash: `arduino-cli upload --fqbn '<above>' -p /dev/cu.usbmodem2101 X32Link`.

### Board / test rig
- Device `/status` and `/phasedbg` at `http://192.168.0.74/`. Serial is flaky on this native-USB
  board — use the web endpoints.
- Reproduce Bug 2 with **Ableton Live** (reference peer) + optionally Note in the same Link session;
  compare the device wheel top vs Live's bar-1.

## LNK-020 (was `doing`)
The phase-accuracy validation surfaced both bugs. Step 1 (Link accuracy) passed; steps 3/5/6/7 held
pending these fixes. Resume LNK-020 after Bug 2 is fixed and re-verified against Live.
