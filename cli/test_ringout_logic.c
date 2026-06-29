#include <stdio.h>
#include <math.h>
#include <string.h>
#include "ringout_logic.h"
#include "toast_logic.h"

static int passed = 0, failed = 0;

static void ok(const char *name) { passed++; printf("PASS  %s\n", name); }
static void fail(const char *name, const char *reason) {
    failed++; printf("FAIL  %s: %s\n", name, reason);
}

#define ASSERT_INT_EQ(name, got, want) do { \
    int _g = (got), _w = (want); \
    if (_g == _w) ok(name); \
    else { char _b[80]; snprintf(_b, sizeof(_b), "got %d want %d", _g, _w); fail(name, _b); } \
} while (0)

#define ASSERT_TRUE(name, cond) do { \
    if (cond) ok(name); else fail(name, "condition false"); \
} while (0)

#define ASSERT_FLOAT_EQ(name, got, want) do { \
    float _g = (got), _w = (want); \
    if (fabsf(_g - _w) < 0.01f) ok(name); \
    else { char _b[80]; snprintf(_b, sizeof(_b), "got %.3f want %.3f", _g, _w); fail(name, _b); } \
} while (0)

/* ── synthetic "feedback world" ──────────────────────────────────────────────
 * Feedback points ring (a narrow 25 dB spike at their bin) once the commanded
 * gain reaches their onset, unless their band has been notched.            */

#define RING_LEVEL 25.0f

typedef struct {
    int   n_fb;
    int   fb_bin[8];
    float fb_onset[8];
    int   suppress_on_notch;   /* 1 = a notch silences the band; 0 = notch "fails" */
} World;

typedef struct {
    int   n_notch;
    int   notch_pars[16];
    int   final_op;            /* RO_DONE / RO_ABORT */
    int   reason;
    float margin_db;
    float max_gain;            /* highest gain ever commanded */
    int   iters;
} Result;

static void build_frame(float bins[100], float baseline[100],
                        const World *w, float gain, const char suppressed_par[32]) {
    for (int i = 0; i < 100; i++) { bins[i] = 0.0f; baseline[i] = 0.0f; }
    for (int f = 0; f < w->n_fb; f++) {
        int par = bin_to_geq_par(w->fb_bin[f]);
        if (gain >= w->fb_onset[f] && !suppressed_par[par])
            bins[w->fb_bin[f]] = RING_LEVEL;
    }
}

/* Drive the controller against the world until it ends or a safety cap is hit. */
static void run(RingoutState *st, const World *w, Result *res) {
    float bins[100], baseline[100];
    char  suppressed[32] = {0};
    float world_gain = st->cfg.start_gain_db;

    memset(res, 0, sizeof(*res));
    res->max_gain = world_gain;

    for (res->iters = 0; res->iters < 5000; res->iters++) {
        build_frame(bins, baseline, w, world_gain, suppressed);
        RingoutAction a = ringout_step(st, bins, baseline);

        if (st->cur_gain_db > res->max_gain) res->max_gain = st->cur_gain_db;

        if (a.op == RO_RAISE_GAIN || a.op == RO_BACK_OFF) {
            world_gain = a.gain_db;
        } else if (a.op == RO_PLACE_NOTCH) {
            res->notch_pars[res->n_notch++] = a.geq_par;
            if (w->suppress_on_notch) suppressed[a.geq_par] = 1;
        } else if (a.op == RO_DONE || a.op == RO_ABORT) {
            res->final_op   = a.op;
            res->reason     = st->done_reason;
            res->margin_db  = st->margin_db;
            return;
        }
    }
}

static RingoutConfig base_cfg(void) {
    RingoutConfig c;
    c.start_gain_db    = 0.0f;
    c.ceiling_db       = 12.0f;
    c.step_db          = 3.0f;
    c.target_margin_db = 0.0f;   /* disabled */
    c.max_notches      = 10;
    c.threshold_db     = 12.0f;
    c.cut_db           = -6.0f;
    c.confirm_frames   = 2;
    c.settle_frames    = 2;
    c.stable_frames    = 2;
    c.narrow_skip      = 2;
    c.narrow_span      = 3;
    c.narrow_min_db    = 10.0f;
    return c;
}

