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
#include "log.h"
#include <stdarg.h>

/* ── constants ──────────────────────────────────────────────────────────── */

#define BSIZE            512
#define XREMOTE_TIMEOUT  9

/* Forward declarations — needed so check_outputs_fn can reference these types
 * before their full definitions appear below MixerModel. */
typedef struct AppConfig_s  AppConfig;
typedef struct AppRuntime_s AppRuntime;

typedef struct MixerModel_s MixerModel;

/* Model-specific bus/output check for doctor mode. NULL = skip. Returns issue count. */
typedef int (*check_outputs_fn)(const AppConfig *cfg, AppRuntime *rt, int fix,
                                DoctorItem *items, int *n_items);

struct MixerModel_s {
    const char        *name;
    int                port;
    const char        *meter_path;
    int                max_channel;
    int                max_bus;
    int                max_fx_slot;
    check_outputs_fn   check_outputs;
};
#define XREMOTE_PERIOD   9      /* send /xremote every 9 seconds */
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

/* Immutable after main() finishes argument parsing. */
typedef struct AppConfig_s {
    const MixerModel *model;
    char   ip[20];
    int    channel;
    int    fx_slot;
    float  threshold_dB;
    float  cut_dB;
    float  release_sec;
    int    verbose;
    int    no_preflight;
    int    doctor;
    int    doctor_fix;
    int    ringout;
    int    ro_bus;
    float  ro_ceiling_db;
    float  ro_step_db;
    float  ro_margin_db;
    int    ro_max_notches;
    int    ro_supervised;
    char   ro_save_profile[256];
    char   ro_load_profile[256];
} AppConfig;

/* Mutable state that changes during a run. */
typedef struct AppRuntime_s {
    OscConn conn;
    float   bins[100];
    float   baseline[100];
    float   baseline_accum[100];
    int     baseline_count;
    int     baseline_ready;
    float   mic_corr[100];
    ToastState toast;
    double  notched_at[31];
    float   ro_notch_at_gain[31];
    float   ro_orig_fader;
    int     ro_orig_captured;
    float   ro_cur_gain_db;
} AppRuntime;

/* ── model constants ─────────────────────────────────────────────────────── */

/* Forward-declared above; doc_report is also forward-referenced here — defined below. */
static void doc_report(const AppConfig *cfg, DoctorItem *items, int *n_items,
                       int pass, const char *fmt, ...);

static int xr18_check_outputs(const AppConfig *cfg, AppRuntime *rt, int fix,
                               DoctorItem *items, int *n_items) {
    int issues = 0;
    static const char *EQ_MODE_NAME[] = {"PEQ", "GEQ", "TEQ"};
    for (int n = 1; n <= 6; n++) {
        char path[32];
        snprintf(path, sizeof(path), "/bus/%d/eq/mode", n);
        int mode = -1;
        if (osc_query_int(&rt->conn, path, &mode) != 0) {
            doc_report(cfg, items, n_items, 0, "Bus %d: no reply", n);
            issues++;
            continue;
        }
        const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
        if (mode == 1 || mode == 2) {
            doc_report(cfg, items, n_items, 1, "Bus %d: %s", n, name);
        } else {
            doc_report(cfg, items, n_items, 0, "Bus %d: %s  <- needs GEQ or TEQ", n, name);
            issues++;
            if (fix) {
                osc_send_int(&rt->conn, path, 2);
                doc_report(cfg, items, n_items, -1, "  -> Bus %d set to TEQ", n);
            }
        }
    }
    {
        int mode = -1;
        if (osc_query_int(&rt->conn, "/lr/eq/mode", &mode) != 0) {
            doc_report(cfg, items, n_items, 0, "LR: no reply");
            issues++;
        } else {
            const char *name = (mode >= 0 && mode <= 2) ? EQ_MODE_NAME[mode] : "?";
            if (mode == 1 || mode == 2) {
                doc_report(cfg, items, n_items, 1, "LR: %s", name);
            } else {
                doc_report(cfg, items, n_items, 0, "LR: %s  <- needs GEQ or TEQ", name);
                issues++;
                if (fix) {
                    osc_send_int(&rt->conn, "/lr/eq/mode", 2);
                    doc_report(cfg, items, n_items, -1, "  -> LR set to TEQ");
                }
            }
        }
    }
    return issues;
}

static const MixerModel MODEL_XR18 = {"XR18", 10024, "/meters/4",  18,  6, 4, xr18_check_outputs};
static const MixerModel MODEL_X32  = {"X32",  10023, "/meters/15", 32, 16, 8, NULL};

/* ── globals ─────────────────────────────────────────────────────────────── */

static volatile int g_running = 1;
static volatile int g_resized = 0;

static void sig_handler(int sig)    { (void)sig; g_running = 0; }
static void sigwinch_handler(int s) { (void)s;   g_resized = 1; }

/* ── log ring buffer ─────────────────────────────────────────────────────── */

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* ── TUI context helpers ─────────────────────────────────────────────────── */

