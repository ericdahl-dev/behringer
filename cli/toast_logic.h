#ifndef TOAST_LOGIC_H
#define TOAST_LOGIC_H

#include <stdint.h>

/* ── toast_step: pure reactive detection loop ───────────────────────────── */

typedef enum {
    TOAST_HOLD,         /* nothing to do this frame */
    TOAST_NOTCH,        /* place a new notch; send cut_val to geq_par */
    TOAST_RELEASE_RAMP, /* ramp an active notch toward flat; send ramp_val */
    TOAST_RELEASE_DONE  /* notch fully released; restore to flat (0.5) */
} ToastOp;

typedef struct {
    ToastOp op;
    int     geq_par;  /* 1-31; valid for NOTCH, RELEASE_RAMP, RELEASE_DONE */
    float   cut_val;  /* TEQ float applied on TOAST_NOTCH */
    float   ramp_val; /* TEQ float to send on TOAST_RELEASE_RAMP/DONE */
} ToastAction;

typedef struct {
    /* per-band mutable state */
    int   active[31];
    float cut_val[31];
    int   release_hold[31];
    int   from_profile[31]; /* pinned — never auto-released */
    int   confirm_hold[31];
    /* config — set at init, read-only in step */
    float threshold_dB;
    float cut_dB;
    int   release_frames;   /* release_sec * 20 */
    int   confirm_frames;
    float narrow_db;
    int   narrow_skip;
    int   narrow_span;
} ToastState;

void toast_state_init(ToastState *st,
                      float threshold_dB, float cut_dB,
                      float release_sec,  int   confirm_frames,
                      float narrow_db,    int   narrow_skip,
                      int   narrow_span);

/* Process one RTA frame. Writes up to cap actions into out; returns count.
 * One frame can produce up to 31 RELEASE_RAMP/DONE + at most 1 NOTCH.
 * Caller must size out to at least 32. */
int toast_step(ToastState *st,
               const float *bins,
               const float *baseline,
               ToastAction *out, int cap);

/* RTA bin index for each GEQ par (index 0 = par 01).
 * Calibrated: pistonphone 1 kHz @ 94 dB SPL → bin 56. */
extern const int TOAST_GEQ_BIN[31];

/* Map RTA bin index (0–99) to GEQ par number (1–31). */
int bin_to_geq_par(int bin);

/* Parse a meters OSC blob into bins[100] in dBFS.
 * blob[0-3]: big-endian total byte count.
 * blob[4-7]: little-endian n_vals (100 for XR18/meters/4; 50 for X32/meters/15).
 * blob[8+]:  100 × little-endian int16 samples (200 bytes in both cases).
 * Returns 0 on success, -1 if packet is too short or n_vals is not 50 or 100. */
int parse_meters4_blob(const uint8_t *blob, int blen, float *bins);

/* Encode dB to GEQ float [0.0, 1.0].  0 dB → 0.5,  −15 dB → 0.0,  +15 dB → 1.0. */
float db_to_geq_float(float dB);

/* Find the highest-amplitude bin that strictly exceeds baseline[i] + threshold.
 * Sets *peak_bin to that index and returns 1 if found, 0 otherwise. */
int detect_peak(const float *bins, const float *baseline, float threshold, int *peak_bin);

/* Exponential smoothing: baseline[i] = (1-alpha)*baseline[i] + alpha*bins[i], for i in [0,n). */
void update_baseline(float *baseline, const float *bins, int n, float alpha);

/* Returns 1 if bins[peak_bin] is at least min_db above the average of neighbor
 * bins sampled skip..skip+span positions away on each side (clamped to [0,n_bins)).
 * Returns 0 if fewer than 2 neighbor samples exist or the advantage is insufficient.
 * Typical call: is_narrow_peak(bins, 100, peak, 2, 3, 10.0f)
 *   → compares peak to bins at ±3, ±4, ±5, skipping the two immediately adjacent. */
int is_narrow_peak(const float *bins, int n_bins, int peak_bin,
                   int skip, int span, float min_db);

/* XAir fader/level value encoding (piecewise, from the XAir OSC cheat-sheet).
 * Converts between a fader OSC float [0.0,1.0] and dB. Used by the ring-out
 * gain drive (T-022) to command /bus/N/mix/fader by dB.
 *   fader_db_to_float(+10) = 1.0, (0) = 0.75, (-10) = 0.5, (-90) = 0.0
 * Out-of-range dB clamps to [0.0, 1.0]. */
float fader_db_to_float(float dB);
float fader_float_to_db(float f);

#endif /* TOAST_LOGIC_H */
