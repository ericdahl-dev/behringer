#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pure BPM-from-MIDI-clock math, no Arduino deps — host-testable.
// MIDI clock is 24 PPQN. A window of `window` timestamps spans (window-1)
// intervals; scale that up to a full beat. Returns 0.0f for a zero/invalid span.
float midi_bpm_calc(uint32_t span_us, int window);

#ifdef __cplusplus
}
#endif
