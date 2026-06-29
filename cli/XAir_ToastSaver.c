/*
 * XAir_ToastSaver.c
 *
 * Automatic feedback destroyer for wedding toasts on the XR18.
 * Subscribes to /meters/4 RTA (100 bins, ~20 Hz), detects feedback spikes,
 * and applies notch cuts via a TEQ inserted in a configured FX slot.
 *
 * Usage: XAir_ToastSaver -i <ip> [-c <ch 1-18>] [-s <slot 1-4>]
 *                        [-t <threshold_dB>] [-d <cut_dB>] [-r <release_sec>]
 *                        [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include "toast_logic.h"
#include "ringout_logic.h"
#include "ringout_profile.h"
#include "mic_cal.h"
#include "rta_bins.h"

extern int Xsprint(char *bd, int index, char format, void *bs);

/* ── constants ──────────────────────────────────────────────────────────── */

#define PORT             10024
#define BSIZE            512
#define XREMOTE_TIMEOUT  9      /* send /xremote every 9 seconds */
#define BASELINE_FRAMES  40     /* ~2 seconds at 20 Hz to establish baseline */
#define CONFIRM_FRAMES    3     /* consecutive frames a peak must persist before notching (~150ms) */
#define NARROW_DB        10.0f  /* dB advantage over neighbors required to classify as feedback */

/* ── GEQ labels (display/logging only) ──────────────────────────────────── */

static const char *GEQ_LABEL[31] = {
    "20Hz",  "25Hz",  "31.5Hz","40Hz",  "50Hz",  "63Hz",  "80Hz",
    "100Hz", "125Hz", "160Hz", "200Hz", "250Hz", "315Hz", "400Hz",
    "500Hz", "630Hz", "800Hz", "1kHz",  "1.25kHz","1.6kHz","2kHz",
    "2.5kHz","3.15kHz","4kHz", "5kHz",  "6.3kHz","8kHz",  "10kHz",
    "12.5kHz","16kHz","20kHz"
};

/* ── types ──────────────────────────────────────────────────────────────── */

typedef struct {
    int    active;
    float  cut_val;       /* float sent to TEQ (0.5 = flat) */
    double notched_at;    /* gettimeofday seconds */
    int    release_hold;  /* consecutive frames below release threshold */
    int    from_profile;  /* loaded via --load-profile: pinned, never auto-released */
} NotchState;

typedef struct {
    /* config */
    char   ip[20];
    int    channel;
    int    fx_slot;
    float  threshold_dB;
    float  cut_dB;
    float  release_sec;
    int    verbose;

    /* socket */
    int                fd;
    struct sockaddr_in xip;
    struct sockaddr   *xip_addr;
    socklen_t          xip_len;

    /* RTA */
    float   bins[100];
    float   baseline[100];
    float   baseline_accum[100];
    int     baseline_count;
    int     baseline_ready;
    float   mic_corr[100];   /* per-bin dB correction from --mic-cal (0 = none) */

    /* notch state */
    NotchState notches[31];  /* index 0 = GEQ par 01 */

    /* feedback confirmation counters — consecutive frames this par has been
     * the narrow peak; resets to 0 for all pars when a different par leads */
    int confirm_hold[31];

    /* doctor mode */
    int    doctor;
    int    doctor_fix;
    int    no_preflight;   /* --no-preflight: skip the auto pre-flight check */

    /* ── ring-out (proactive) — T-022 plumbing, control loop in T-023 ──────── */
    int    ringout;          /* --ringout: proactive mode instead of reactive */
    int    ro_bus;           /* target monitor bus 1-6 */
    float  ro_ceiling_db;    /* hard gain ceiling (dB) */
    float  ro_step_db;       /* ramp increment / back-off (dB) */
    float  ro_margin_db;     /* stop once gain-before-feedback margin reached */
    int    ro_max_notches;   /* stop after this many notches */
    float  ro_orig_fader;    /* captured original bus fader [0..1] — restore target */
    int    ro_orig_captured; /* 1 once ro_orig_fader is valid */
    int    ro_supervised;    /* --ringout-supervised: pause for operator each raise */
    float  ro_cur_gain_db;   /* gain currently commanded on the bus */
    char   ro_save_profile[256]; /* --save-profile PATH (ring-out) */
    char   ro_load_profile[256]; /* --load-profile PATH (reactive) */
} AppState;

/* ── globals ─────────────────────────────────────────────────────────────── */

static volatile int g_running = 1;

static void sig_handler(int sig) { (void)sig; g_running = 0; }

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* ── OSC send helpers ────────────────────────────────────────────────────── */

static void osc_send_raw(AppState *s, const char *buf, int len) {
    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
}

static void osc_no_args(AppState *s, const char *path) {
    char buf[BSIZE];
    int len = Xsprint(buf, 0, 's', (void *)path);
    osc_send_raw(s, buf, len);
}

static void osc_int(AppState *s, const char *path, int val) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",i");
    len = Xsprint(buf, len, 'i', &val);
    osc_send_raw(s, buf, len);
}

static void osc_float(AppState *s, const char *path, float val) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",f");
    len = Xsprint(buf, len, 'f', &val);
    osc_send_raw(s, buf, len);
}

static void osc_int_float(AppState *s, const char *path, int ival, float fval) {
    char buf[BSIZE];
    int len = 0;
    len = Xsprint(buf, len, 's', (void *)path);
    len = Xsprint(buf, len, 's', ",if");
    len = Xsprint(buf, len, 'i', &ival);
    len = Xsprint(buf, len, 'f', &fval);
    osc_send_raw(s, buf, len);
}

static void send_teq_band(AppState *s, int par, float val) {
    char path[32];
    snprintf(path, sizeof(path), "/fx/%d/par/%02d", s->fx_slot, par);
    osc_float(s, path, val);
}

/* ── ring-out gain drive / safety (T-022) ────────────────────────────────── */

/* Query a single float node (e.g. /bus/N/mix/fader) and read its reply.
 * Returns 0 and sets *out on success, -1 on timeout / malformed reply.
 * Call before subscribing to /meters so the reply isn't buried in meter blobs. */
