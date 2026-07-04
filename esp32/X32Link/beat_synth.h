#pragma once
// Pure free-running beat generator (LNK-033). Given a wall-clock millisecond
// reading and a BPM, fire true once per 60000/bpm interval — the "synthesised
// beat" the LED/UI fall back to before a real Link phase measurement is valid.
// Previously inlined in tempo_source_beat() against millis(); factored out so the
// interval + edge-detect + self-clear is host-tested (ADR-0003). No Arduino
// dependency. See test/test_beat_synth.c.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t last_ms;   // wall-clock ms of the last emitted beat
} BeatSynth;

// Clear state (last_ms = 0). Behaviour then matches a fresh static counter:
// the first step() with now_ms past one interval fires immediately.
void beat_synth_reset(BeatSynth* s);

// Advance to now_ms and return true if a beat is due at `bpm`. Fires when
// (now_ms - last_ms) has reached the 60000/bpm interval (unsigned/wrap-safe),
// then latches last_ms = now_ms. bpm <= 0 always returns false and leaves state
// untouched (so a resumed BPM fires promptly, matching the old inline behaviour).
bool beat_synth_step(BeatSynth* s, uint32_t now_ms, float bpm);

#ifdef __cplusplus
}
#endif
