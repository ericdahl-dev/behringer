#include "mic_cal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   /* strncasecmp */

/* Copy one line (sans newline) into buf; return pointer past the newline. */
static const char *next_line(const char *p, const char *end, char *buf, int bufsz) {
    int i = 0;
    while (p < end && *p != '\n') {
        if (i < bufsz - 1) buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';
    return (p < end) ? p + 1 : p;
}

int mic_cal_parse(const char *text, int len, MicCal *out) {
    out->n = 0;
    out->has_sensitivity = 0;
    out->sensitivity_dbfs = 0.0f;

    const char *p = text, *end = text + len;
    char line[256];
    while (p < end) {
        p = next_line(p, end, line, sizeof line);
        for (char *t = line; *t; t++) if (*t == ',') *t = ' ';  /* CSV → space */

        char *s = line;
        while (*s == ' ' || *s == '\t' || *s == '\r') s++;
        if (*s == '\0' || *s == '#' || *s == ';') continue;

        /* REW: "Sensitivity <x> dBFS" */
        if (strncasecmp(s, "Sensitivity", 11) == 0) {
            float v;
            if (sscanf(s + 11, " %f", &v) == 1) { out->sensitivity_dbfs = v; out->has_sensitivity = 1; }
            continue;
        }
        /* Factory: "*<freq>Hz <sens>" — reference sensitivity, not a data point */
        if (*s == '*') {
            float f, v;
            if (sscanf(s + 1, " %f Hz %f", &f, &v) == 2 ||
                sscanf(s + 1, " %fHz %f", &f, &v) == 2) {
                out->sensitivity_dbfs = v; out->has_sensitivity = 1;
            }
            continue;
        }
        /* Data row: "<freq> <gain>" (extra columns ignored). */
        float f, g;
        if (sscanf(s, "%f %f", &f, &g) == 2 && f > 0.0f && out->n < MIC_CAL_MAX_PTS) {
            out->pts[out->n].freq_hz = f;
            out->pts[out->n].gain_db = g;
            out->n++;
        }
    }
    return out->n >= 2 ? 0 : -1;
}

float mic_cal_at(const MicCal *cal, float freq_hz) {
    if (cal->n < 2) return 0.0f;
    if (freq_hz <= cal->pts[0].freq_hz)          return cal->pts[0].gain_db;
    if (freq_hz >= cal->pts[cal->n - 1].freq_hz) return cal->pts[cal->n - 1].gain_db;

    float lh = log10f(freq_hz);
    for (int i = 0; i < cal->n - 1; i++) {
        if (freq_hz >= cal->pts[i].freq_hz && freq_hz <= cal->pts[i + 1].freq_hz) {
            float l0 = log10f(cal->pts[i].freq_hz);
            float l1 = log10f(cal->pts[i + 1].freq_hz);
            float t  = (lh - l0) / (l1 - l0);
            return cal->pts[i].gain_db + t * (cal->pts[i + 1].gain_db - cal->pts[i].gain_db);
        }
    }
    return cal->pts[cal->n - 1].gain_db;
}

void mic_cal_bin_corrections(const MicCal *cal, const float *bin_freq,
                             float *corr_db, int n_bins) {
    for (int i = 0; i < n_bins; i++)
        corr_db[i] = mic_cal_at(cal, bin_freq[i]);
}