static TuiReactiveCtx make_reactive_ctx(const AppConfig *cfg, const AppRuntime *rt) {
    TuiReactiveCtx c;
    memset(&c, 0, sizeof(c));
    c.ip             = cfg->ip;
    c.channel        = cfg->channel;
    c.fx_slot        = cfg->fx_slot;
    c.threshold_dB   = cfg->threshold_dB;
    c.cut_dB         = cfg->cut_dB;
    c.release_sec    = cfg->release_sec;
    c.bins           = rt->bins;
    c.baseline       = rt->baseline;
    c.baseline_ready = rt->baseline_ready;
    c.baseline_count = rt->baseline_count;
    c.baseline_frames = BASELINE_FRAMES;
    c.toast          = &rt->toast;
    c.notched_at     = rt->notched_at;
    c.geq_labels     = GEQ_LABEL;
    c.now            = now_sec();
    return c;
}

/* Doctor item helper: if items != NULL fills the array and redraws; else printf. */
static void doc_report(const AppConfig *cfg, DoctorItem *items, int *n_items,
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
        tui_draw_doctor(cfg->ip, items, *n_items);
    } else {
        printf("[%s] %s\n",
               pass == 1 ? "PASS" : (pass == 0 ? "FAIL" : "INFO"), msg);
        fflush(stdout);
    }
}

/* ── OSC send helpers ────────────────────────────────────────────────────── */

static void send_teq_band(const AppConfig *cfg, AppRuntime *rt, int par, float val) {
    char path[32];
    snprintf(path, sizeof(path), "/fx/%d/par/%02d", cfg->fx_slot, par);
    osc_send_float(&rt->conn, path, val);
}

/* ── ring-out gain drive / safety (T-022) ────────────────────────────────── */


/* Command the target monitor bus to a gain in dB, clamped to the ceiling.
 * The ceiling is also enforced upstream by the controller (T-021); this is the
 * belt-and-suspenders guard at the wire. */
static void ringout_set_gain(const AppConfig *cfg, AppRuntime *rt, float dB) {
    if (dB > cfg->ro_ceiling_db) dB = cfg->ro_ceiling_db;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", cfg->ro_bus);
    osc_send_float(&rt->conn, path, fader_db_to_float(dB));
    rt->ro_cur_gain_db = dB;
}

/* Immediately restore the bus fader to its captured original value. Safe to
 * call any time after capture; bound to the SIGINT/SIGTERM shutdown path. */
static void ringout_panic(const AppConfig *cfg, AppRuntime *rt) {
    if (!rt->ro_orig_captured) return;
    char path[40];
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", cfg->ro_bus);
    osc_send_float(&rt->conn, path, rt->ro_orig_fader);
    fprintf(stderr, "[ring-out] restored bus %d fader to %.3f\n",
            cfg->ro_bus, rt->ro_orig_fader);
}

static void subscribe_meters(const AppConfig *cfg, AppRuntime *rt) {
    osc_subscribe_meters(&rt->conn, cfg->model->meter_path);
}

/* ── doctor mode ─────────────────────────────────────────────────────────── */

/* items/n_items: non-NULL → TUI mode (stream results); NULL → plain printf. */
static int run_doctor(const AppConfig *cfg, AppRuntime *rt, int fix,
                      DoctorItem *items, int *n_items) {
    char buf[BSIZE];

    if (!items) {
        printf("XAir Doctor — %s\n", cfg->ip);
        for (int i = 0; i < 40; i++) fputs("\xe2\x94\x80", stdout);
        printf("\n");
    }

    if (osc_handshake(&rt->conn) != 0) {
        doc_report(cfg, items, n_items, 0, "%s not reachable at %s", cfg->model->name, cfg->ip);
        return 1;
    }
    doc_report(cfg, items, n_items, 1, "%s reachable  (ch %d, fx slot %d)",
               cfg->model->name, cfg->channel, cfg->fx_slot);

    int issues = 0;

    if (cfg->model->check_outputs) {
        issues += cfg->model->check_outputs(cfg, rt, fix, items, n_items);
    } else {
        doc_report(cfg, items, n_items, -1,
                   "%s: bus GEQ/TEQ is in FX rack slot — ensure bus output is routed through it",
                   cfg->model->name);
    }

    /* FX slot must hold a GEQ/TEQ for reactive notches */
    {
        char node[160], q[24];
        snprintf(q, sizeof q, "fx/%d", cfg->fx_slot);
        if (osc_query_node(&rt->conn, q, node, sizeof node) != 0) {
            doc_report(cfg, items, n_items, -1, "FX slot %d: no reply", cfg->fx_slot);
        } else if (strstr(node, "TEQ") || strstr(node, "GEQ")) {
            doc_report(cfg, items, n_items, 1, "FX slot %d: %.40s", cfg->fx_slot, node);
        } else {
            doc_report(cfg, items, n_items, 0,
                       "FX slot %d: %.40s  <- load GEQ/TEQ for reactive notches",
                       cfg->fx_slot, node);
            issues++;
        }
    }

    /* RTA streaming check */
    {
        subscribe_meters(cfg, rt);
        int   frames = 0;
        float peak   = -200.0f;
        for (int t = 0; t < 10; t++) {
            struct timeval tv2 = {0, 200000};
            fd_set f; FD_ZERO(&f); FD_SET(rt->conn.fd, &f);
            if (select(rt->conn.fd + 1, &f, NULL, NULL, &tv2) <= 0) continue;
            int r = recvfrom(rt->conn.fd, buf, BSIZE, 0, 0, 0);
            if (r > 0 && strcmp(buf, cfg->model->meter_path) == 0) {
                int blen;
                const uint8_t *blob = osc_locate_blob(buf, r, &blen);
                if (blob && parse_meters4_blob(blob, blen, rt->bins) == 0) {
                    frames++;
                    for (int i = 0; i < 100; i++) if (rt->bins[i] > peak) peak = rt->bins[i];
                }
            }
        }
        if (frames > 0)
            doc_report(cfg, items, n_items, 1,
                       "RTA %s streaming (%d frames, peak %.0f dBFS)",
                       cfg->model->meter_path, frames, peak);
        else {
            doc_report(cfg, items, n_items, 0,
                       "RTA %s: no frames — check the RTA source channel/tap",
                       cfg->model->meter_path);
            issues++;
        }
    }

    /* mic-cal status (client side) */
    {
        int loaded = 0;
        for (int i = 0; i < 100; i++) if (rt->mic_corr[i] != 0.0f) { loaded = 1; break; }
        doc_report(cfg, items, n_items, -1,
                   "mic-cal: %s", loaded ? "loaded" : "not configured (--mic-cal)");
    }

    /* summary */
    if (!items) printf("\n");
    if (issues == 0) {
        doc_report(cfg, items, n_items, 1, "All outputs OK.");
    } else if (fix) {
        doc_report(cfg, items, n_items, -1,
                   "%d output%s set to TEQ.", issues, issues == 1 ? "" : "s");
    } else {
        doc_report(cfg, items, n_items, 0,
                   "%d issue%s found. Run with --fix to set failing outputs to TEQ.",
                   issues, issues == 1 ? "" : "s");
    }
    return issues;
}