static int osc_query_float(AppState *s, const char *path, float *out) {
    char buf[BSIZE];
    int  len = Xsprint(buf, 0, 's', (void *)path);
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(s->fd, &fds);

    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
    if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    int r = recvfrom(s->fd, buf, BSIZE - 1, 0, 0, 0);
    if (r <= 0) return -1;
    buf[r] = 0;
    if (strcmp(buf, path) != 0) return -1;

    int addr_padded = ((int)strlen(buf) + 1 + 3) & ~3;
    int tag_padded  = ((int)strlen(buf + addr_padded) + 1 + 3) & ~3;
    int off = addr_padded + tag_padded;
    if (off + 4 > r) return -1;

    uint32_t be; memcpy(&be, buf + off, 4);
    be = ntohl(be);                 /* OSC floats are big-endian */
    memcpy(out, &be, 4);
    return 0;
}

static int osc_query_int(AppState *s, const char *path, int *out) {
    char buf[BSIZE];
    int  len = Xsprint(buf, 0, 's', (void *)path);
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(s->fd, &fds);

    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
    if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    int r = recvfrom(s->fd, buf, BSIZE - 1, 0, 0, 0);
    if (r <= 0) return -1;
    buf[r] = 0;
    if (strcmp(buf, path) != 0) return -1;

    int addr_padded = ((int)strlen(buf) + 1 + 3) & ~3;
    int tag_padded  = ((int)strlen(buf + addr_padded) + 1 + 3) & ~3;
    int off = addr_padded + tag_padded;
    if (off + 4 > r) return -1;

    uint32_t be; memcpy(&be, buf + off, 4);
    *out = (int)ntohl(be);
    return 0;
}

/* Query a node ("/node ,s <name>") and copy the reply string (e.g. "TEQ ON")
 * into out. Returns 0 on success, -1 on timeout / malformed reply. */
static int osc_query_node(AppState *s, const char *node, char *out, int outsz) {
    char buf[BSIZE];
    int  len = 0;
    len = Xsprint(buf, len, 's', "/node");
    len = Xsprint(buf, len, 's', ",s");
    len = Xsprint(buf, len, 's', (void *)node);
    struct timeval tv = {1, 0};
    fd_set fds; FD_ZERO(&fds); FD_SET(s->fd, &fds);

    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
    if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    int r = recvfrom(s->fd, buf, BSIZE - 1, 0, 0, 0);
    if (r <= 0) return -1;
    buf[r] = 0;
    if (strncmp(buf, "node", 4) != 0) return -1;   /* X32 /node replies sans leading slash */
    int addr_padded = ((int)strlen(buf) + 1 + 3) & ~3;
    int tag_padded  = ((int)strlen(buf + addr_padded) + 1 + 3) & ~3;
    int off = addr_padded + tag_padded;
    if (off >= r) return -1;
    strncpy(out, buf + off, outsz - 1);
    out[outsz - 1] = 0;
    return 0;
}

/* Command the target monitor bus to a gain in dB, clamped to the ceiling.
 * The ceiling is also enforced upstream by the controller (T-021); this is the
 * belt-and-suspenders guard at the wire. */
static void ringout_set_gain(AppState *s, float dB) {
    if (dB > s->ro_ceiling_db) dB = s->ro_ceiling_db;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", s->ro_bus);
    osc_float(s, path, fader_db_to_float(dB));
    s->ro_cur_gain_db = dB;
}

/* Immediately restore the bus fader to its captured original value. Safe to
 * call any time after capture; bound to the SIGINT/SIGTERM shutdown path. */
static void ringout_panic(AppState *s) {
    if (!s->ro_orig_captured) return;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", s->ro_bus);
    osc_float(s, path, s->ro_orig_fader);
    fprintf(stderr, "[ring-out] restored bus %d fader to %.3f\n",
            s->ro_bus, s->ro_orig_fader);
}

static void subscribe_meters4(AppState *s) {
    char buf[BSIZE];
    int zero = 0;
    int len = 0;
    len = Xsprint(buf, len, 's', "/meters");
    len = Xsprint(buf, len, 's', ",siii");
    len = Xsprint(buf, len, 's', "/meters/4");
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    len = Xsprint(buf, len, 'i', &zero);
    osc_send_raw(s, buf, len);
}

/* ── doctor mode ─────────────────────────────────────────────────────────── */

static const char *EQ_MODE_NAME[] = {"PEQ", "GEQ", "TEQ"};

