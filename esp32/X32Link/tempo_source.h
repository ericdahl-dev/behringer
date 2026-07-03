#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Where the BPM comes from. Stored in NVS as input_source.
typedef enum { TEMPO_SRC_LINK = 0, TEMPO_SRC_MIDI = 1 } TempoSourceKind;

// Select the active input. Call once at boot before pre_net/begin.
void     tempo_source_select(int kind);

void     tempo_source_pre_net(void);   // pre-WiFi init (USB MIDI enumerates here)
void     tempo_source_begin(void);     // post-WiFi init (Link joins multicast here)
void     tempo_source_poll(void);      // service the input every bpm_task tick
float    tempo_source_bpm(void);       // current BPM; 0.0 = no live signal
bool     tempo_source_active(void);    // peers>0 / pulses within timeout

// Per-beat blink (LNK-008): MIDI forwards the real clock's beat flag; Link
// synthesizes one locally from BPM via a millis()-timed interval. Cheap and
// always available — doesn't require a completed Link measurement. This is
// coarser than tempo_source_phase() crossing zero: once
// tempo_source_phase_valid() is true, watching phase() cross zero is a
// *more accurate* beat-crossing event (Link: derived from the session's
// actual timeline, not re-synthesized; MIDI: pulse-exact, not just
// BPM-interval timed). The two are not redundant — beat() stays useful as
// a fallback before phase() has enough data to be valid. See LNK-019.
bool     tempo_source_beat(void);      // true once per beat, self-clears (LED)

// Phase within the current quantum (bar), range [0, quantum). Returns
// -1.0f if no valid phase reading is available yet (no completed
// GhostXForm for Link; less than one full bar of pulses for MIDI) — check
// tempo_source_phase_valid() first. See LNK-019.
float    tempo_source_phase(float quantum);

// True once tempo_source_phase() can return a real value.
bool     tempo_source_phase_valid(void);

// Monotonic absolute beat position of the active source, or -1.0 when phase
// isn't valid yet. Unlike tempo_source_phase() this does NOT wrap at the
// quantum — it grows continuously with the timeline, which is what a clock
// generator needs to quantize to a 1/24-beat tick grid (LNK-027). Link: the
// session's beats-now; MIDI: pulse_count / 24.
double   tempo_source_beats_now(void);

float    tempo_source_threshold(void); // per-input change threshold
uint32_t tempo_source_poll_ms(void);   // per-input poll interval

#ifdef __cplusplus
}
#endif
