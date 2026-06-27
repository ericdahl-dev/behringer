#include "osc_out.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

float bpm_to_normalized(float bpm) {
    if (bpm <= 0.0f) return 0.0f;
    float ms = 60000.0f / bpm;
    if (ms > 3000.0f) ms = 3000.0f;
    return ms / 3000.0f;
}

static int osc_str(uint8_t* buf, int idx, const char* s) {
    int len = (int)strlen(s) + 1;
    memcpy(buf + idx, s, len);
    idx += len;
    while (idx % 4) buf[idx++] = 0;
    return idx;
}

int osc_build_fx_delay(uint8_t* buf, int slot, float norm_val) {
    char path[24];
    snprintf(path, sizeof(path), "/fx/%d/par/01", slot);

    memset(buf, 0, 32);
    int idx = 0;
    idx = osc_str(buf, idx, path);
    idx = osc_str(buf, idx, ",f");

    uint32_t f;
    memcpy(&f, &norm_val, 4);
    buf[idx++] = (uint8_t)((f >> 24) & 0xff);
    buf[idx++] = (uint8_t)((f >> 16) & 0xff);
    buf[idx++] = (uint8_t)((f >>  8) & 0xff);
    buf[idx++] = (uint8_t)( f        & 0xff);

    return idx;
}
