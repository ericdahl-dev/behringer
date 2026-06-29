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
#include "osc_io.h"
#include "tui.h"
#include <stdarg.h>

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
    /* config */
    char   ip[20];
    int    channel;
    int    fx_slot;
    float  threshold_dB;
    float  cut_dB;
    float  release_sec;
    int    verbose;

    /* socket */
    OscConn conn;

    /* RTA */
    float   bins[100];
    float   baseline[100];
    float   baseline_accum[100];
    int     baseline_count;
    int     baseline_ready;
    float   mic_corr[100];   /* per-bin dB correction from --mic-cal (0 = none) */

    /* reactive detection state */
    ToastState toast;
    double notched_at[31];        /* wall-clock stamp per band (display only) */
    float  ro_notch_at_gain[31];  /* ring-out: gain_dB when each notch was placed */

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
static volatile int g_resized = 0;

static void sig_handler(int sig)    { (void)sig; g_running = 0; }
static void sigwinch_handler(int s) { (void)s;   g_resized = 1; }

/* ── log ring buffer ─────────────────────────────────────────────────────── */

#define LOG_CAP  200
#define LOG_LINE 128
static char        g_log[LOG_CAP][LOG_LINE];
static const char *g_log_ptrs[LOG_CAP];
static int         g_log_head   = 0;
static int         g_log_count  = 0;
static int         g_log_scroll = 0;

static void log_push(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_log[g_log_head], LOG_LINE, fmt, ap);
    va_end(ap);
    g_log[g_log_head][LOG_LINE - 1] = '\0';
    g_log_head = (g_log_head + 1) % LOG_CAP;
    if (g_log_count < LOG_CAP) g_log_count++;
    if (g_log_scroll > 0) g_log_scroll++;  /* keep relative position */
    for (int i = 0; i < g_log_count; i++)
        g_log_ptrs[i] = g_log[(g_log_head - g_log_count + i + LOG_CAP) % LOG_CAP];
}

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* ── TUI context helpers ─────────────────────────────────────────────────── */

static TuiReactiveCtx make_reactive_ctx(const AppState *s) {
    TuiReactiveCtx c;
    memset(&c, 0, sizeof(c));
    c.ip             = s->ip;
    c.channel        = s->channel;
    c.fx_slot        = s->fx_slot;
    c.threshold_dB   = s->threshold_dB;
    c.cut_dB         = s->cut_dB;
    c.release_sec    = s->release_sec;
    c.bins           = s->bins;
    c.baseline       = s->baseline;
    c.baseline_ready = s->baseline_ready;
    c.baseline_count = s->baseline_count;
    c.baseline_frames = BASELINE_FRAMES;
    c.toast          = &s->toast;
    c.notched_at     = s->notched_at;
    c.geq_labels     = GEQ_LABEL;
    c.now            = now_sec();
    return c;
}

/* Doctor item helper: if items != NULL fills the array and redraws; else printf. */
static void doc_report(AppState *s, DoctorItem *items, int *n_items,
                       int pass, const char *fmt, ...) {
    char msg[256];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    if (items && n_items) {
        if (*n_items < 20) {
            items[*n_items].pass = pass;
            strncpy(items[*n_items].text, msg, sizeof(items[*n_items].text) - 1);
            items[*n_items].text[sizeof(items[*n_items].text) - 1] = '\0';
            (*n_items)++;
        }
        tui_draw_doctor(s->ip, items, *n_items);
    } else {
        printf("[%s] %s\n",
               pass == 1 ? "PASS" : (pass == 0 ? "FAIL" : "INFO"), msg);
        fflush(stdout);
    }
}

/* ── OSC send helpers ────────────────────────────────────────────────────── */

static void send_teq_band(AppState *s, int par, float val) {
    char path[32];
    snprintf(path, sizeof(path), "/fx/%d/par/%02d", s->fx_slot, par);
    osc_send_float(&s->conn, path, val);
}

/* ── ring-out gain drive / safety (T-022) ────────────────────────────────── */


/* Command the target monitor bus to a gain in dB, clamped to the ceiling.
 * The ceiling is also enforced upstream by the controller (T-021); this is the
 * belt-and-suspenders guard at the wire. */
