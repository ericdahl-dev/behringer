#pragma once
// Pure 24-PPQN clock-tick scheduler for MIDI clock OUT (LNK-027), and the
// shared tick engine a future analog sync out (LNK-028) can reuse. No
// Arduino/USB dependency — host-tested in test/test_midi_clock_out.c. The thin
// glue (midi_clock_out_io.cpp) owns the timer task and the TinyUSB writes.
//
// The scheduler quantizes a *monotonic beat position* (as produced by the Link
// phase pipeline, LNK-017/019) to 24 ticks per beat and reports how many 0xF8
// ticks are now due since the last call. Because the grid is derived from the
// session timeline, ticks are phase-locked: the first tick lands on phase-zero
// and the clock stays bar-aligned, not just tempo-matched.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_CLOCK_OUT_PPQN 24

typedef struct {
    int64_t last_tick;  // last emitted 1/24-beat grid index
    bool    primed;     // false until the first beats_now aligns the grid
} MidiClockOut;

// Clear state: the next ticks_due() call re-primes the grid (emits nothing).
void midi_clock_out_reset(MidiClockOut* s);

// Advance the scheduler to the current monotonic beat position and return how
// many 0xF8 ticks to emit now.
//   - first call after a reset primes the grid to beats_now and returns 0 (no
//     backlog dump);
//   - a backward or same-slot move returns 0;
//   - a forward move of N 1/24-beat slots returns N, UNLESS N exceeds
//     max_burst (a tempo re-origin or a long stall), in which case it re-primes
//     and returns 0 — the caller keeps phase alignment rather than flooding the
//     bus with a catch-up burst.
// beats_now < 0 is treated as 0 (a "not valid yet" sentinel from the glue).
int midi_clock_out_ticks_due(MidiClockOut* s, double beats_now, int max_burst);

#ifdef __cplusplus
}
#endif
