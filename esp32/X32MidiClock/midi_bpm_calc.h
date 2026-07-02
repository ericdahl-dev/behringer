#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pure BPM-from-MIDI-clock math, no Arduino deps — host-testable.
// MIDI clock is 24 PPQN. A window of `window` timestamps spans (window-1)
// intervals; scale that up to a full beat. Returns 0.0f for a zero/invalid span.
float midi_bpm_calc(uint32_t span_us, int window);

// Phase within the current bar [0, quantum) from a raw, directly-observed
// 24-PPQN pulse count (LNK-019). Unlike Link's phase — which is estimated
// from a clock-sync offset and carries clock-sync jitter — MIDI clock
// pulses are locally counted exactly, so this reading is *more* precise,
// not less, despite the simpler math. Returns 0.0f for quantum < 1.
float midi_phase_calc(uint32_t pulse_count, int quantum);

// True once at least one full bar's worth of pulses (24 * quantum) has been
// observed since clock start — avoids a misleading phase reading from a
// partial bar at stream start. Returns false for quantum < 1.
bool midi_phase_valid(uint32_t pulse_count, int quantum);

#ifdef __cplusplus
}
#endif