static void ringout_set_gain(AppState *s, float dB) {
    if (dB > s->ro_ceiling_db) dB = s->ro_ceiling_db;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", s->ro_bus);
    osc_send_float(&s->conn, path, fader_db_to_float(dB));
    s->ro_cur_gain_db = dB;
}

/* Immediately restore the bus fader to its captured original value. Safe to
 * call any time after capture; bound to the SIGINT/SIGTERM shutdown path. */
static void ringout_panic(AppState *s) {
    if (!s->ro_orig_captured) return;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", s->ro_bus);
    osc_send_float(&s->conn, path, s->ro_orig_fader);
    fprintf(stderr, "[ring-out] restored bus %d fader to %.3f\n",
            s->ro_bus, s->ro_orig_fader);
}

static void subscribe_meters4(AppState *s) {
    osc_subscribe_meters4(&s->conn);
}

/* ── doctor mode ─────────────────────────────────────────────────────────── */

static const char *EQ_MODE_NAME[] = {"PEQ", "GEQ", "TEQ"};

/* items/n_items: non-NULL → TUI mode (stream results); NULL → plain printf. */
static int run_doctor(AppState *s, int fix, DoctorItem *items, int *n_items) {
    char buf[BSIZE];

    if (!items) {
        printf("XAir Doctor — %s\n", s->ip);
        for (int i = 0; i < 40; i++) fputs("\xe2\x94\x80", stdout);
        printf("\n");
    }

    if (osc_handshake(&s->conn) != 0) {
        doc_report(s, items, n_items, 0, "XR18 not reachable at %s", s->ip);
        return 1;
    }
    doc_report(s, items, n_items, 1, "XR18 reachable  (ch %d, fx slot %d)",
               s->channel, s->fx_slot);

    int issues = 0;

    /* buses 1–6 */
    for (int n = 1; n <= 6; n++) {
        char path[32];
        snprintf(path, sizeof(path), "/bus/%d/eq/mode", n);
        int mode = -1;
        if (osc_query_int(&s->conn, path, &mode) != 0) {
            doc_report(s, items, n_items, 0, "Bus %d: no reply", n);
            issues++;
            continue;
        }
        const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
        if (mode == 1 || mode == 2) {
            doc_report(s, items, n_items, 1, "Bus %d: %s", n, name);
        } else {
            doc_report(s, items, n_items, 0, "Bus %d: %s  <- needs GEQ or TEQ", n, name);
            issues++;
            if (fix) {
                osc_send_int(&s->conn, path, 2);
                doc_report(s, items, n_items, -1, "  -> Bus %d set to TEQ", n);
            }
        }
    }

    /* LR */
    {
        int mode = -1;
        if (osc_query_int(&s->conn, "/lr/eq/mode", &mode) != 0) {
            doc_report(s, items, n_items, 0, "LR: no reply");
            issues++;
        } else {
            const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
            if (mode == 1 || mode == 2) {
                doc_report(s, items, n_items, 1, "LR: %s", name);
            } else {
                doc_report(s, items, n_items, 0, "LR: %s  <- needs GEQ or TEQ", name);
                issues++;
                if (fix) {
                    osc_send_int(&s->conn, "/lr/eq/mode", 2);
                    doc_report(s, items, n_items, -1, "  -> LR set to TEQ");
                }
            }
        }
    }

    /* FX slot must hold a GEQ/TEQ for reactive notches */
    {
        char node[160], q[24];
        snprintf(q, sizeof q, "fx/%d", s->fx_slot);
        if (osc_query_node(&s->conn, q, node, sizeof node) != 0) {
            doc_report(s, items, n_items, -1, "FX slot %d: no reply", s->fx_slot);
        } else if (strstr(node, "TEQ") || strstr(node, "GEQ")) {
            doc_report(s, items, n_items, 1, "FX slot %d: %.40s", s->fx_slot, node);
        } else {
            doc_report(s, items, n_items, 0,
                       "FX slot %d: %.40s  <- load GEQ/TEQ for reactive notches",
                       s->fx_slot, node);
            issues++;
        }
    }

    /* RTA streaming check */
    {
        subscribe_meters4(s);
        int   frames = 0;
        float peak   = -200.0f;
        for (int t = 0; t < 10; t++) {
            struct timeval tv2 = {0, 200000};
            fd_set f; FD_ZERO(&f); FD_SET(s->conn.fd, &f);
            if (select(s->conn.fd + 1, &f, NULL, NULL, &tv2) <= 0) continue;
            int r = recvfrom(s->conn.fd, buf, BSIZE, 0, 0, 0);
            if (r > 0 && strcmp(buf, "/meters/4") == 0) {
                int blen;
                const uint8_t *blob = osc_locate_blob(buf, r, &blen);
                if (blob && parse_meters4_blob(blob, blen, s->bins) == 0) {
                    frames++;
                    for (int i = 0; i < 100; i++) if (s->bins[i] > peak) peak = s->bins[i];
                }
            }
        }
        if (frames > 0)
            doc_report(s, items, n_items, 1,
                       "RTA /meters/4 streaming (%d frames, peak %.0f dBFS)", frames, peak);
        else {
            doc_report(s, items, n_items, 0,
                       "RTA /meters/4: no frames — check the RTA source channel/tap");
            issues++;
        }
    }

    /* mic-cal status (client side) */
    {
        int loaded = 0;
        for (int i = 0; i < 100; i++) if (s->mic_corr[i] != 0.0f) { loaded = 1; break; }
        doc_report(s, items, n_items, -1,
                   "mic-cal: %s", loaded ? "loaded" : "not configured (--mic-cal)");
    }

    /* summary */
    if (!items) printf("\n");
    if (issues == 0) {
        doc_report(s, items, n_items, 1, "All outputs OK.");
    } else if (fix) {
        doc_report(s, items, n_items, -1,
                   "%d output%s set to TEQ.", issues, issues == 1 ? "" : "s");
    } else {
        doc_report(s, items, n_items, 0,
                   "%d issue%s found. Run with --fix to set failing outputs to TEQ.",
                   issues, issues == 1 ? "" : "s");
    }
    return issues;
}