/* ── display: ncurses TUI via tui.{h,c} replaces old ANSI functions ──────── */

/* ── detection and notch control ─────────────────────────────────────────── */

static void cleanup_notches(const AppConfig *cfg, AppRuntime *rt) {
    int n = 0;
    for (int j = 0; j < 31; j++) {
        if (rt->toast.active[j]) {
            send_teq_band(cfg, rt, j + 1, 0.5f);
            rt->toast.active[j] = 0;
            n++;
        }
    }
    if (n > 0)
        fprintf(stderr, "\n[cleanup] restored %d band(s) to flat\n", n);
}

/* ── meters blob handler ─────────────────────────────────────────────────── */

static void handle_calibration_frame(const AppConfig *cfg, AppRuntime *rt) {
    for (int i = 0; i < 100; i++)
        rt->baseline_accum[i] += rt->bins[i];
    rt->baseline_count++;
    if (rt->baseline_count >= BASELINE_FRAMES) {
        for (int i = 0; i < 100; i++)
            rt->baseline[i] = rt->baseline_accum[i] / BASELINE_FRAMES;
        rt->baseline_ready = 1;
        float sum = 0.0f;
        for (int i = 0; i < 100; i++) sum += rt->baseline[i];
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "[baseline] established: avg noise floor = %.1f dBFS", sum / 100.0f);
        log_push("%s", msg);
        if (cfg->verbose) printf("%s\n", msg);
    }
    if (!cfg->verbose) {
        TuiReactiveCtx ctx = make_reactive_ctx(cfg, rt);
        tui_draw_reactive(&ctx, log_lines(), log_count(), log_scroll());
    }
}

static void handle_detection_frame(const AppConfig *cfg, AppRuntime *rt) {
    update_baseline(rt->baseline, rt->bins, 100, 0.005f);
    if (cfg->verbose) {
        int peak = 0;
        for (int i = 1; i < 100; i++)
            if (rt->bins[i] > rt->bins[peak]) peak = i;
        printf("peak bin=%d  level=%.1f dBFS  baseline=%.1f dBFS  excess=%.1f dB\n",
               peak, rt->bins[peak], rt->baseline[peak],
               rt->bins[peak] - rt->baseline[peak]);
    }
    ToastAction acts[32];
    int nacts = toast_step(&rt->toast, rt->bins, rt->baseline, acts, 32);
    for (int i = 0; i < nacts; i++) {
        ToastAction *a = &acts[i];
        int j = a->geq_par - 1;
        char msg[128];
        switch (a->op) {
        case TOAST_NOTCH:
            send_teq_band(cfg, rt, a->geq_par, a->cut_val);
            rt->notched_at[j] = now_sec();
            snprintf(msg, sizeof(msg),
                     "[notch] par=%02d (%s) cut=%.0fdB  level=%.1fdBFS",
                     a->geq_par, GEQ_LABEL[j], rt->toast.cut_dB,
                     rt->bins[TOAST_GEQ_BIN[j]]);
            log_push("%s", msg);
            if (cfg->verbose) printf("%s\n", msg);
            break;
        case TOAST_RELEASE_RAMP:
            send_teq_band(cfg, rt, a->geq_par, a->ramp_val);
            break;
        case TOAST_RELEASE_DONE:
            send_teq_band(cfg, rt, a->geq_par, 0.5f);
            snprintf(msg, sizeof(msg),
                     "[release] par=%02d (%s) after %.1fs",
                     a->geq_par, GEQ_LABEL[j], now_sec() - rt->notched_at[j]);
            log_push("%s", msg);
            if (cfg->verbose) printf("%s\n", msg);
            break;
        default: break;
        }
    }
    if (!cfg->verbose) {
        TuiReactiveCtx ctx = make_reactive_ctx(cfg, rt);
        tui_draw_reactive(&ctx, log_lines(), log_count(), log_scroll());
    }
}

