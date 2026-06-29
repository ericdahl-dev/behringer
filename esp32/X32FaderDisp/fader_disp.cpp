#include "fader_disp.h"
#include "fader_db.h"
#include "osc_listener.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

#define FDR_MAXCH 32

static int      s_chan_count = FDR_CHAN_COUNT;
static char     s_orig_name[FDR_MAXCH + 1][16];   // 1-indexed
static bool     s_orig_cached[FDR_MAXCH + 1];
static uint32_t s_last_write[FDR_MAXCH + 1];

// "/ch/NN/..." -> NN (1..chan_count), or -1.
static int parse_ch(const char *a) {
    if (strncmp(a, "/ch/", 4) != 0) return -1;
    if (a[4] < '0' || a[4] > '9' || a[5] < '0' || a[5] > '9') return -1;
    int ch = (a[4] - '0') * 10 + (a[5] - '0');
    return (ch >= 1 && ch <= s_chan_count) ? ch : -1;
}

static int osc_str(uint8_t *b, int o, const char *s) {
    int n = (int)strlen(s) + 1;
    memcpy(b + o, s, n); o += n;
    while (o % 4) b[o++] = 0;
    return o;
}

static void set_name(int ch, const char *name) {
    uint8_t b[48]; char addr[24];
    snprintf(addr, sizeof addr, "/ch/%02d/config/name", ch);
    int o = osc_str(b, 0, addr);
    o = osc_str(b, o, ",s");
    o = osc_str(b, o, name);
    osc_listener_send(b, o);
}

static void query_name(int ch) {
    uint8_t b[32]; char addr[24];
    snprintf(addr, sizeof addr, "/ch/%02d/config/name", ch);
    int o = osc_str(b, 0, addr);
    osc_listener_send(b, o);   // node query → mixer replies with current name
}

void fader_disp_begin(int chan_count) {
    s_chan_count = (chan_count > 0 && chan_count <= FDR_MAXCH) ? chan_count : FDR_CHAN_COUNT;
    memset(s_orig_cached, 0, sizeof s_orig_cached);
    memset(s_last_write,  0, sizeof s_last_write);
    for (int ch = 1; ch <= s_chan_count; ch++) query_name(ch);
}

void fader_disp_handle(const osc_msg_t *m) {
    int ch = parse_ch(m->address);
    if (ch < 0) return;
    const char *suf = m->address + 6;   // after "/ch/NN"

    if (strcmp(suf, "/mix/fader") == 0 &&
        m->n_args >= 1 && m->args[0].type == OSC_ARG_FLOAT) {
        uint32_t now = (uint32_t)millis();
        if (now - s_last_write[ch] < FDR_NAME_REFRESH_MS) return;   // throttle
        s_last_write[ch] = now;
        char db[8];
        fader_db_str(m->args[0].val.f, db, sizeof db);
        set_name(ch, db);
    } else if (strcmp(suf, "/config/name") == 0 &&
               m->n_args >= 1 && m->args[0].type == OSC_ARG_STRING) {
        if (!s_orig_cached[ch]) {                                   // first sight = original
            strncpy(s_orig_name[ch], m->args[0].val.s, sizeof s_orig_name[ch] - 1);
            s_orig_name[ch][sizeof s_orig_name[ch] - 1] = 0;
            s_orig_cached[ch] = true;
        }
    }
}

void fader_disp_restore(void) {
#if FDR_RESTORE_ON_EXIT
    for (int ch = 1; ch <= s_chan_count; ch++)
        if (s_orig_cached[ch]) set_name(ch, s_orig_name[ch]);
#endif
}