/* ── display: ncurses TUI via tui.{h,c} replaces old ANSI functions ──────── */

/* ── detection and notch control ─────────────────────────────────────────── */

static void cleanup_notches(AppState *s) {
    int n = 0;
    for (int j = 0; j < 31; j++) {
        if (s->toast.active[j]) {
            send_teq_band(s, j + 1, 0.5f);
            s->toast.active[j] = 0;
            n++;
        }
    }
    if (n > 0)
        fprintf(stderr, "\n[cleanup] restored %d band(s) to flat\n", n);
}

/* ── /meters/4 blob handler ──────────────────────────────────────────────── */

static void handle_meters4(const uint8_t *blob, int blen, AppState *s) {
    if (parse_meters4_blob(blob, blen, s->bins) != 0) return;
    for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];

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
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "[baseline] established: avg noise floor = %.1f dBFS", sum / 100.0f);
            log_push("%s", msg);
            if (s->verbose) printf("%s\n", msg);
        }
        if (!s->verbose) {
            TuiReactiveCtx ctx = make_reactive_ctx(s);
            tui_draw_reactive(&ctx, (const char * const *)g_log_ptrs,
                              g_log_count, g_log_scroll);
        }
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

    /* toast_step() dispatch loop */
    ToastAction acts[32];
    int nacts = toast_step(&s->toast, s->bins, s->baseline, acts, 32);
    for (int i = 0; i < nacts; i++) {
        ToastAction *a = &acts[i];
        int j = a->geq_par - 1;
        char msg[128];
        switch (a->op) {
        case TOAST_NOTCH:
            send_teq_band(s, a->geq_par, a->cut_val);
            s->notched_at[j] = now_sec();
            snprintf(msg, sizeof(msg),
                     "[notch] par=%02d (%s) cut=%.0fdB  level=%.1fdBFS",
                     a->geq_par, GEQ_LABEL[j], s->toast.cut_dB,
                     s->bins[TOAST_GEQ_BIN[j]]);
            log_push("%s", msg);
            if (s->verbose) printf("%s\n", msg);
            break;
        case TOAST_RELEASE_RAMP:
            send_teq_band(s, a->geq_par, a->ramp_val);
            break;
        case TOAST_RELEASE_DONE:
            send_teq_band(s, a->geq_par, 0.5f);
            snprintf(msg, sizeof(msg),
                     "[release] par=%02d (%s) after %.1fs",
                     a->geq_par, GEQ_LABEL[j], now_sec() - s->notched_at[j]);
            log_push("%s", msg);
            if (s->verbose) printf("%s\n", msg);
            break;
        default: break;
        }
    }
    if (!s->verbose) {
        TuiReactiveCtx ctx = make_reactive_ctx(s);
        tui_draw_reactive(&ctx, (const char * const *)g_log_ptrs,
                          g_log_count, g_log_scroll);
    }
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
        if (s->toast.active[j]) { p.band_db[j] = s->toast.cut_dB; count++; }

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
            s->toast.active[j]       = 1;
            s->toast.cut_val[j]      = v;
            s->toast.from_profile[j] = 1;
            s->notched_at[j]         = now_sec();
            applied++;
        }
    }
    fprintf(stderr, "[load] applied %d pinned profile notch%s from %s "
            "(made for bus %d, margin %.1fdB)\n",
            applied, applied == 1 ? "" : "es", s->ro_load_profile, p.bus, p.margin_db);
    return 0;
}

