#ifndef TUI_H
#define TUI_H

#include "toast_logic.h"
#include "ringout_logic.h"

/* ── doctor items ────────────────────────────────────────────────────────── */

typedef struct {
    int  pass;       /* 1=PASS, 0=FAIL, -1=INFO */
    char text[256];
} DoctorItem;

/* ── draw contexts ───────────────────────────────────────────────────────── */

typedef struct {
    const char *ip;
    int    channel, fx_slot;
    float  threshold_dB, cut_dB, release_sec;
    const float          *bins, *baseline;
    int    baseline_ready, baseline_count, baseline_frames;
    const ToastState     *toast;
    const double         *notched_at;    /* [31] wall-clock stamps */
    const char * const   *geq_labels;   /* [31] */
    double now;
} TuiReactiveCtx;

typedef struct {
    TuiReactiveCtx  base;
    int    ro_bus;
    float  ceiling_db, start_db;
    const RingoutState *rst;
    const char         *phase_str;  /* "SETTLING", "ANALYZING", "DONE", etc. */
    int    supervised;
    int    waiting;           /* 1 = supervisor waiting for Enter */
    float  next_gain_db;
    const float *notch_at_gain; /* [31] gain_db when placed (0 = not placed) */
} TuiRingoutCtx;

/* ── lifecycle ───────────────────────────────────────────────────────────── */

void tui_init(void);
void tui_shutdown(void);
void tui_handle_resize(void);

/* ── draw calls ──────────────────────────────────────────────────────────── */

void tui_draw_doctor(const char *ip, const DoctorItem *items, int n);

void tui_draw_reactive(const TuiReactiveCtx *ctx,
                       const char * const *log, int log_n, int log_scroll);

void tui_draw_ringout(const TuiRingoutCtx *ctx,
                      const char * const *log, int log_n, int log_scroll);

/* ── input ───────────────────────────────────────────────────────────────── */

/* Poll one key. Updates *running, *ro_confirmed, *log_scroll per bindings.
 * Returns the key code, or ERR if no key was pressed. */
int tui_poll_key(int *running, int *ro_confirmed, int *log_scroll);

/* ── testable pure helpers (no ncurses) ──────────────────────────────────── */

/* Map RTA bin [0,99] to terminal column [0, cols-1]. */
int tui_bin_to_col(int bin, int cols);

/* Map terminal column to nearest RTA bin. */
int tui_col_to_bin(int col, int cols);

/* Return gradient level 0-4 for excess dB above baseline:
 *   0: < 5 dB     (space)
 *   1: 5–10 dB    (░)
 *   2: 10–15 dB   (▒)
 *   3: 15–thr dB  (▓)
 *   4: ≥ threshold (█ yellow) */
int tui_excess_level(float excess, float threshold);

#endif /* TUI_H */