static int run_doctor(AppState *s, int fix) {
    char buf[BSIZE];
    int  len = Xsprint(buf, 0, 's', "/xinfo");
    struct timeval tv = {1, 0};
    fd_set fds;
    FD_ZERO(&fds); FD_SET(s->fd, &fds);

    printf("XAir Doctor — %s\n", s->ip);
    for (int i = 0; i < 40; i++) fputs("\xe2\x94\x80", stdout);
    printf("\n");

    sendto(s->fd, buf, len, 0, s->xip_addr, s->xip_len);
    if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0 ||
        recvfrom(s->fd, buf, BSIZE, 0, 0, 0) < 0 ||
        strcmp(buf, "/xinfo") != 0) {
        printf("[FAIL] XR18 not reachable at %s\n", s->ip);
        return 1;
    }
    printf("[PASS] XR18 reachable\n");
    printf("       RTA source ch %d  |  reactive FX slot %d\n", s->channel, s->fx_slot);

    int issues = 0;

    /* buses 1–6 */
    for (int n = 1; n <= 6; n++) {
        char path[32];
        snprintf(path, sizeof(path), "/bus/%d/eq/mode", n);
        int mode = -1;
        if (osc_query_int(s, path, &mode) != 0) {
            printf("[FAIL] Bus %d: no reply\n", n);
            issues++;
            continue;
        }
        const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
        if (mode == 1 || mode == 2) {
            printf("[PASS] Bus %d: %s\n", n, name);
        } else {
            printf("[FAIL] Bus %d: %s  <- needs GEQ or TEQ\n", n, name);
            issues++;
            if (fix) {
                osc_int(s, path, 2);
                printf("       -> set to TEQ\n");
            }
        }
    }

    /* LR */
    {
        int mode = -1;
        if (osc_query_int(s, "/lr/eq/mode", &mode) != 0) {
            printf("[FAIL] LR: no reply\n");
            issues++;
        } else {
            const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
            if (mode == 1 || mode == 2) {
                printf("[PASS] LR: %s\n", name);
            } else {
                printf("[FAIL] LR: %s  <- needs GEQ or TEQ\n", name);
                issues++;
                if (fix) {
                    osc_int(s, "/lr/eq/mode", 2);
                    printf("       -> set to TEQ\n");
                }
            }
        }
    }

    /* reactive mode notch target: the FX slot must hold a GEQ/TEQ */
    {
        char node[160], q[24];
        snprintf(q, sizeof q, "fx/%d", s->fx_slot);
        if (osc_query_node(s, q, node, sizeof node) != 0) {
            printf("[WARN] FX slot %d: no reply\n", s->fx_slot);
        } else if (strstr(node, "TEQ") || strstr(node, "GEQ")) {
            printf("[PASS] FX slot %d: %.40s\n", s->fx_slot, node);
        } else {
            printf("[FAIL] FX slot %d: %.40s  <- load a GEQ/TEQ for reactive notches\n",
                   s->fx_slot, node);
            issues++;
        }
    }

    /* RTA input is actually streaming (and carrying signal) */
    {
        subscribe_meters4(s);
        int   frames = 0;
        float peak   = -200.0f;
        for (int t = 0; t < 10; t++) {
            struct timeval tv2 = {0, 200000};
            fd_set f; FD_ZERO(&f); FD_SET(s->fd, &f);
            if (select(s->fd + 1, &f, NULL, NULL, &tv2) <= 0) continue;
            int r = recvfrom(s->fd, buf, BSIZE, 0, 0, 0);
            if (r > 0 && strcmp(buf, "/meters/4") == 0) {
                int ap = ((int)strlen(buf) + 1 + 3) & ~3;
                int tp = ((int)strlen(buf + ap) + 1 + 3) & ~3;
                int bs = ap + tp;
                if (bs < r && parse_meters4_blob((uint8_t *)buf + bs, r - bs, s->bins) == 0) {
                    frames++;
                    for (int i = 0; i < 100; i++) if (s->bins[i] > peak) peak = s->bins[i];
                }
            }
        }
        if (frames > 0)
            printf("[PASS] RTA /meters/4 streaming (%d frames, peak %.0f dBFS)\n", frames, peak);
        else {
            printf("[FAIL] RTA /meters/4: no frames — check the RTA source channel/tap\n");
            issues++;
        }
    }

    /* mic-cal status (client side) */
    {
        int loaded = 0;
        for (int i = 0; i < 100; i++) if (s->mic_corr[i] != 0.0f) { loaded = 1; break; }
        printf("[INFO] mic-cal: %s\n", loaded ? "loaded" : "not configured (--mic-cal)");
    }

    printf("\n");
    if (issues == 0) {
        printf("All outputs OK.\n");
    } else if (fix) {
        printf("%d output%s set to TEQ.\n", issues, issues == 1 ? "" : "s");
    } else {
        printf("%d issue%s found. Run with --fix to set failing outputs to TEQ.\n",
               issues, issues == 1 ? "" : "s");
    }
    return issues;
}

/* ── ANSI display ────────────────────────────────────────────────────────── */

/* Print the static header rows (once on startup). */
static void print_static_display(const AppState *s) {
    printf("\033[2J\033[H\033[?25l");
    printf("XAir Toast Saver  ch:%-2d  fx:%d  thr:%.0fdB  cut:%.0fdB  rel:%.0fs\n",
           s->channel, s->fx_slot, s->threshold_dB, s->cut_dB, s->release_sec);
    /* 100-char separator using box-drawing char U+2500 */
    for (int i = 0; i < 100; i++) fputs("\xe2\x94\x80", stdout);
    printf("\n");
    /* frequency ruler: "20Hz" at col 0, "1kHz" centered at col 54, "20kHz" at col 95 */
    printf("20Hz");
    for (int i = 4; i < 54; i++) putchar(' ');
    printf("1kHz");
    for (int i = 58; i < 95; i++) putchar(' ');
    printf("20kHz\n");
    printf("\n");  /* row 4: blank */
    fflush(stdout);
}

/* Redraw rows 5-7 in-place. Call each meter frame. */
static void draw_display(const AppState *s) {
    /* row 5: RTA bar or calibration progress */
    printf("\033[5;1H\033[2K");
    if (!s->baseline_ready) {
        int prog = s->baseline_count * 20 / BASELINE_FRAMES;
        printf("Calibrating... [");
        for (int i = 0; i < 20; i++)
            fputs(i < prog ? "\xe2\x96\x88" : "\xe2\x96\x91", stdout);
        printf("] %d/%d frames", s->baseline_count, BASELINE_FRAMES);
    } else {
        for (int i = 0; i < 100; i++) {
            /* check if this bin belongs to an active notch */
            int notched = 0;
            for (int j = 0; j < 31; j++) {
                if (s->notches[j].active && TOAST_GEQ_BIN[j] == i) { notched = 1; break; }
            }
            if (notched) {
                fputs("\033[31mN\033[0m", stdout);
            } else {
                float excess = s->bins[i] - s->baseline[i];
                if (excess >= s->threshold_dB)
                    fputs("\033[33m\xe2\x96\x88\033[0m", stdout); /* yellow █ */
                else if (excess >= 15.0f)
                    fputs("\xe2\x96\x93", stdout);                 /* ▓ */
                else if (excess >= 10.0f)
                    fputs("\xe2\x96\x92", stdout);                 /* ▒ */
                else if (excess >= 5.0f)
                    fputs("\xe2\x96\x91", stdout);                 /* ░ */
                else
                    putchar(' ');
            }
        }
    }

    /* row 6: notch marker row (▼ at each active notch's RTA bin) */
    printf("\n\033[2K");
    if (s->baseline_ready) {
        for (int i = 0; i < 100; i++) {
            int has_notch = 0;
            for (int j = 0; j < 31; j++) {
                if (s->notches[j].active && TOAST_GEQ_BIN[j] == i) { has_notch = 1; break; }
            }
            if (has_notch)
                fputs("\033[31m\xe2\x96\xbc\033[0m", stdout);  /* red ▼ */
            else
                putchar(' ');
        }
    }

    /* row 7: status line */
    printf("\n\033[2KNotched: ");
    int any = 0;
    if (s->baseline_ready) {
        double t = now_sec();
        for (int j = 0; j < 31; j++) {
            if (s->notches[j].active) {
                printf("%s(%.0fdB %.1fs)  ", GEQ_LABEL[j], s->cut_dB,
                       t - s->notches[j].notched_at);
                any = 1;
            }
        }
    }
    if (!any) printf("(none)");

    fflush(stdout);
}