int main(void) {
    /* ── init ────────────────────────────────────────────────────────────── */
    {
        RingoutConfig c = base_cfg();
        c.start_gain_db = -3.0f;
        RingoutState st;
        ringout_init(&st, &c);
        ASSERT_FLOAT_EQ("init: cur_gain = start_gain", st.cur_gain_db, -3.0f);
        ASSERT_INT_EQ("init: zero notches", st.n_notches, 0);
        ASSERT_INT_EQ("init: reason NONE", st.done_reason, RO_REASON_NONE);
    }

    /* ── full closed loop: two rings notched, then ceiling ───────────────── */
    {
        RingoutConfig c = base_cfg();
        RingoutState st;
        ringout_init(&st, &c);

        World w = { .n_fb = 2,
                    .fb_bin   = {56, 23},
                    .fb_onset = {6.0f, 9.0f},
                    .suppress_on_notch = 1 };
        Result r;
        run(&st, &w, &r);

        ASSERT_INT_EQ("closed loop: placed 2 notches", r.n_notch, 2);
        ASSERT_INT_EQ("closed loop: 1st notch is par 18 (1 kHz / bin 56)",
                      r.n_notch > 0 ? r.notch_pars[0] : -1, 18);
        ASSERT_INT_EQ("closed loop: 2nd notch is par 8 (100 Hz / bin 23)",
                      r.n_notch > 1 ? r.notch_pars[1] : -1, 8);
        ASSERT_INT_EQ("closed loop: ends DONE", r.final_op, RO_DONE);
        ASSERT_INT_EQ("closed loop: reason CEILING", r.reason, RO_REASON_CEILING);
        ASSERT_FLOAT_EQ("closed loop: margin = ceiling-start = 12", r.margin_db, 12.0f);
        ASSERT_TRUE("closed loop: gain never exceeded ceiling",
                    r.max_gain <= c.ceiling_db + 0.001f);
    }

    /* ── no feedback: ramp straight to ceiling ───────────────────────────── */
    {
        RingoutConfig c = base_cfg();
        RingoutState st;
        ringout_init(&st, &c);

        World w = { .n_fb = 0, .suppress_on_notch = 1 };
        Result r;
        run(&st, &w, &r);

        ASSERT_INT_EQ("no-fb: zero notches", r.n_notch, 0);
        ASSERT_INT_EQ("no-fb: reason CEILING", r.reason, RO_REASON_CEILING);
        ASSERT_FLOAT_EQ("no-fb: margin 12", r.margin_db, 12.0f);
        ASSERT_TRUE("no-fb: never exceeded ceiling", r.max_gain <= c.ceiling_db + 0.001f);
    }

    /* ── max-notch budget stops the run ──────────────────────────────────── */
    {
        RingoutConfig c = base_cfg();
        c.max_notches = 1;
        RingoutState st;
        ringout_init(&st, &c);

        World w = { .n_fb = 2,
                    .fb_bin   = {56, 23},
                    .fb_onset = {6.0f, 9.0f},
                    .suppress_on_notch = 1 };
        Result r;
        run(&st, &w, &r);

        ASSERT_INT_EQ("max-notch: exactly 1 notch placed", r.n_notch, 1);
        ASSERT_INT_EQ("max-notch: reason MAX_NOTCHES", r.reason, RO_REASON_MAX_NOTCHES);
    }

    /* ── target margin stops the run before ceiling ──────────────────────── */
    {
        RingoutConfig c = base_cfg();
        c.ceiling_db       = 30.0f;
        c.target_margin_db = 6.0f;
        RingoutState st;
        ringout_init(&st, &c);

        World w = { .n_fb = 0, .suppress_on_notch = 1 };
        Result r;
        run(&st, &w, &r);

        ASSERT_INT_EQ("margin: reason MARGIN", r.reason, RO_REASON_MARGIN);
        ASSERT_FLOAT_EQ("margin: stops at +6 dB", r.margin_db, 6.0f);
    }

    /* ── abort: a notched band rings again (notch "fails") ───────────────── */
    {
        RingoutConfig c = base_cfg();
        RingoutState st;
        ringout_init(&st, &c);

        World w = { .n_fb = 1,
                    .fb_bin   = {56},
                    .fb_onset = {6.0f},
                    .suppress_on_notch = 0 };   /* notch does NOT silence it */
        Result r;
        run(&st, &w, &r);

        ASSERT_INT_EQ("abort: ends ABORT", r.final_op, RO_ABORT);
        ASSERT_INT_EQ("abort: reason ABORT", r.reason, RO_REASON_ABORT);
        ASSERT_INT_EQ("abort: notched the band exactly once (no double-notch)", r.n_notch, 1);
        ASSERT_TRUE("abort: never exceeded ceiling", r.max_gain <= c.ceiling_db + 0.001f);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