static void handle_meters(const uint8_t *blob, int blen,
                          const AppConfig *cfg, AppRuntime *rt) {
    if (parse_meters4_blob(blob, blen, rt->bins) != 0) return;
    for (int i = 0; i < RTA_BIN_COUNT; i++) rt->bins[i] += rt->mic_corr[i];
    if (!rt->baseline_ready)
        handle_calibration_frame(cfg, rt);
    else
        handle_detection_frame(cfg, rt);
}

/* ── ring-out profile save / load (T-024) ────────────────────────────────── */

/* Write the completed ring-out result (notches + margin + target) to a file. */
static void ringout_save_profile(const AppConfig *cfg, AppRuntime *rt, float margin_db) {
    RingoutProfile p;
    memset(&p, 0, sizeof(p));
    strncpy(p.model, cfg->model->name, sizeof(p.model) - 1);
    p.bus = cfg->ro_bus;
    p.fx_slot = cfg->fx_slot;
    p.margin_db = margin_db;
    time_t tnow = time(NULL);
    strftime(p.created, sizeof(p.created), "%Y-%m-%dT%H:%M:%S", localtime(&tnow));
    int count = 0;
    for (int j = 0; j < 31; j++)
        if (rt->toast.active[j]) { p.band_db[j] = rt->toast.cut_dB; count++; }

    char buf[2048];
    int n = ringout_profile_serialize(&p, buf, sizeof(buf));
    if (n < 0) { fprintf(stderr, "[ring-out] profile serialize failed\n"); return; }
    FILE *f = fopen(cfg->ro_save_profile, "w");
    if (!f) { fprintf(stderr, "[ring-out] cannot write %s\n", cfg->ro_save_profile); return; }
    fwrite(buf, 1, n, f);
    fclose(f);
    printf("[ring-out] saved profile to %s (%d notch%s, margin %.1fdB)\n",
           cfg->ro_save_profile, count, count == 1 ? "" : "es", margin_db);
}

/* Load a profile and pre-place its static notches for reactive protection.
 * Returns 0 on success, -1 on read/parse/mismatch error. */