/* ── detection and notch control ─────────────────────────────────────────── */

static void run_detection(AppState *s) {
    int release_frames = (int)(s->release_sec * 20.0f);
    float release_thr  = s->threshold_dB / 2.0f;

    /* release pass: check all active notches */
    for (int j = 0; j < 31; j++) {
        if (!s->notches[j].active) continue;
        if (s->notches[j].from_profile) continue;  /* pinned static notch — never auto-release */
        int bin = TOAST_GEQ_BIN[j];
        if (s->bins[bin] < s->baseline[bin] + release_thr) {
            s->notches[j].release_hold++;
            /* Smooth ramp from cut_val toward flat (0.5) over the release window */
            float t = (float)s->notches[j].release_hold / (float)release_frames;
            if (t > 1.0f) t = 1.0f;
            float ramp_val = s->notches[j].cut_val +
                             (0.5f - s->notches[j].cut_val) * t;
            send_teq_band(s, j + 1, ramp_val);
            if (s->notches[j].release_hold >= release_frames) {
                double elapsed = now_sec() - s->notches[j].notched_at;
                if (!s->verbose)
                    printf("\033[8;1H\033[2K");
                printf("[release] par=%02d (%s) after %.1fs\n",
                       j + 1, GEQ_LABEL[j], elapsed);
                if (!s->verbose) fflush(stdout);
                s->notches[j].active = 0;
                s->notches[j].release_hold = 0;
            }
        } else {
            if (s->notches[j].release_hold > 0) {
                /* Feedback returned mid-fade — snap back to full cut */
                s->notches[j].release_hold = 0;
                send_teq_band(s, j + 1, s->notches[j].cut_val);
            }
        }
    }

    /* detection pass: find peak bin above threshold */
    int peak_bin = -1;
    if (!detect_peak(s->bins, s->baseline, s->threshold_dB, &peak_bin)) {
        /* no spike this frame — clear all confirmation counters */
        for (int j = 0; j < 31; j++) s->confirm_hold[j] = 0;
        return;
    }

    /* narrowness gate — reject broadband transients (speech, claps, music) */
    if (!is_narrow_peak(s->bins, 100, peak_bin, 2, 3, NARROW_DB)) {
        for (int j = 0; j < 31; j++) s->confirm_hold[j] = 0;
        return;
    }

    /* accumulate confirmation for this par; reset all others */
    int par = bin_to_geq_par(peak_bin);
    for (int j = 0; j < 31; j++)
        s->confirm_hold[j] = (j == par - 1) ? s->confirm_hold[j] + 1 : 0;

    int j = par - 1;
    if (s->notches[j].active) return;           /* already notched */
    if (s->confirm_hold[j] < CONFIRM_FRAMES) return;  /* not confirmed yet */

    s->confirm_hold[j] = 0;
    float cut_float = db_to_geq_float(s->cut_dB);
    send_teq_band(s, par, cut_float);

    s->notches[j].active       = 1;
    s->notches[j].cut_val      = cut_float;
    s->notches[j].notched_at   = now_sec();
    s->notches[j].release_hold = 0;

    if (!s->verbose)
        printf("\033[8;1H\033[2K");
    printf("[notch] par=%02d (%s) cut=%.0fdB  bin=%d  level=%.1fdBFS\n",
           par, GEQ_LABEL[j], s->cut_dB, peak_bin, s->bins[peak_bin]);
    if (!s->verbose) fflush(stdout);
}

static void cleanup_notches(AppState *s) {
    int n = 0;
    for (int j = 0; j < 31; j++) {
        if (s->notches[j].active) {
            send_teq_band(s, j + 1, 0.5f);
            s->notches[j].active = 0;
            n++;
        }
    }
    if (n > 0)
        fprintf(stderr, "\n[cleanup] restored %d band(s) to flat\n", n);
}

/* ── /meters/4 blob handler ──────────────────────────────────────────────── */

static void handle_meters4(const uint8_t *blob, int blen, AppState *s) {
    if (parse_meters4_blob(blob, blen, s->bins) != 0) return;
    for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];  /* mic-cal */

    /* baseline calibration */
    if (!s->baseline_ready) {
        for (int i = 0; i < 100; i++)
            s->baseline_accum[i] += s->bins[i];
        s->baseline_count++;
        if (s->baseline_count >= BASELINE_FRAMES) {
            for (int i = 0; i < 100; i++)
                s->baseline[i] = s->baseline_accum[i] / BASELINE_FRAMES;
            s->baseline_ready = 1;
            float sum = 0.0f;
            for (int i = 0; i < 100; i++) sum += s->baseline[i];
            if (!s->verbose)
                printf("\033[8;1H\033[2K");
            printf("[baseline] established: avg noise floor = %.1f dBFS\n",
                   sum / 100.0f);
            if (!s->verbose) fflush(stdout);
        }
        if (!s->verbose) draw_display(s);
        return;
    }

    /* exponential baseline update */
    update_baseline(s->baseline, s->bins, 100, 0.005f);

    if (s->verbose) {
        int peak = 0;
        for (int i = 1; i < 100; i++)
            if (s->bins[i] > s->bins[peak]) peak = i;
        printf("peak bin=%d  level=%.1f dBFS  baseline=%.1f dBFS  excess=%.1f dB\n",
               peak, s->bins[peak], s->baseline[peak],
               s->bins[peak] - s->baseline[peak]);
    }

    run_detection(s);
    if (!s->verbose) draw_display(s);
}

