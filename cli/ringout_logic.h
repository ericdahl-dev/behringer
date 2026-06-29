#ifndef RINGOUT_LOGIC_H
#define RINGOUT_LOGIC_H

/* Proactive ring-out controller — pure logic, no I/O.
 *
 * Drives a monitor toward (but never into) feedback: ramp gain up, find each
 * ring as it starts, notch it, repeat until a gain ceiling / target headroom /
 * max-notch limit. The caller executes the returned action (set a bus fader,
 * send a TEQ notch) and feeds back the next /meters/4 RTA frame.
 *
 * Detection reuses toast_logic (detect_peak / is_narrow_peak); this module adds
 * only the gain-drive state machine. No sockets, no time calls — the caller
 * advances one frame per ringout_step() call. See docs/ringout-design.md.
 */

/* What the caller should do with the mixer after this step. */
typedef enum {
    RO_HOLD,        /* do nothing this frame (settling / still confirming) */
    RO_RAISE_GAIN,  /* set the driven bus gain to action.gain_db */
    RO_PLACE_NOTCH, /* notch GEQ par action.geq_par by action.cut_db, then expect RO_BACK_OFF next */
    RO_BACK_OFF,    /* set the driven bus gain to action.gain_db (lowered after a notch) */
    RO_DONE,        /* run finished cleanly (see RingoutState.done_reason) */
    RO_ABORT        /* unsafe condition — restore gain to start immediately */
} RingoutOp;

typedef struct {
    RingoutOp op;
    float     gain_db;  /* RAISE_GAIN / BACK_OFF / ABORT: the gain to command */
    int       geq_par;  /* PLACE_NOTCH: GEQ parameter 1..31 */
    float     cut_db;   /* PLACE_NOTCH: notch depth (negative dB) */
} RingoutAction;

/* Why a run ended (valid once op == RO_DONE or RO_ABORT). */
typedef enum {
    RO_REASON_NONE = 0,
    RO_REASON_CEILING,      /* next raise would exceed the gain ceiling */
    RO_REASON_MARGIN,       /* target headroom over start gain achieved */
    RO_REASON_MAX_NOTCHES,  /* notch budget exhausted */
    RO_REASON_ABORT         /* an already-notched band rang again (runaway) */
} RingoutReason;

typedef struct {
    float start_gain_db;     /* gain the bus starts at (and the abort/restore floor) */
    float ceiling_db;        /* hard cap — never command gain above this */
    float step_db;           /* per-step raise increment; also the back-off amount */
    float target_margin_db;  /* stop once (gain - start) >= this; <= 0 disables */
    int   max_notches;       /* stop after this many notches placed */
    float threshold_db;      /* detect_peak threshold (dB over baseline) */
    float cut_db;            /* notch depth applied per ring (negative) */
    int   confirm_frames;    /* consecutive frames a ring must persist to confirm */
    int   settle_frames;     /* frames to wait after any gain change / notch */
    int   stable_frames;     /* consecutive ring-free frames required before raising */
    int   narrow_skip;       /* is_narrow_peak: skip */
    int   narrow_span;       /* is_narrow_peak: span */
    float narrow_min_db;     /* is_narrow_peak: min advantage over neighbours */
} RingoutConfig;

typedef struct {
    RingoutConfig cfg;

    int   phase;             /* internal: settle / analyze / done */
    int   settle_count;      /* frames left to settle */
    int   stable_count;      /* consecutive ring-free frames seen */
    int   confirm_count;     /* consecutive frames the current ring has held */
    int   confirm_bin;       /* RTA bin currently being confirmed (-1 = none) */
    int   pending_backoff;   /* a notch was just placed; next step backs off */

    float cur_gain_db;       /* gain currently commanded on the bus */
    int   n_notches;         /* notches placed so far */
    char  placed[32];        /* placed[par] = 1 if GEQ par 1..31 already notched */

    int   done_reason;       /* RingoutReason, set on DONE/ABORT */
    float margin_db;         /* gain-before-feedback margin achieved (cur - start) */
} RingoutState;

/* Initialise state from config. cfg is copied; sane fields are required (e.g.
 * step_db > 0, ceiling_db >= start_gain_db). The bus is assumed to already be at
 * cfg.start_gain_db when the loop begins. */
void ringout_init(RingoutState *st, const RingoutConfig *cfg);

/* Advance one RTA frame. bins[100] = current dBFS spectrum, baseline[100] = the
 * caller's smoothed noise floor. Returns the action to apply. */
RingoutAction ringout_step(RingoutState *st, const float *bins, const float *baseline);

/* Human-readable reason string for logging/summaries. */
const char *ringout_reason_str(int reason);

#endif /* RINGOUT_LOGIC_H */