static int ringout_load_profile(const AppConfig *cfg, AppRuntime *rt) {
    FILE *f = fopen(cfg->ro_load_profile, "r");
    if (!f) { fprintf(stderr, "error: cannot read profile %s\n", cfg->ro_load_profile); return -1; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) { fprintf(stderr, "error: empty profile %s\n", cfg->ro_load_profile); return -1; }
    buf[n] = 0;

    RingoutProfile p;
    if (ringout_profile_parse(buf, &p) != 0) {
        fprintf(stderr, "error: malformed profile %s\n", cfg->ro_load_profile); return -1;
    }
    /* mismatch guard: notch bands target a TEQ in a specific model + FX slot. */
    if (strcmp(p.model, cfg->model->name) != 0) {
        fprintf(stderr, "error: profile model '%s' != %s\n", p.model, cfg->model->name); return -1;
    }
    if (p.fx_slot != cfg->fx_slot) {
        fprintf(stderr, "error: profile fx_slot %d != -s %d\n", p.fx_slot, cfg->fx_slot); return -1;
    }

    int applied = 0;
    for (int j = 0; j < 31; j++) {
        if (p.band_db[j] < 0.0f) {
            float v = db_to_geq_float(p.band_db[j]);
            send_teq_band(cfg, rt, j + 1, v);
            rt->toast.active[j]       = 1;
            rt->toast.cut_val[j]      = v;
            rt->toast.from_profile[j] = 1;
            rt->notched_at[j]         = now_sec();
            applied++;
        }
    }
    fprintf(stderr, "[load] applied %d pinned profile notch%s from %s "
            "(made for bus %d, margin %.1fdB)\n",
            applied, applied == 1 ? "" : "es", cfg->ro_load_profile, p.bus, p.margin_db);
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
static void run_ringout(const AppConfig *cfg, AppRuntime *rt) {
    char path[40];

    /* 1) capture original fader BEFORE touching anything — refuse to drive a
     *    bus we cannot restore. */
    snprintf(path, sizeof(path), "/bus/%d/mix/fader", cfg->ro_bus);
    if (osc_query_float(&rt->conn, path, &rt->ro_orig_fader) != 0) {
        fprintf(stderr, "error: no reply reading %s — aborting ring-out "
                "(refusing to drive a bus we can't restore)\n", path);
        return;
    }
    rt->ro_orig_captured = 1;
    float start_db = fader_float_to_db(rt->ro_orig_fader);

    /* 2) insert the TEQ FX slot on the target monitor bus so notches act there.
     * XR18 only: X32 uses FX rack routing, which is a one-time user setup. */
    if (cfg->model == &MODEL_XR18) {
        snprintf(path, sizeof(path), "/bus/%d/insert/sel", cfg->ro_bus);
        osc_send_int(&rt->conn, path, cfg->fx_slot);  /* 0=OFF, 1..4 = Fx1..Fx4 */
        snprintf(path, sizeof(path), "/bus/%d/insert/on", cfg->ro_bus);
        osc_send_int(&rt->conn, path, 1);
    }

    /* 3) RTA source + subscribe + keepalive */
    osc_send_int(&rt->conn, "/-stat/rta/source", cfg->channel);
    osc_send_int_float(&rt->conn, "/-prefs/rta", 0, 0.25f);
    subscribe_meters(cfg, rt);
    osc_send_no_args(&rt->conn, "/xremote");

    /* 4) assert the known start gain (clamped to ceiling) */
    ringout_set_gain(cfg, rt, start_db);

    log_push("[ring-out] bus %d  start %.1fdB  ceiling %.1fdB  step %.1fdB  "
             "margin %.1fdB  max-notch %d  fx slot %d",
             cfg->ro_bus, start_db, cfg->ro_ceiling_db,
             cfg->ro_step_db, cfg->ro_margin_db, cfg->ro_max_notches, cfg->fx_slot);
    log_push("[ring-out] RTA source = ch %d — ensure it taps bus %d monitor mic",
             cfg->channel, cfg->ro_bus);
    if (start_db > cfg->ro_ceiling_db)
        log_push("[ring-out] WARNING: start %.1fdB > ceiling %.1fdB; clamped",
                 start_db, cfg->ro_ceiling_db);

    if (cfg->verbose) {
        for (int i = 0; i < log_count(); i++) printf("%s\n", log_lines()[i]);
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

    rt->baseline_ready = 0;
    rt->baseline_count = 0;

    while (g_running && got < BASELINE_FRAMES) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&rt->conn, "/xremote"); subscribe_meters(cfg, rt); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(rt->conn.fd, &fds);
        if (select(rt->conn.fd + 1, &fds, NULL, NULL, &tv) > 0) {
            int r_len = recvfrom(rt->conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
            if (r_len > 0 && strcmp(r_buf, cfg->model->meter_path) == 0) {
                int blen;
                const uint8_t *blob = osc_locate_blob(r_buf, r_len, &blen);
                if (blob && parse_meters4_blob(blob, blen, rt->bins) == 0) {
                    for (int i = 0; i < RTA_BIN_COUNT; i++) rt->bins[i] += rt->mic_corr[i];
                    for (int i = 0; i < 100; i++) accum[i] += rt->bins[i];
                    got++;
                    rt->baseline_count = got;
                }
            }
        }
        if (!cfg->verbose) {
            TuiRingoutCtx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.base    = make_reactive_ctx(cfg, rt);
            ctx.ro_bus  = cfg->ro_bus;
            ctx.ceiling_db = cfg->ro_ceiling_db;
            ctx.start_db   = start_db;
            ctx.rst        = NULL;
            ctx.phase_str  = "CALIBRATING";
            tui_draw_ringout(&ctx, log_lines(),
                             log_count(), log_scroll());
        }
        /* poll key during calibration */
        if (!cfg->verbose) {
            int tui_run = 1;
            tui_poll_key(&tui_run, NULL, log_scroll_ptr());
            if (!tui_run) g_running = 0;
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }
    }
    if (!g_running) { ringout_panic(cfg, rt); return; }
    for (int i = 0; i < 100; i++) rt->baseline[i] = (float)(accum[i] / (got ? got : 1));
    rt->baseline_ready = 1;

    /* ── configure the controller from the CLI args ── */
    RingoutConfig rocfg;
    rocfg.start_gain_db    = start_db;
    rocfg.ceiling_db       = cfg->ro_ceiling_db;
    rocfg.step_db          = cfg->ro_step_db;
    rocfg.target_margin_db = cfg->ro_margin_db;
    rocfg.max_notches      = cfg->ro_max_notches;
    rocfg.threshold_db     = cfg->threshold_dB;
    rocfg.cut_db           = cfg->cut_dB;
    rocfg.confirm_frames   = CONFIRM_FRAMES;
    rocfg.settle_frames    = 10;
    rocfg.stable_frames    = 10;
    rocfg.narrow_skip      = 2;
    rocfg.narrow_span      = 3;
    rocfg.narrow_min_db    = NARROW_DB;
    RingoutState st;
    ringout_init(&st, &rocfg);

    log_push("[ring-out] ramping toward ceiling %.1fdB (step %.1fdB)%s",
             cfg->ro_ceiling_db, cfg->ro_step_db, cfg->ro_supervised ? " — supervised" : "");

    /* ── control loop: one ringout_step() per RTA frame ── */
    int completed = 0;
    int ro_waiting    = 0;   /* supervised: waiting for Enter */
    float ro_next_gain = 0.0f;

    while (g_running) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&rt->conn, "/xremote"); subscribe_meters(cfg, rt); last_xremote = t;
        }
        struct timeval tv = {0, 60000};
        fd_set fds; FD_ZERO(&fds); FD_SET(rt->conn.fd, &fds);
        if (select(rt->conn.fd + 1, &fds, NULL, NULL, &tv) > 0) {
            int r_len = recvfrom(rt->conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
            if (r_len > 0 && strcmp(r_buf, cfg->model->meter_path) == 0) {
                int blen2;
                const uint8_t *blob2 = osc_locate_blob(r_buf, r_len, &blen2);
                if (blob2 && parse_meters4_blob(blob2, blen2, rt->bins) == 0) {
                    for (int i = 0; i < RTA_BIN_COUNT; i++) rt->bins[i] += rt->mic_corr[i];
                }
            }
        }

        /* run controller only when not waiting for supervised confirmation */
        RingoutAction a = {RO_HOLD, 0, 0, 0};
        if (!ro_waiting) {
            a = ringout_step(&st, rt->bins, rt->baseline);
            char msg[128];
            switch (a.op) {
            case RO_RAISE_GAIN:
                if (cfg->ro_supervised) {
                    ro_waiting   = 1;
                    ro_next_gain = a.gain_db;
                    log_push("[ring-out] waiting: raise to %.1fdB — press Enter", a.gain_db);
                } else {
                    ringout_set_gain(cfg, rt, a.gain_db);
                    snprintf(msg, sizeof(msg),
                             "[ring-out] raise -> %.1fdB  (%.1fdB to ceiling, %d notch%s)",
                             a.gain_db, cfg->ro_ceiling_db - a.gain_db,
                             st.n_notches, st.n_notches == 1 ? "" : "es");
                    log_push("%s", msg);
                    if (cfg->verbose) printf("%s\n", msg);
                }
                break;
            case RO_BACK_OFF:
                ringout_set_gain(cfg, rt, a.gain_db);
                snprintf(msg, sizeof(msg), "[ring-out] back off -> %.1fdB", a.gain_db);
                log_push("%s", msg);
                if (cfg->verbose) printf("%s\n", msg);
                break;
            case RO_PLACE_NOTCH: {
                int j = a.geq_par - 1;
                send_teq_band(cfg, rt, a.geq_par, db_to_geq_float(a.cut_db));
                rt->toast.active[j]         = 1;
                rt->toast.cut_val[j]        = db_to_geq_float(a.cut_db);
                rt->notched_at[j]           = now_sec();
                rt->ro_notch_at_gain[j]     = st.cur_gain_db;
                snprintf(msg, sizeof(msg),
                         "[ring-out] notch par=%02d (%s) %.0fdB at %.1fdB",
                         a.geq_par, GEQ_LABEL[j], a.cut_db, st.cur_gain_db);
                log_push("%s", msg);
                if (cfg->verbose) printf("%s\n", msg);
                break;
            }
            case RO_DONE:  completed = 1; break;
            case RO_ABORT: completed = 0; break;
            case RO_HOLD: default: break;
            }
        }

        /* key input (TUI mode) */
        if (!cfg->verbose) {
            int tui_run = 1, ro_confirm = 0;
            tui_poll_key(&tui_run, ro_waiting ? &ro_confirm : NULL, log_scroll_ptr());
            if (!tui_run) g_running = 0;
            if (ro_confirm) {
                ro_waiting = 0;
                ringout_set_gain(cfg, rt, ro_next_gain);
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "[ring-out] raise -> %.1fdB  (%.1fdB to ceiling, %d notch%s) [confirmed]",
                         ro_next_gain, cfg->ro_ceiling_db - ro_next_gain,
                         st.n_notches, st.n_notches == 1 ? "" : "es");
                log_push("%s", msg);
            }
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }

        /* TUI draw */
        if (!cfg->verbose) {
            TuiRingoutCtx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.base          = make_reactive_ctx(cfg, rt);
            ctx.ro_bus        = cfg->ro_bus;
            ctx.ceiling_db    = cfg->ro_ceiling_db;
            ctx.start_db      = start_db;
            ctx.rst           = &st;
            ctx.phase_str     = ro_phase_str(&st);
            ctx.supervised    = cfg->ro_supervised;
            ctx.waiting       = ro_waiting;
            ctx.next_gain_db  = ro_next_gain;
            ctx.notch_at_gain = rt->ro_notch_at_gain;
            tui_draw_ringout(&ctx, log_lines(),
                             log_count(), log_scroll());
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
    if (cfg->verbose) printf("%s\n", summary);

    ringout_panic(cfg, rt);
    if (completed) {
        if (cfg->ro_save_profile[0]) ringout_save_profile(cfg, rt, st.margin_db);
        log_push("[ring-out] notches LEFT in place (ring-out result).");
    } else {
        cleanup_notches(cfg, rt);
        log_push("[ring-out] notches reverted (run did not complete).");
    }
    if (cfg->verbose) {
        printf("%s\n", log_lines()[log_count() - 2]);
        printf("%s\n", log_lines()[log_count() - 1]);
    }
}