/* ── ring-out profile save / load (T-024) ────────────────────────────────── */

/* Write the completed ring-out result (notches + margin + target) to a file. */
static void ringout_save_profile(AppState *s, float margin_db) {
    RingoutProfile p;
    memset(&p, 0, sizeof(p));
    strncpy(p.model, "XR18", sizeof(p.model) - 1);
    p.bus = s->ro_bus;
    p.fx_slot = s->fx_slot;
    p.margin_db = margin_db;
    time_t tnow = time(NULL);
    strftime(p.created, sizeof(p.created), "%Y-%m-%dT%H:%M:%S", localtime(&tnow));
    int count = 0;
    for (int j = 0; j < 31; j++)
        if (s->notches[j].active) { p.band_db[j] = s->cut_dB; count++; }

    char buf[2048];
    int n = ringout_profile_serialize(&p, buf, sizeof(buf));
    if (n < 0) { fprintf(stderr, "[ring-out] profile serialize failed\n"); return; }
    FILE *f = fopen(s->ro_save_profile, "w");
    if (!f) { fprintf(stderr, "[ring-out] cannot write %s\n", s->ro_save_profile); return; }
    fwrite(buf, 1, n, f);
    fclose(f);
    printf("[ring-out] saved profile to %s (%d notch%s, margin %.1fdB)\n",
           s->ro_save_profile, count, count == 1 ? "" : "es", margin_db);
}

/* Load a profile and pre-place its static notches for reactive protection.
 * Returns 0 on success, -1 on read/parse/mismatch error. */
static int ringout_load_profile(AppState *s) {
    FILE *f = fopen(s->ro_load_profile, "r");
    if (!f) { fprintf(stderr, "error: cannot read profile %s\n", s->ro_load_profile); return -1; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) { fprintf(stderr, "error: empty profile %s\n", s->ro_load_profile); return -1; }
    buf[n] = 0;

    RingoutProfile p;
    if (ringout_profile_parse(buf, &p) != 0) {
        fprintf(stderr, "error: malformed profile %s\n", s->ro_load_profile); return -1;
    }
    /* mismatch guard: notch bands target a TEQ in a specific model + FX slot. */
    if (strcmp(p.model, "XR18") != 0) {
        fprintf(stderr, "error: profile model '%s' != XR18\n", p.model); return -1;
    }
    if (p.fx_slot != s->fx_slot) {
        fprintf(stderr, "error: profile fx_slot %d != -s %d\n", p.fx_slot, s->fx_slot); return -1;
    }

    int applied = 0;
    for (int j = 0; j < 31; j++) {
        if (p.band_db[j] < 0.0f) {
            float v = db_to_geq_float(p.band_db[j]);
            send_teq_band(s, j + 1, v);
            s->notches[j].active       = 1;
            s->notches[j].cut_val      = v;
            s->notches[j].from_profile = 1;
            s->notches[j].notched_at   = now_sec();
            applied++;
        }
    }
    fprintf(stderr, "[load] applied %d pinned profile notch%s from %s "
            "(made for bus %d, margin %.1fdB)\n",
            applied, applied == 1 ? "" : "es", s->ro_load_profile, p.bus, p.margin_db);
    return 0;
}

/* ── ring-out flow (T-023: supervised ramp/detect/notch control loop) ─────── */

/* Supervised gate: in --ringout-supervised, pause before each gain raise until
 * the operator presses Enter ('q' aborts). Returns 1 to proceed, 0 to stop. */
static int ringout_prompt_continue(float next_db) {
    printf("[ring-out] Enter to raise to %.1fdB (q+Enter to stop): ", next_db);
    fflush(stdout);
    int c = getchar();
    if (c == 'q' || c == 'Q') { g_running = 0; return 0; }
    while (c != '\n' && c != EOF) c = getchar();
    return 1;
}

/* Capture the bus's current fader (so it can always be restored), insert the
 * notch TEQ on that bus, calibrate the noise floor, then ramp gain up driving
 * the pure ringout_step() controller — notching each ring as it appears — until
 * a ceiling / target-margin / max-notch stop or an abort. The fader is always
 * restored on exit; notches are kept only if the run completed cleanly. */
