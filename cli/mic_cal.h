#ifndef MIC_CAL_H
#define MIC_CAL_H

#define MIC_CAL_MAX_PTS 512

typedef struct { float freq_hz; float gain_db; } MicCalPoint;

typedef struct {
    MicCalPoint pts[MIC_CAL_MAX_PTS];
    int   n;
    float sensitivity_dbfs;   /* valid only if has_sensitivity */
    int   has_sensitivity;
} MicCal;

/* Parse a REW-style `.cal` or factory `.txt` mic-calibration buffer (both are
 * `freq gain` rows; REW has a `Sensitivity <x> dBFS` line, factory a
 * `*<f>Hz <sens>` line). Returns 0 on success, -1 if fewer than 2 points. */
int mic_cal_parse(const char *text, int len, MicCal *out);

/* Correction (dB) at a frequency: linear interpolation in log10(freq), clamped
 * to the cal's frequency range. Returns 0.0 if the cal has fewer than 2 points. */
float mic_cal_at(const MicCal *cal, float freq_hz);

/* Per-bin correction aligned to an RTA: corr_db[i] = mic_cal_at(bin_freq[i]). */
void mic_cal_bin_corrections(const MicCal *cal, const float *bin_freq,
                             float *corr_db, int n_bins);

#endif /* MIC_CAL_H */