/* Load a mic calibration file and bake it into per-RTA-bin dB corrections. */
static void load_mic_cal(AppRuntime *rt, const char *path) {
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
    mic_cal_bin_corrections(&cal, RTA_BIN_FREQ, rt->mic_corr, RTA_BIN_COUNT);
    fprintf(stderr, "mic-cal: %d points from %s applied to RTA bins\n", cal.n, path);
}

/* ── reactive mode ───────────────────────────────────────────────────────── */

static void run_reactive(const AppConfig *cfg, AppRuntime *rt) {
    if (!cfg->verbose)
        printf("XAir Toast Saver  connecting to %s  ch:%d  slot:%d  "
               "threshold:%.0fdB  cut:%.0fdB  release:%.0fs\n",
               cfg->ip, cfg->channel, cfg->fx_slot,
               cfg->threshold_dB, cfg->cut_dB, cfg->release_sec);

    /* /xinfo handshake */
    if (osc_handshake(&rt->conn) != 0) {
        fprintf(stderr, "error: no /xinfo reply from %s\n", cfg->ip);
        osc_conn_close(&rt->conn);
        return;
    }
    if (cfg->verbose) printf("connected to %s\n", cfg->ip);

    /* auto pre-flight: never run against an output we can't notch */
    if (!cfg->no_preflight) {
        int n = run_doctor(cfg, rt, cfg->doctor_fix, NULL, NULL);
        if (n > 0 && !cfg->doctor_fix) {
            fprintf(stderr, "\n*** pre-flight found %d issue(s) above. Set the outputs up, "
                            "re-run with --fix, or --no-preflight to override. ***\n", n);
            osc_conn_close(&rt->conn);
            return;
        }
        printf("\n");
    }

    /* configure RTA source and subscribe */
    osc_send_int(&rt->conn, "/-stat/rta/source", cfg->channel);
    osc_send_int_float(&rt->conn, "/-prefs/rta", 0, 0.25f);
    subscribe_meters(cfg, rt);
    osc_send_no_args(&rt->conn, "/xremote");

    /* reactive handoff: pre-place a saved ring-out profile's notches */
    if (cfg->ro_load_profile[0] && ringout_load_profile(cfg, rt) != 0) {
        osc_conn_close(&rt->conn);
        return;
    }

    if (!cfg->verbose) tui_init();

    double last_xremote = now_sec();
    char r_buf[BSIZE];

    while (g_running) {
        double t = now_sec();
        if (t - last_xremote >= XREMOTE_TIMEOUT) {
            osc_send_no_args(&rt->conn, "/xremote");
            subscribe_meters(cfg, rt);
            last_xremote = t;
        }

        struct timeval tv = {0, 60000};
        fd_set fds;
        FD_ZERO(&fds); FD_SET(rt->conn.fd, &fds);
        if (select(rt->conn.fd + 1, &fds, NULL, NULL, &tv) <= 0) {
            if (!cfg->verbose) {
                int tui_run = 1;
                tui_poll_key(&tui_run, NULL, log_scroll_ptr());
                if (!tui_run) g_running = 0;
                if (g_resized) { tui_handle_resize(); g_resized = 0; }
            }
            continue;
        }

        int r_len = recvfrom(rt->conn.fd, r_buf, BSIZE - 1, 0, 0, 0);
        if (r_len <= 0) continue;
        if (strcmp(r_buf, cfg->model->meter_path) != 0) continue;

        int blen;
        const uint8_t *blob = osc_locate_blob(r_buf, r_len, &blen);
        if (!blob) continue;

        if (!cfg->verbose) {
            int tui_run = 1;
            tui_poll_key(&tui_run, NULL, log_scroll_ptr());
            if (!tui_run) g_running = 0;
            if (g_resized) { tui_handle_resize(); g_resized = 0; }
        }

        handle_meters(blob, blen, cfg, rt);
    }

    cleanup_notches(cfg, rt);
    osc_conn_close(&rt->conn);
    printf("toast saver stopped\n");
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    AppConfig cfg;
    AppRuntime rt;
    memset(&cfg, 0, sizeof(cfg));
    memset(&rt, 0, sizeof(rt));

    /* defaults */
    strcpy(cfg.ip, "192.168.0.64");
    cfg.channel      = 1;
    cfg.fx_slot      = 4;
    cfg.threshold_dB = 20.0f;
    cfg.cut_dB       = -6.0f;
    cfg.release_sec  = 10.0f;

    cfg.model = &MODEL_XR18;

    /* ring-out defaults */
    cfg.ro_bus         = 1;
    cfg.ro_ceiling_db  = 0.0f;
    cfg.ro_step_db     = 1.0f;
    cfg.ro_margin_db   = 6.0f;
    cfg.ro_max_notches = 8;

    enum { OPT_RINGOUT = 1000, OPT_RO_BUS, OPT_RO_CEIL,
           OPT_RO_STEP, OPT_RO_MARGIN, OPT_RO_MAXN, OPT_RO_SUP,
           OPT_MIC_CAL, OPT_SAVE_PROFILE, OPT_LOAD_PROFILE,
           OPT_DOCTOR, OPT_FIX, OPT_NO_PREFLIGHT, OPT_MODEL };
    static struct option long_opts[] = {
        {"model",              required_argument, 0, OPT_MODEL},
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
        case 'i': strncpy(cfg.ip, optarg, sizeof(cfg.ip) - 1); ip_set = 1; break;
        case 'c': cfg.channel      = atoi(optarg); break;
        case 's': cfg.fx_slot      = atoi(optarg); break;
        case 't': cfg.threshold_dB = atof(optarg); break;
        case 'd': cfg.cut_dB       = atof(optarg); break;
        case 'r': cfg.release_sec  = atof(optarg); break;
        case 'v': cfg.verbose      = 1;            break;
        case OPT_MIC_CAL:   load_mic_cal(&rt, optarg);         break;
        case OPT_RINGOUT:   cfg.ringout        = 1;            break;
        case OPT_RO_BUS:    cfg.ro_bus         = atoi(optarg); break;
        case OPT_RO_CEIL:   cfg.ro_ceiling_db  = atof(optarg); break;
        case OPT_RO_STEP:   cfg.ro_step_db     = atof(optarg); break;
        case OPT_RO_MARGIN: cfg.ro_margin_db   = atof(optarg); break;
        case OPT_RO_MAXN:   cfg.ro_max_notches = atoi(optarg); break;
        case OPT_RO_SUP:    cfg.ro_supervised  = 1;            break;
        case OPT_SAVE_PROFILE: strncpy(cfg.ro_save_profile, optarg, sizeof(cfg.ro_save_profile)-1); break;
        case OPT_LOAD_PROFILE: strncpy(cfg.ro_load_profile, optarg, sizeof(cfg.ro_load_profile)-1); break;
        case OPT_DOCTOR: cfg.doctor     = 1; break;
        case OPT_FIX:    cfg.doctor_fix = 1; break;
        case OPT_NO_PREFLIGHT: cfg.no_preflight = 1; break;
        case OPT_MODEL:
            if (strcmp(optarg, "x32") == 0 || strcmp(optarg, "X32") == 0)
                cfg.model = &MODEL_X32;
            else if (strcmp(optarg, "xr18") == 0 || strcmp(optarg, "XR18") == 0)
                cfg.model = &MODEL_XR18;
            else { fprintf(stderr, "error: unknown model '%s' (xr18 or x32)\n", optarg); return 1; }
            break;
        default:
        case 'h':
            fprintf(stderr,
                "usage: XAir_ToastSaver -i <ip> [--model {xr18,x32}] [-c <ch>] [-s <slot>]\n"
                "                       [-t <threshold_dB>] [-d <cut_dB>]\n"
                "                       [-r <release_sec>] [-v]\n"
                "       XAir_ToastSaver -i <ip> --ringout [--ringout-bus N]\n"
                "                       [--ringout-ceiling dB] [--ringout-step dB]\n"
                "                       [--ringout-margin dB] [--ringout-max-notches N]\n"
                "       XAir_ToastSaver -i <ip> --doctor [--fix]\n"
                "  -i  Mixer IP address (required)\n"
                "  --model {xr18,x32}  Mixer model (default: xr18)\n"
                "  -c  RTA source channel (default: 1; xr18: 1-18, x32: 1-32)\n"
                "  -s  FX slot with TEQ (default: 4; xr18: 1-4, x32: 1-8)\n"
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
    if (cfg.channel < 1 || cfg.channel > cfg.model->max_channel) {
        fprintf(stderr, "error: channel must be 1-%d for %s\n",
                cfg.model->max_channel, cfg.model->name); return 1;
    }
    if (cfg.fx_slot < 1 || cfg.fx_slot > cfg.model->max_fx_slot) {
        fprintf(stderr, "error: fx slot must be 1-%d for %s\n",
                cfg.model->max_fx_slot, cfg.model->name); return 1;
    }
    if (cfg.cut_dB > 0.0f) {
        fprintf(stderr, "warning: cut_dB should be negative; got %.1f\n", cfg.cut_dB);
    }
    if (cfg.ringout) {
        if (cfg.ro_bus < 1 || cfg.ro_bus > cfg.model->max_bus) {
            fprintf(stderr, "error: --ringout-bus must be 1-%d for %s\n",
                    cfg.model->max_bus, cfg.model->name); return 1;
        }
        if (cfg.ro_step_db <= 0.0f) {
            fprintf(stderr, "error: --ringout-step must be > 0\n"); return 1;
        }
        if (cfg.ro_max_notches < 1) {
            fprintf(stderr, "error: --ringout-max-notches must be >= 1\n"); return 1;
        }
        if (cfg.ro_load_profile[0])
            fprintf(stderr, "warning: --load-profile is ignored in ring-out mode\n");
    } else if (cfg.ro_save_profile[0]) {
        fprintf(stderr, "warning: --save-profile only applies to --ringout mode\n");
    }

    signal(SIGINT,   sig_handler);
    signal(SIGTERM,  sig_handler);
    signal(SIGWINCH, sigwinch_handler);

    toast_state_init(&rt.toast, cfg.threshold_dB, cfg.cut_dB, cfg.release_sec,
                     CONFIRM_FRAMES, NARROW_DB, 2, 3);

    /* ── create UDP socket ── */
    if (osc_conn_init(&rt.conn, cfg.ip, cfg.model->port) < 0) {
        perror("socket");
        return 1;
    }

    /* ── doctor mode ── */
    if (cfg.doctor) {
        if (!cfg.verbose) tui_init();
        DoctorItem items[24]; int n_items = 0;
        run_doctor(&cfg, &rt, cfg.doctor_fix,
                   cfg.verbose ? NULL : items,
                   cfg.verbose ? NULL : &n_items);
        if (!cfg.verbose) {
            /* Hold the display until 'q' */
            int running = 1;
            while (running) {
                tui_poll_key(&running, NULL, NULL);
                if (g_resized) { tui_handle_resize(); g_resized = 0; }
                tui_draw_doctor(cfg.ip, items, n_items);
            }
        }
        osc_conn_close(&rt.conn);
        return 0;
    }

    /* ── ring-out (proactive) takes its own flow ── */
    if (cfg.ringout) {
        run_ringout(&cfg, &rt);
        osc_conn_close(&rt.conn);
        printf("toast saver (ring-out) stopped\n");
        return 0;
    }

    run_reactive(&cfg, &rt);
    return 0;
}