static void run_ringout(AppState *s) {
    char path[40];

    /* 1) capture original fader BEFORE touching anything — refuse to drive a
     *    bus we cannot restore. */
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", s->ro_bus);
    if (osc_query_float(s, path, &s->ro_orig_fader) != 0) {
        fprintf(stderr, "error: no reply reading %s — aborting ring-out "
                "(refusing to drive a bus we can't restore)\n", path);
        return;
    }
    s->ro_orig_captured = 1;
    float start_db = fader_float_to_db(s->ro_orig_fader);

    /* 2) insert the TEQ FX slot on the target monitor bus so notches act there */
    snprintf(path, sizeof(path), "/bus/%d/insert/sel", s->ro_bus);
    osc_int(s, path, s->fx_slot);          /* 0=OFF, 1..4 = Fx1..Fx4 */
    snprintf(path, sizeof(path), "/bus/%d/insert/on", s->ro_bus);
    osc_int(s, path, 1);

    /* 3) RTA source + subscribe + keepalive */
    osc_int(s, "/-stat/rta/source", s->channel);
    osc_int_float(s, "/-prefs/rta", 0, 0.25f);
    subscribe_meters4(s);
    osc_no_args(s, "/xremote");

    /* 4) assert the known start gain (clamped to ceiling) */
    ringout_set_gain(s, start_db);

    printf("[ring-out] bus %d  start %.1fdB (fader %.3f)  ceiling %.1fdB  "
           "step %.1fdB  margin %.1fdB  max-notch %d  fx slot %d\n",
           s->ro_bus, start_db, s->ro_orig_fader, s->ro_ceiling_db,
           s->ro_step_db, s->ro_margin_db, s->ro_max_notches, s->fx_slot);
    printf("[ring-out] RTA source = ch %d — ensure it taps the bus %d monitor "
           "mic before ramping.\n", s->channel, s->ro_bus);
    if (start_db > s->ro_ceiling_db)
        printf("[ring-out] WARNING: start gain %.1fdB exceeds ceiling %.1fdB; "
               "clamped down.\n", start_db, s->ro_ceiling_db);
    printf("[ring-out] calibrating noise floor (%d frames)...\n", BASELINE_FRAMES);

    /* ── baseline calibration at start gain ── */
    double accum[100];
    for (int i = 0; i < 100; i++) accum[i] = 0.0;
    int    got = 0;
    double last_xremote = now_sec();
    char   r_buf[BSIZE];
    while (g_running && got < BASELINE_FRAMES) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_no_args(s, "/xremote"); subscribe_meters4(s); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(s->fd, &fds);
        if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        int r_len = recvfrom(s->fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0 || strcmp(r_buf, "/meters/4") != 0) continue;
        int ap = ((int)strlen(r_buf) + 1 + 3) & ~3;
        int tp = ((int)strlen(r_buf + ap) + 1 + 3) & ~3;
        if (ap + tp >= r_len) continue;
        if (parse_meters4_blob((uint8_t *)r_buf + ap + tp, r_len - ap - tp, s->bins) != 0) continue;
        for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];  /* mic-cal */
        for (int i = 0; i < 100; i++) accum[i] += s->bins[i];
        got++;
    }
    if (!g_running) { ringout_panic(s); return; }
    for (int i = 0; i < 100; i++) s->baseline[i] = (float)(accum[i] / (got ? got : 1));

    /* ── configure the controller from the CLI args ── */
    RingoutConfig cfg;
    cfg.start_gain_db    = start_db;
    cfg.ceiling_db       = s->ro_ceiling_db;
    cfg.step_db          = s->ro_step_db;
    cfg.target_margin_db = s->ro_margin_db;
    cfg.max_notches      = s->ro_max_notches;
    cfg.threshold_db     = s->threshold_dB;
    cfg.cut_db           = s->cut_dB;
    cfg.confirm_frames   = CONFIRM_FRAMES;
    cfg.settle_frames    = 10;          /* ~0.5 s at 20 Hz after a gain change/notch */
    cfg.stable_frames    = 10;          /* ~0.5 s ring-free before raising */
    cfg.narrow_skip      = 2;
    cfg.narrow_span      = 3;
    cfg.narrow_min_db    = NARROW_DB;
    RingoutState st;
    ringout_init(&st, &cfg);

    printf("[ring-out] ramping toward ceiling %.1fdB (step %.1fdB)%s\n",
           s->ro_ceiling_db, s->ro_step_db, s->ro_supervised ? " — supervised" : "");

    /* ── control loop: one ringout_step() per RTA frame ── */
    int completed = 0;
    while (g_running) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_no_args(s, "/xremote"); subscribe_meters4(s); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(s->fd, &fds);
        if (select(s->fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        int r_len = recvfrom(s->fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0 || strcmp(r_buf, "/meters/4") != 0) continue;
        int ap = ((int)strlen(r_buf) + 1 + 3) & ~3;
        int tp = ((int)strlen(r_buf + ap) + 1 + 3) & ~3;
        if (ap + tp >= r_len) continue;
        if (parse_meters4_blob((uint8_t *)r_buf + ap + tp, r_len - ap - tp, s->bins) != 0) continue;
        for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];  /* mic-cal */

        RingoutAction a = ringout_step(&st, s->bins, s->baseline);
        switch (a.op) {
        case RO_RAISE_GAIN:
            if (s->ro_supervised && !ringout_prompt_continue(a.gain_db)) break;
            ringout_set_gain(s, a.gain_db);
            printf("[ring-out] raise -> %.1fdB  (%.1fdB to ceiling, %d notch%s)\n",
                   a.gain_db, s->ro_ceiling_db - a.gain_db,
                   st.n_notches, st.n_notches == 1 ? "" : "es");
            break;
        case RO_BACK_OFF:
            ringout_set_gain(s, a.gain_db);
            printf("[ring-out] back off -> %.1fdB\n", a.gain_db);
            break;
        case RO_PLACE_NOTCH: {
            int j = a.geq_par - 1;
            send_teq_band(s, a.geq_par, db_to_geq_float(a.cut_db));
            s->notches[j].active     = 1;
            s->notches[j].cut_val    = db_to_geq_float(a.cut_db);
            s->notches[j].notched_at = now_sec();
            printf("[ring-out] notch par=%02d (%s) %.0fdB at %.1fdB\n",
                   a.geq_par, GEQ_LABEL[j], a.cut_db, st.cur_gain_db);
            break;
        }
        case RO_DONE:  completed = 1; break;
        case RO_ABORT: completed = 0; break;
        case RO_HOLD: default: break;
        }
        if (a.op == RO_DONE || a.op == RO_ABORT) break;
    }

    /* ── summary + restore ── */
    printf("\n[ring-out] ");
    if (!g_running && st.done_reason == RO_REASON_NONE)
        printf("interrupted by operator.\n");
    else if (st.done_reason == RO_REASON_ABORT)
        printf("ABORTED: %s\n", ringout_reason_str(st.done_reason));
    else
        printf("complete: %s\n", ringout_reason_str(st.done_reason));

    printf("[ring-out] notches: ");
    int any = 0;
    for (int j = 0; j < 31; j++)
        if (s->notches[j].active) { printf("%s ", GEQ_LABEL[j]); any = 1; }
    if (!any) printf("(none)");
    printf("\n[ring-out] gain-before-feedback margin: %.1f dB above start (%.1fdB)\n",
           st.margin_db, start_db);

    ringout_panic(s);                 /* always return the fader to the operator's level */
    if (completed) {
        if (s->ro_save_profile[0]) ringout_save_profile(s, st.margin_db);
        printf("[ring-out] notches LEFT in place (the ring-out result).\n");
    } else {
        cleanup_notches(s);           /* incomplete run → revert to as-found */
        printf("[ring-out] notches reverted (run did not complete).\n");
    }
}

