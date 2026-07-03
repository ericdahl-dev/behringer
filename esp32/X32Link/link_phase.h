// Pure bar-quantized phase math for the Link adapter (LNK-019).
//
// Glues LNK-017's parsed LinkTimeline and LNK-018's GhostXForm together:
// the Arduino-side caller (tempo_source.cpp) maps its local esp_timer clock
// into ghost time via link_ghost_xform_host_to_ghost() and passes the
// result in here. Everything in this file is pure arithmetic — no
// WiFi/Arduino dependency — and is host-tested in test/test_link_phase.c.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "link_protocol.h"  // LinkTimeline

#ifdef __cplusplus
extern "C" {
#endif

// Beats elapsed at ghost_now_us, per Timeline.toBeats() (Link SDK
// terminology): beat_origin_micro is the timeline's beat origin expressed
// in fixed-point micro-beats (beats * 1e6), time_origin_us/micros_per_beat
// anchor the linear beats-vs-ghost-time relationship.
double link_phase_beats_now(LinkTimeline timeline, int64_t ghost_now_us);

// Phase within [0, quantum) given beats_now. fmod() can return a negative
// result when beats_now < 0 (e.g. a ghost_now_us before the timeline's
// origin) — shifted back into [0, quantum) here.
double link_phase_from_beats(double beats_now, double quantum);

// LNK-026: a Link host restarting transport can re-origin the session — it
// republishes the timeline with a fresh ghost-time epoch, so time_origin_us
// jumps backward far beyond gossip jitter or UDP reordering. Detect that so the
// caller can invalidate a committed GhostXForm bound to the OLD epoch; without
// this, phase is computed from the new origin + the stale clock offset and reads
// as garbage until the ~2s periodic re-measurement catches up. Returns true only
// for a large backward step (not a small out-of-order blip).
bool link_phase_timeline_epoch_reset(int64_t prev_time_origin_us, int64_t new_time_origin_us);

#ifdef __cplusplus
}
#endif
