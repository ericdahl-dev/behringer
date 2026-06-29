#include "toast_logic.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

const int TOAST_GEQ_BIN[31] = {
     0,  3,  7, 10, 13, 16, 20, 23, 26, 30,
    33, 36, 39, 43, 46, 49, 53, 56, 59, 63,
    66, 69, 72, 76, 79, 82, 86, 89, 92, 96,
    99
};

int bin_to_geq_par(int bin) {
    int best = 0, best_dist = 999;
    for (int i = 0; i < 31; i++) {
        int d = abs(TOAST_GEQ_BIN[i] - bin);
        if (d < best_dist) { best_dist = d; best = i; }
    }
    return best + 1;
}

int parse_meters4_blob(const uint8_t *blob, int blen, float *bins) {
    if (blen < 8) return -1;
    uint32_t n_vals = (uint32_t)blob[4]        | ((uint32_t)blob[5] << 8) |
                      ((uint32_t)blob[6] << 16) | ((uint32_t)blob[7] << 24);
    if (n_vals != 100) return -1;
    if (blen < 8 + (int)(n_vals * 2)) return -1;
    for (int i = 0; i < 100; i++) {
        int16_t raw;
        memcpy(&raw, blob + 8 + i * 2, 2);
        bins[i] = (float)raw / 256.0f;
    }
    return 0;
}

float db_to_geq_float(float dB) {
    return (dB + 15.0f) / 30.0f;
}

int detect_peak(const float *bins, const float *baseline, float threshold, int *peak_bin) {
    int   best     = -1;
    float best_val = -999.0f;
    for (int i = 0; i < 100; i++) {
        float excess = bins[i] - baseline[i];
        if (excess > threshold && bins[i] > best_val) {
            best_val = bins[i];
            best     = i;
        }
    }
    if (best < 0) return 0;
    *peak_bin = best;
    return 1;
}

void update_baseline(float *baseline, const float *bins, int n, float alpha) {
    for (int i = 0; i < n; i++)
        baseline[i] = (1.0f - alpha) * baseline[i] + alpha * bins[i];
}

int is_narrow_peak(const float *bins, int n_bins, int peak_bin,
                   int skip, int span, float min_db) {
    float sum = 0.0f;
    int   lcnt = 0, rcnt = 0;
    for (int i = peak_bin - skip - span; i <= peak_bin - skip - 1; i++)
        if (i >= 0) { sum += bins[i]; lcnt++; }
    for (int i = peak_bin + skip + 1; i <= peak_bin + skip + span; i++)
        if (i < n_bins) { sum += bins[i]; rcnt++; }
    if (lcnt < 1 || rcnt < 1) return 0;  /* need neighbors on both sides */
    return (bins[peak_bin] - sum / (float)(lcnt + rcnt)) >= min_db;
}

float fader_db_to_float(float dB) {
    if (dB >= 10.0f)  return 1.0f;
    if (dB >= -10.0f) return (dB + 30.0f) / 40.0f;
    if (dB >= -30.0f) return (dB + 50.0f) / 80.0f;
    if (dB >= -60.0f) return (dB + 70.0f) / 160.0f;
    if (dB >= -90.0f) return (dB + 90.0f) / 480.0f;
    return 0.0f;
}

float fader_float_to_db(float f) {
    if (f >= 1.0f)    return 10.0f;
    if (f >= 0.5f)    return 40.0f  * f - 30.0f;
    if (f >= 0.25f)   return 80.0f  * f - 50.0f;
    if (f >= 0.0625f) return 160.0f * f - 70.0f;
    if (f >= 0.0f)    return 480.0f * f - 90.0f;
    return -90.0f;
}
