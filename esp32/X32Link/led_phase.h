// Pure phase-wrap-detection decision for the beat LED (LNK-021).
//
// Source-agnostic: led_task() in X32Link.ino polls tempo_source_phase(1.0f)
// every 5ms and needs to know whether that poll just crossed a beat
// boundary (phase wrapped from near-1 back to near-0) — this is that
// decision, pulled out so it's host-testable without dragging in
// FreeRTOS/Arduino. No relation to link_phase.c, which is Link-adapter-only
// beats/timeline math; this file knows nothing about Link or MIDI.
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Should the LED flash on this poll? `prev_phase` is the phase reading
// from the previous poll (-1.0f means "no prior reading since (re)sync" —
// the caller must seed it that way on the invalid->valid transition so the
// first valid reading never falsely flashes). `phase` is this poll's
// reading. `valid` mirrors tempo_source_phase_valid() — when false the
// answer is always false (the caller should be running the fallback blink
// instead, not calling this at all, but the gate is here too so the
// decision is fully described by its inputs).
bool led_phase_should_flash(float prev_phase, float phase, bool valid);

#ifdef __cplusplus
}
#endif
