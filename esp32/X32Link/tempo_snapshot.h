#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// One coherent reading of the live tempo (ARC-001). Published by the bpm task,
// read by the web /status endpoint, the on-device UI, and serial logging. The
// whole struct is published/read under a lock so a reader never observes a torn
// pair — e.g. the old three-loose-globals path could emit valid=true alongside
// a stale phase=-1. Here that combination is impossible by construction.
typedef struct {
    float bpm;      // last published BPM
    float phase;    // [0, quantum) when valid; < 0 (sentinel) when not
    bool  valid;    // phase is a real, trustworthy reading
    int   quantum;  // beats per bar the phase is measured against
} TempoSnapshot;

// Publish a coherent snapshot. One writer (the bpm task). Enforces the
// invariant that `valid` implies a non-negative phase: a would-be contradictory
// (valid && phase < 0) publish is stored as not-valid.
void tempo_snapshot_publish(float bpm, float phase, bool valid, int quantum);

// Read the current snapshot atomically into *out. Many readers. NULL-safe.
void tempo_snapshot_read(TempoSnapshot* out);

#ifdef __cplusplus
}
#endif