/* Load a mic calibration file and bake it into per-RTA-bin dB corrections. */
static void load_mic_cal(AppState *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "mic-cal: cannot open %s\n", path); return; }
    char buf[16384];
    int n = (int)fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n < 0 ? 0 : n] = '\0';
    MicCal cal;
    if (mic_cal_parse(buf, n, &cal) != 0) {
        fprintf(stderr, "mic-cal: parse failed (%s) — no correction applied\n", path);
        return;
    }
    mic_cal_bin_corrections(&cal, RTA_BIN_FREQ, s->mic_corr, RTA_BIN_COUNT);
    fprintf(stderr, "mic-cal: %d points from %s applied to RTA bins\n", cal.n, path);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    AppState s;
    memset(&s, 0, sizeof(s));

    /* defaults */
    strcpy(s.ip, "192.168.0.64");
    s.channel      = 1;
    s.fx_slot      = 4;
    s.threshold_dB = 20.0f;
    s.cut_dB       = -6.0f;
    s.release_sec  = 10.0f;

    /* ring-out defaults */
    s.ro_bus         = 1;
    s.ro_ceiling_db  = 0.0f;    /* don't push a monitor past unity by default */
    s.ro_step_db     = 1.0f;
    s.ro_margin_db   = 6.0f;
    s.ro_max_notches = 8;

    enum { OPT_RINGOUT = 1000, OPT_RO_BUS, OPT_RO_CEIL,
           OPT_RO_STEP, OPT_RO_MARGIN, OPT_RO_MAXN, OPT_RO_SUP,
           OPT_MIC_CAL, OPT_SAVE_PROFILE, OPT_LOAD_PROFILE,
           OPT_DOCTOR, OPT_FIX, OPT_NO_PREFLIGHT };
    static struct option long_opts[] = {
        {"mic-cal",            required_argument, 0, OPT_MIC_CAL},
        {"ringout",            no_argument,       0, OPT_RINGOUT},
        {"ringout-bus",        required_argument, 0, OPT_RO_BUS},
        {"ringout-ceiling",    required_argument, 0, OPT_RO_CEIL},
        {"ringout-step",       required_argument, 0, OPT_RO_STEP},
        {"ringout-margin",     required_argument, 0, OPT_RO_MARGIN},
        {"ringout-max-notches",required_argument, 0, OPT_RO_MAXN},
        {"ringout-supervised", no_argument,       0, OPT_RO_SUP},
        {"save-profile",       required_argument, 0, OPT_SAVE_PROFILE},
        {"load-profile",       required_argument, 0, OPT_LOAD_PROFILE},
        {"doctor",             no_argument,       0, OPT_DOCTOR},
        {"fix",                no_argument,       0, OPT_FIX},
        {"no-preflight",       no_argument,       0, OPT_NO_PREFLIGHT},
        {0, 0, 0, 0}
    };

    int opt, ip_set = 0;
    while ((opt = getopt_long(argc, argv, "i:c:s:t:d:r:vh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'i': strncpy(s.ip, optarg, sizeof(s.ip) - 1); ip_set = 1; break;
        case 'c': s.channel      = atoi(optarg); break;
        case 's': s.fx_slot      = atoi(optarg); break;
        case 't': s.threshold_dB = atof(optarg); break;
        case 'd': s.cut_dB       = atof(optarg); break;
        case 'r': s.release_sec  = atof(optarg); break;
        case 'v': s.verbose      = 1;            break;
        case OPT_MIC_CAL:   load_mic_cal(&s, optarg);        break;
        case OPT_RINGOUT:   s.ringout        = 1;            break;
        case OPT_RO_BUS:    s.ro_bus         = atoi(optarg); break;
        case OPT_RO_CEIL:   s.ro_ceiling_db  = atof(optarg); break;
        case OPT_RO_STEP:   s.ro_step_db     = atof(optarg); break;
        case OPT_RO_MARGIN: s.ro_margin_db   = atof(optarg); break;
        case OPT_RO_MAXN:   s.ro_max_notches = atoi(optarg); break;
        case OPT_RO_SUP:    s.ro_supervised  = 1;            break;
        case OPT_SAVE_PROFILE: strncpy(s.ro_save_profile, optarg, sizeof(s.ro_save_profile)-1); break;
        case OPT_LOAD_PROFILE: strncpy(s.ro_load_profile, optarg, sizeof(s.ro_load_profile)-1); break;
        case OPT_DOCTOR: s.doctor     = 1; break;
        case OPT_FIX:    s.doctor_fix = 1; break;
        case OPT_NO_PREFLIGHT: s.no_preflight = 1; break;
        default:
        case 'h':
            fprintf(stderr,
                "usage: XAir_ToastSaver -i <ip> [-c <ch 1-18>] [-s <slot 1-4>]\n"
                "                       [-t <threshold_dB>] [-d <cut_dB>]\n"
                "                       [-r <release_sec>] [-v]\n"
                "       XAir_ToastSaver -i <ip> --ringout [--ringout-bus N]\n"
                "                       [--ringout-ceiling dB] [--ringout-step dB]\n"
                "                       [--ringout-margin dB] [--ringout-max-notches N]\n"
                "       XAir_ToastSaver -i <ip> --doctor [--fix]\n"
                "  -i  XR18 IP address (required)\n"
                "  -c  RTA source channel 1-18 (default: 1)\n"
                "  -s  FX slot with TEQ, 1-4 (default: 4)\n"
                "  -t  spike threshold in dB above noise floor (default: 20)\n"
                "  -d  notch cut depth in dB, negative (default: -6)\n"
                "  -r  seconds before a notch is released (default: 10)\n"
                "  -v  verbose text output (disables live display)\n"
                "  --mic-cal <file>       REW .cal / factory .txt mic correction for the RTA\n"
                "  --ringout              proactive ring-out mode (drives a monitor bus)\n"
                "  --ringout-bus N        target monitor bus 1-6 (default: 1)\n"
                "  --ringout-ceiling dB   hard gain ceiling (default: 0)\n"
                "  --ringout-step dB      ramp/back-off increment (default: 1)\n"
                "  --ringout-margin dB    stop once this much headroom is gained (default: 6)\n"
                "  --ringout-max-notches N stop after N notches (default: 8)\n"
                "  --ringout-supervised   pause for Enter before each gain raise\n"
                "  --save-profile PATH    (ring-out) write the result profile to PATH\n"
                "  --load-profile PATH    (reactive) pre-place a saved profile's notches\n"
                "  --doctor               check all bus/LR outputs have GEQ or TEQ mode set\n"
                "  --fix                  (with --doctor) set failing outputs to TEQ\n"
                "  --no-preflight         skip the auto pre-flight readiness check\n");
            return opt == 'h' ? 0 : 1;
        }
    }

    if (!ip_set) {
        fprintf(stderr, "error: -i <ip> is required\n");
        return 1;
    }
    if (s.channel < 1 || s.channel > 18) {
        fprintf(stderr, "error: channel must be 1-18\n"); return 1;
    }
    if (s.fx_slot < 1 || s.fx_slot > 4) {
        fprintf(stderr, "error: fx slot must be 1-4\n"); return 1;
    }
    if (s.cut_dB > 0.0f) {
        fprintf(stderr, "warning: cut_dB should be negative; got %.1f\n", s.cut_dB);
    }
    if (s.ringout) {
        if (s.ro_bus < 1 || s.ro_bus > 6) {
            fprintf(stderr, "error: --ringout-bus must be 1-6\n"); return 1;
        }
        if (s.ro_step_db <= 0.0f) {
            fprintf(stderr, "error: --ringout-step must be > 0\n"); return 1;
        }
        if (s.ro_max_notches < 1) {
            fprintf(stderr, "error: --ringout-max-notches must be >= 1\n"); return 1;
        }
        if (s.ro_load_profile[0])
            fprintf(stderr, "warning: --load-profile is ignored in ring-out mode\n");
    } else if (s.ro_save_profile[0]) {
        fprintf(stderr, "warning: --save-profile only applies to --ringout mode\n");
    }

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* ── create UDP socket ── */
    if ((s.fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }
    memset(&s.xip, 0, sizeof(s.xip));
    s.xip.sin_family      = AF_INET;
    s.xip.sin_addr.s_addr = inet_addr(s.ip);
    s.xip.sin_port        = htons(PORT);
    s.xip_addr            = (struct sockaddr *)&s.xip;
    s.xip_len             = sizeof(s.xip);

    /* ── doctor mode ── */
    if (s.doctor) {
        run_doctor(&s, s.doctor_fix);
        close(s.fd);
        return 0;
    }

    if (!s.verbose)
        printf("XAir Toast Saver  connecting to %s  ch:%d  slot:%d  "
               "threshold:%.0fdB  cut:%.0fdB  release:%.0fs\n",
               s.ip, s.channel, s.fx_slot, s.threshold_dB, s.cut_dB, s.release_sec);

    /* ── /xinfo handshake ── */
    {
        char buf[BSIZE];
        int  len = Xsprint(buf, 0, 's', "/xinfo");
        struct timeval tv = {1, 0};
        fd_set fds;
        FD_ZERO(&fds); FD_SET(s.fd, &fds);

        sendto(s.fd, buf, len, 0, s.xip_addr, s.xip_len);
        if (select(s.fd + 1, &fds, NULL, NULL, &tv) <= 0 ||
            recvfrom(s.fd, buf, BSIZE, 0, 0, 0) < 0 ||
            strcmp(buf, "/xinfo") != 0) {
            fprintf(stderr, "error: no /xinfo reply from %s\n", s.ip);
            close(s.fd);
            return 1;
        }
        if (s.verbose) printf("connected to %s\n", s.ip);
    }

    /* ── auto pre-flight: never run against an output we can't notch ── */
    if (!s.no_preflight) {
        int n = run_doctor(&s, s.doctor_fix);
        if (n > 0 && !s.doctor_fix) {
            fprintf(stderr, "\n*** pre-flight found %d issue(s) above. Set the outputs up, "
                            "re-run with --fix, or --no-preflight to override. ***\n", n);
            close(s.fd);
            return 1;
        }
        printf("\n");
    }

    /* ── ring-out (proactive) takes its own flow ── */
    if (s.ringout) {
        run_ringout(&s);   /* owns all restore: fader always; notches per outcome */
        close(s.fd);
        printf("toast saver (ring-out) stopped\n");
        return 0;
    }

    /* ── configure RTA source and subscribe ── */
    osc_int(&s, "/-stat/rta/source", s.channel);
    osc_int_float(&s, "/-prefs/rta", 0, 0.25f);  /* PEAK mode, decay 0.25 */
    subscribe_meters4(&s);
    osc_no_args(&s, "/xremote");

    /* ── reactive handoff: pre-place a saved ring-out profile's notches ── */
    if (s.ro_load_profile[0] && ringout_load_profile(&s) != 0) {
        close(s.fd);
        return 1;
    }

    if (!s.verbose) print_static_display(&s);

    /* ── main loop ── */
    double last_xremote = now_sec();
    char r_buf[BSIZE];

    while (g_running) {
        /* keepalive */
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_no_args(&s, "/xremote");
            subscribe_meters4(&s);  /* re-subscribe alongside keepalive */
            last_xremote = t;
        }

        /* poll */
        struct timeval tv = {0, 60000};  /* 60 ms */
        fd_set fds;
        FD_ZERO(&fds); FD_SET(s.fd, &fds);
        if (select(s.fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;

        int r_len = recvfrom(s.fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0) continue;
        if (strcmp(r_buf, "/meters/4") != 0) continue;

        /* locate blob: skip OSC address (4-byte padded) and type tag (4-byte padded) */
        int addr_padded = ((int)strlen(r_buf) + 1 + 3) & ~3;
        int tag_padded  = ((int)strlen(r_buf + addr_padded) + 1 + 3) & ~3;
        int blob_start  = addr_padded + tag_padded;
        if (blob_start >= r_len) continue;

        handle_meters4((uint8_t *)r_buf + blob_start, r_len - blob_start, &s);
    }

    /* ── shutdown ── */
    cleanup_notches(&s);
    close(s.fd);
    if (!s.verbose) {
        printf("\033[9;1H\033[?25h");  /* show cursor below display */
        fflush(stdout);
    }
    printf("toast saver stopped\n");
    return 0;
}