/* ── ring-out flow (T-023: supervised ramp/detect/notch control loop) ─────── */

/* Determine ring-out phase label from RingoutState. */
static const char *ro_phase_str(const RingoutState *st) {
    if (!st) return "INIT";
    if (st->done_reason != RO_REASON_NONE)
        return (st->done_reason == RO_REASON_ABORT) ? "ABORTED" : "DONE";
    if (st->pending_backoff) return "BACKING OFF";
    if (st->settle_count > 0) return "SETTLING";
    return "ANALYZING";
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
    if (osc_query_float(&s->conn, path, &s->ro_orig_fader) != 0) {
        fprintf(stderr, "error: no reply reading %s — aborting ring-out "
                "(refusing to drive a bus we can't restore)\n", path);
        return;
    }
    s->ro_orig_captured = 1;
    float start_db = fader_float_to_db(s->ro_orig_fader);

    /* 2) insert the TEQ FX slot on the target monitor bus so notches act there */
    snprintf(path, sizeof(path), "/bus/%d/insert/sel", s->ro_bus);
    osc_send_int(&s->conn, path, s->fx_slot);  /* 0=OFF, 1..4 = Fx1..Fx4 */
    snprintf(path, sizeof(path), "/bus/%d/insert/on", s->ro_bus);
    osc_send_int(&s->conn, path, 1);

    /* 3) RTA source + subscribe + keepalive */
    osc_send_int(&s->conn, "/-stat/rta/source", s->channel);
    osc_send_int_float(&s->conn, "/-prefs/rta", 0, 0.25f);
    subscribe_meters4(s);
    osc_send_no_args(&s->conn, "/xremote");

    /* 4) assert the known start gain (clamped to ceiling) */
    ringout_set_gain(s, start_db);

    log_push("[ring-out] bus %d  start %.1fdB  ceiling %.1fdB  step %.1fdB  "
             "margin %.1fdB  max-notch %d  fx slot %d",
             s->ro_bus, start_db, s->ro_ceiling_db,
             s->ro_step_db, s->ro_margin_db, s->ro_max_notches, s->fx_slot);
    log_push("[ring-out] RTA source = ch %d — ensure it taps bus %d monitor mic",
             s->channel, s->ro_bus);
    if (start_db > s->ro_ceiling_db)
        log_push("[ring-out] WARNING: start %.1fdB > ceiling %.1fdB; clamped",
                 start_db, s->ro_ceiling_db);

    if (s->verbose) {
        for (int i = 0; i < g_log_count; i++) printf("%s\n", g_log_ptrs[i]);
        fflush(stdout);
    } else {
        tui_init();
    }

    /* ── baseline calibration at start gain ── */
    double accum[100];
    for (int i = 0; i < 100; i++) accum[i] = 0.0;
    int    got = 0;
    double last_xremote = now_sec();
    char   r_buf[BSIZE];

    s->baseline_ready = 0;
    s->baseline_count = 0;

    while (g_running && got < BASELINE_FRAMES) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&s->conn, "/xremote"); subscribe_meters4(s); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(s->conn.fd, &fds);
        if (select(s->conn.fd + 1, &fds, NULL, NULL, &tv) > 0) {
            int r_len = recvfrom(s->conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
            if (r_len > 0 && strcmp(r_buf, "/meters/4") == 0) {
                int blen;
                const uint8_t *blob = osc_locate_blob(r_buf, r_len, &blen);
                if (blob && parse_meters4_blob(blob, blen, s->bins) == 0) {
                    for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];
                    for (int i = 0; i < 100; i++) accum[i] += s->bins[i];
                    got++;
                    s->baseline_count = got;
                }
            }
        }
        if (!s->verbose) {
            TuiRingoutCtx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.base    = make_reactive_ctx(s);
            ctx.ro_bus  = s->ro_bus;
            ctx.ceiling_db = s->ro_ceiling_db;
            ctx.start_db   = start_db;
            ctx.rst        = NULL;
            ctx.phase_str  = "CALIBRATING";
            tui_draw_ringout(&ctx, (const char * const *)g_log_ptrs,
                             g_log_count, g_log_scroll);
        }
        /* poll key during calibration */
        if (!s->verbose) {
            int tui_run = 1;
            tui_poll_key(&tui_run, NULL, &g_log_scroll);
            if (!tui_run) g_running = 0;
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }
    }
    if (!g_running) { ringout_panic(s); return; }
    for (int i = 0; i < 100; i++) s->baseline[i] = (float)(accum[i] / (got ? got : 1));
    s->baseline_ready = 1;

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
    cfg.settle_frames    = 10;
    cfg.stable_frames    = 10;
    cfg.narrow_skip      = 2;
    cfg.narrow_span      = 3;
    cfg.narrow_min_db    = NARROW_DB;
    RingoutState st;
    ringout_init(&st, &cfg);

    log_push("[ring-out] ramping toward ceiling %.1fdB (step %.1fdB)%s",
             s->ro_ceiling_db, s->ro_step_db, s->ro_supervised ? " — supervised" : "");

    /* ── control loop: one ringout_step() per RTA frame ── */
    int completed = 0;
    int ro_waiting    = 0;   /* supervised: waiting for Enter */
    float ro_next_gain = 0.0f;

    while (g_running) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&s->conn, "/xremote"); subscribe_meters4(s); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(s->conn.fd, &fds);
        if (select(s->conn.fd + 1, &fds, NULL, NULL, &tv) > 0) {
            int r_len = recvfrom(s->conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
            if (r_len > 0 && strcmp(r_buf, "/meters/4") == 0) {
                int blen2;
                const uint8_t *blob2 = osc_locate_blob(r_buf, r_len, &blen2);
                if (blob2 && parse_meters4_blob(blob2, blen2, s->bins) == 0) {
                    for (int i = 0; i < RTA_BIN_COUNT; i++) s->bins[i] += s->mic_corr[i];
                }
            }
        }

        /* run controller only when not waiting for supervised confirmation */
        RingoutAction a = {RO_HOLD, 0, 0, 0};
        if (!ro_waiting) {
            a = ringout_step(&st, s->bins, s->baseline);
            char msg[128];
            switch (a.op) {
            case RO_RAISE_GAIN:
                if (s->ro_supervised) {
                    ro_waiting   = 1;
                    ro_next_gain = a.gain_db;
                    log_push("[ring-out] waiting: raise to %.1fdB — press Enter", a.gain_db);
                } else {
                    ringout_set_gain(s, a.gain_db);
                    snprintf(msg, sizeof(msg),
                             "[ring-out] raise -> %.1fdB  (%.1fdB to ceiling, %d notch%s)",
                             a.gain_db, s->ro_ceiling_db - a.gain_db,
                             st.n_notches, st.n_notches == 1 ? "" : "es");
                    log_push("%s", msg);
                    if (s->verbose) printf("%s\n", msg);
                }
                break;
            case RO_BACK_OFF:
                ringout_set_gain(s, a.gain_db);
                snprintf(msg, sizeof(msg), "[ring-out] back off -> %.1fdB", a.gain_db);
                log_push("%s", msg);
                if (s->verbose) printf("%s\n", msg);
                break;
            case RO_PLACE_NOTCH: {
                int j = a.geq_par - 1;
                send_teq_band(s, a.geq_par, db_to_geq_float(a.cut_db));
                s->toast.active[j]         = 1;
                s->toast.cut_val[j]        = db_to_geq_float(a.cut_db);
                s->notched_at[j]           = now_sec();
                s->ro_notch_at_gain[j]     = st.cur_gain_db;
                snprintf(msg, sizeof(msg),
                         "[ring-out] notch par=%02d (%s) %.0fdB at %.1fdB",
                         a.geq_par, GEQ_LABEL[j], a.cut_db, st.cur_gain_db);
                log_push("%s", msg);
                if (s->verbose) printf("%s\n", msg);
                break;
            }
            case RO_DONE:  completed = 1; break;
            case RO_ABORT: completed = 0; break;
            case RO_HOLD: default: break;
            }
        }

        /* key input (TUI mode) */
        if (!s->verbose) {
            int tui_run = 1, ro_confirm = 0;
            tui_poll_key(&tui_run, ro_waiting ? &ro_confirm : NULL, &g_log_scroll);
            if (!tui_run) g_running = 0;
            if (ro_confirm) {
                ro_waiting = 0;
                ringout_set_gain(s, ro_next_gain);
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "[ring-out] raise -> %.1fdB  (%.1fdB to ceiling, %d notch%s) [confirmed]",
                         ro_next_gain, s->ro_ceiling_db - ro_next_gain,
                         st.n_notches, st.n_notches == 1 ? "" : "es");
                log_push("%s", msg);
            }
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }

        /* TUI draw */
        if (!s->verbose) {
            TuiRingoutCtx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.base          = make_reactive_ctx(s);
            ctx.ro_bus        = s->ro_bus;
            ctx.ceiling_db    = s->ro_ceiling_db;
            ctx.start_db      = start_db;
            ctx.rst           = &st;
            ctx.phase_str     = ro_phase_str(&st);
            ctx.supervised    = s->ro_supervised;
            ctx.waiting       = ro_waiting;
            ctx.next_gain_db  = ro_next_gain;
            ctx.notch_at_gain = s->ro_notch_at_gain;
            tui_draw_ringout(&ctx, (const char * const *)g_log_ptrs,
                             g_log_count, g_log_scroll);
        }

        if (a.op == RO_DONE || a.op == RO_ABORT) break;
    }

    /* ── summary + restore ── */
    char summary[200];
    if (!g_running && st.done_reason == RO_REASON_NONE)
        snprintf(summary, sizeof(summary), "[ring-out] interrupted by operator.");
    else if (st.done_reason == RO_REASON_ABORT)
        snprintf(summary, sizeof(summary), "[ring-out] ABORTED: %s",
                 ringout_reason_str(st.done_reason));
    else
        snprintf(summary, sizeof(summary), "[ring-out] complete: %s",
                 ringout_reason_str(st.done_reason));
    log_push("%s", summary);
    if (s->verbose) printf("%s\n", summary);

    ringout_panic(s);
    if (completed) {
        if (s->ro_save_profile[0]) ringout_save_profile(s, st.margin_db);
        log_push("[ring-out] notches LEFT in place (ring-out result).");
    } else {
        cleanup_notches(s);
        log_push("[ring-out] notches reverted (run did not complete).");
    }
    if (s->verbose) {
        printf("%s\n", g_log_ptrs[g_log_count - 2]);
        printf("%s\n", g_log_ptrs[g_log_count - 1]);
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

    signal(SIGINT,   sig_handler);
    signal(SIGTERM,  sig_handler);
    signal(SIGWINCH, sigwinch_handler);

    toast_state_init(&s.toast, s.threshold_dB, s.cut_dB, s.release_sec,
                     CONFIRM_FRAMES, NARROW_DB, 2, 3);

    /* ── create UDP socket ── */
    if (osc_conn_init(&s.conn, s.ip, PORT) < 0) {
        perror("socket");
        return 1;
    }

    /* ── doctor mode ── */
    if (s.doctor) {
        if (!s.verbose) tui_init();
        DoctorItem items[24]; int n_items = 0;
        run_doctor(&s, s.doctor_fix,
                   s.verbose ? NULL : items,
                   s.verbose ? NULL : &n_items);
        if (!s.verbose) {
            /* Hold the display until 'q' */
            int running = 1;
            while (running) {
                tui_poll_key(&running, NULL, NULL);
                if (g_resized) { tui_handle_resize(); g_resized = 0; }
                tui_draw_doctor(s.ip, items, n_items);
            }
        }
        osc_conn_close(&s.conn);
        return 0;
    }

    if (!s.verbose)
        printf("XAir Toast Saver  connecting to %s  ch:%d  slot:%d  "
               "threshold:%.0fdB  cut:%.0fdB  release:%.0fs\n",
               s.ip, s.channel, s.fx_slot, s.threshold_dB, s.cut_dB, s.release_sec);

    /* ── /xinfo handshake ── */
    if (osc_handshake(&s.conn) != 0) {
        fprintf(stderr, "error: no /xinfo reply from %s\n", s.ip);
        osc_conn_close(&s.conn);
        return 1;
    }
    if (s.verbose) printf("connected to %s\n", s.ip);

    /* ── auto pre-flight: never run against an output we can't notch ── */
    if (!s.no_preflight) {
        int n = run_doctor(&s, s.doctor_fix, NULL, NULL);  /* plain-text preflight */
        if (n > 0 && !s.doctor_fix) {
            fprintf(stderr, "\n*** pre-flight found %d issue(s) above. Set the outputs up, "
                            "re-run with --fix, or --no-preflight to override. ***\n", n);
            osc_conn_close(&s.conn);
            return 1;
        }
        printf("\n");
    }

    /* ── ring-out (proactive) takes its own flow ── */
    if (s.ringout) {
        run_ringout(&s);   /* owns all restore: fader always; notches per outcome */
        osc_conn_close(&s.conn);
        printf("toast saver (ring-out) stopped\n");
        return 0;
    }

    /* ── configure RTA source and subscribe ── */
    osc_send_int(&s.conn, "/-stat/rta/source", s.channel);
    osc_send_int_float(&s.conn, "/-prefs/rta", 0, 0.25f);  /* PEAK mode, decay 0.25 */
    subscribe_meters4(&s);
    osc_send_no_args(&s.conn, "/xremote");

    /* ── reactive handoff: pre-place a saved ring-out profile's notches ── */
    if (s.ro_load_profile[0] && ringout_load_profile(&s) != 0) {
        osc_conn_close(&s.conn);
        return 1;
    }

    if (!s.verbose) tui_init();

    /* ── main loop ── */
    double last_xremote = now_sec();
    char r_buf[BSIZE];

    while (g_running) {
        /* keepalive */
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&s.conn, "/xremote");
            subscribe_meters4(&s);
            last_xremote = t;
        }

        /* poll */
        struct timeval tv = {0, 60000};
        fd_set fds;
        FD_ZERO(&fds); FD_SET(s.conn.fd, &fds);
        if (select(s.conn.fd + 1, &fds, NULL, NULL, &tv) <= 0) {
            /* no data — still handle keys and resize */
            if (!s.verbose) {
                int tui_run = 1;
                tui_poll_key(&tui_run, NULL, &g_log_scroll);
                if (!tui_run) g_running = 0;
                if (g_resized) { tui_handle_resize(); g_resized = 0; }
            }
            continue;
        }

        int r_len = recvfrom(s.conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0) continue;
        if (strcmp(r_buf, "/meters/4") != 0) continue;

        int blen3;
        const uint8_t *blob3 = osc_locate_blob(r_buf, r_len, &blen3);
        if (!blob3) continue;

        /* key + resize before the heavy draw */
        if (!s.verbose) {
            int tui_run = 1;
            tui_poll_key(&tui_run, NULL, &g_log_scroll);
            if (!tui_run) g_running = 0;
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }

        handle_meters4(blob3, blen3, &s);
    }

    /* ── shutdown ── */
    cleanup_notches(&s);
    osc_conn_close(&s.conn);
    printf("toast saver stopped\n");
    return 0;
}
