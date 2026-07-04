#pragma once
// 24-PPQN MIDI clock OUT scheduler (LNK-027) — a thin adapter over the shared,
// pure clock_ticker engine (LNK-028 uses the same engine at a configurable
// PPQN). No Arduino/USB dependency — host-tested in test/test_midi_clock_out.c.
// The thin glue (midi_clock_out_io.cpp) owns the timer task and the TinyUSB
// writes. Ticks are phase-locked: the first tick lands on phase-zero and the
// clock stays bar-aligned, not just tempo-matched.
#include "clock_ticker.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_CLOCK_OUT_PPQN 24

// MIDI clock out is a ClockTicker fixed at 24 PPQN.
typedef ClockTicker MidiClockOut;

// Clear state: the next ticks_due() call re-primes the grid (emits nothing).
void midi_clock_out_reset(MidiClockOut* s);

// Advance the scheduler to the current monotonic beat position and return how
// many 0xF8 ticks to emit now (24 PPQN). Same semantics as
// clock_ticker_ticks_due() with ppqn=24 — prime-on-first, no backward/backlog,
// re-prime (return 0) on a forward jump larger than max_burst.
int midi_clock_out_ticks_due(MidiClockOut* s, double beats_now, int max_burst);

#ifdef __cplusplus
}
#endif
