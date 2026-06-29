#ifndef TOAST_LOGIC_H
#define TOAST_LOGIC_H

#include <stdint.h>

/* RTA bin index for each GEQ par (index 0 = par 01).
 * Calibrated: pistonphone 1 kHz @ 94 dB SPL → bin 56. */
extern const int TOAST_GEQ_BIN[31];

/* Map RTA bin index (0–99) to GEQ par number (1–31). */
int bin_to_geq_par(int bin);

/* Parse a /meters/4 OSC blob into bins[100] in dBFS.
 * blob[0-3]: big-endian total byte count.
 * blob[4-7]: little-endian sample count (must equal 100).
 * blob[8+]:  100 × little-endian int16 samples.
 * Returns 0 on success, -1 if the packet is too short or n_vals != 100. */
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
