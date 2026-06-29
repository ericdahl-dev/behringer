#include "ringout_profile.h"
#include <stdio.h>
#include <string.h>

/* Text format (v1), one token-set per line:
 *
 *   # XAir ToastSaver ring-out profile v1
 *   model XR18
 *   bus 3
 *   fx_slot 4
 *   margin_db 6.0
 *   band 01 0.0
 *   ...
 *   band 31 -6.0
 */

int ringout_profile_serialize(const RingoutProfile *p, char *buf, int buflen) {
    int n = 0, w;

    w = snprintf(buf + n, buflen - n,
                 "# XAir ToastSaver ring-out profile v1\n"
                 "model %s\nbus %d\nfx_slot %d\nmargin_db %.1f\n",
                 p->model, p->bus, p->fx_slot, p->margin_db);
    if (w < 0 || w >= buflen - n) return -1;
    n += w;

    if (p->created[0]) {
        w = snprintf(buf + n, buflen - n, "created %s\n", p->created);
        if (w < 0 || w >= buflen - n) return -1;
        n += w;
    }

    for (int i = 0; i < RINGOUT_PROFILE_BANDS; i++) {
        w = snprintf(buf + n, buflen - n, "band %02d %.1f\n", i + 1, p->band_db[i]);
        if (w < 0 || w >= buflen - n) return -1;
        n += w;
    }
    return n;
}

int ringout_profile_parse(const char *text, RingoutProfile *p) {
    memset(p, 0, sizeof(*p));

    int seen_model = 0, seen_bus = 0, seen_slot = 0;
    const char *line = text;

    while (line && *line) {
        const char *nl = strchr(line, '\n');
        char tmp[128];
        int len = nl ? (int)(nl - line) : (int)strlen(line);
        if (len >= (int)sizeof(tmp)) len = (int)sizeof(tmp) - 1;
        memcpy(tmp, line, len);
        tmp[len] = 0;

        if (tmp[0] != '#' && tmp[0] != '\0') {
            int   par;
            float val;
            char  model[8];
            char  created[24];
            int   ival;
            if (sscanf(tmp, "model %7s", model) == 1) {
                strncpy(p->model, model, sizeof(p->model) - 1);
                seen_model = 1;
            } else if (sscanf(tmp, "created %23s", created) == 1) {
                strncpy(p->created, created, sizeof(p->created) - 1);
            } else if (sscanf(tmp, "bus %d", &ival) == 1) {
                p->bus = ival; seen_bus = 1;
            } else if (sscanf(tmp, "fx_slot %d", &ival) == 1) {
                p->fx_slot = ival; seen_slot = 1;
            } else if (sscanf(tmp, "margin_db %f", &val) == 1) {
                p->margin_db = val;
            } else if (sscanf(tmp, "band %d %f", &par, &val) == 2) {
                if (par >= 1 && par <= RINGOUT_PROFILE_BANDS)
                    p->band_db[par - 1] = val;
            }
        }

        if (!nl) break;
        line = nl + 1;
    }

    /* require the identifying header fields */
    if (!seen_model || !seen_bus || !seen_slot) return -1;
    return 0;
}
