#include "ringout_logic.h"
#include "toast_logic.h"

/* Internal phases. */
enum { RO_PHASE_ANALYZE = 0, RO_PHASE_SETTLE, RO_PHASE_DONE };

#define RTA_BINS 100

void ringout_init(RingoutState *st, const RingoutConfig *cfg) {
    st->cfg = *cfg;

    st->phase           = RO_PHASE_ANALYZE;
    st->settle_count    = 0;
    st->stable_count    = 0;
    st->confirm_count   = 0;
    st->confirm_bin     = -1;
    st->pending_backoff = 0;

    st->cur_gain_db = cfg->start_gain_db;
    st->n_notches   = 0;
    for (int i = 0; i < 32; i++) st->placed[i] = 0;

    st->done_reason = RO_REASON_NONE;
    st->margin_db   = 0.0f;
}

/* Record a terminal outcome and emit the matching action. */
static RingoutAction finish(RingoutState *st, RingoutReason reason, RingoutOp op,
                            float gain_db) {
    RingoutAction a;
    st->phase       = RO_PHASE_DONE;
    st->done_reason = reason;
    st->margin_db   = st->cur_gain_db - st->cfg.start_gain_db;
    a.op = op; a.gain_db = gain_db; a.geq_par = 0; a.cut_db = 0.0f;
    return a;
}

RingoutAction ringout_step(RingoutState *st, const float *bins, const float *baseline) {
    RingoutAction a = { RO_HOLD, st->cur_gain_db, 0, 0.0f };

    if (st->phase == RO_PHASE_DONE) {
        a.op = RO_DONE;
        return a;
    }

    /* A notch placed last step takes priority: back the gain off and re-settle
     * before analysing again, so the notch can take effect. */
    if (st->pending_backoff) {
        st->pending_backoff = 0;
        st->cur_gain_db -= st->cfg.step_db;
        if (st->cur_gain_db < st->cfg.start_gain_db)
            st->cur_gain_db = st->cfg.start_gain_db;
        st->phase        = RO_PHASE_SETTLE;
        st->settle_count = st->cfg.settle_frames;
        a.op = RO_BACK_OFF;
        a.gain_db = st->cur_gain_db;
        return a;
    }

    /* Settle: ignore frame content until the system has stabilised. */
    if (st->phase == RO_PHASE_SETTLE) {
        if (st->settle_count > 0) {
            st->settle_count--;
            return a; /* RO_HOLD */
        }
        st->phase         = RO_PHASE_ANALYZE;
        st->confirm_count = 0;
        st->confirm_bin   = -1;
        st->stable_count  = 0;
        /* fall through and analyse this frame */
    }

    /* Analyse: is there a narrow ring? */
    int peak;
    int found  = detect_peak(bins, baseline, st->cfg.threshold_db, &peak);
    int narrow = found && is_narrow_peak(bins, RTA_BINS, peak,
                     st->cfg.narrow_skip, st->cfg.narrow_span, st->cfg.narrow_min_db);

    if (narrow) {
        int par = bin_to_geq_par(peak);

        /* A band we already notched is ringing again after settling → the notch
         * is not holding it; bail rather than chase a runaway. */
        if (st->placed[par])
            return finish(st, RO_REASON_ABORT, RO_ABORT, st->cfg.start_gain_db);

        if (peak == st->confirm_bin) {
            st->confirm_count++;
        } else {
            st->confirm_bin   = peak;
            st->confirm_count = 1;
        }
        st->stable_count = 0;

        if (st->confirm_count >= st->cfg.confirm_frames) {
            st->placed[par] = 1;
            st->n_notches++;
            st->confirm_count   = 0;
            st->confirm_bin     = -1;
            st->pending_backoff = 1;          /* next step backs the gain off */
            a.op      = RO_PLACE_NOTCH;
            a.geq_par = par;
            a.cut_db  = st->cfg.cut_db;
            a.gain_db = st->cur_gain_db;
            return a;
        }
        return a; /* RO_HOLD — still confirming */
    }

    /* No qualifying ring this frame. */
    st->confirm_count = 0;
    st->confirm_bin   = -1;
    st->stable_count++;

    if (st->stable_count >= st->cfg.stable_frames) {
        st->stable_count = 0;

        /* Stop conditions, checked before committing to another raise. */
        if (st->n_notches >= st->cfg.max_notches)
            return finish(st, RO_REASON_MAX_NOTCHES, RO_DONE, st->cur_gain_db);

        if (st->cfg.target_margin_db > 0.0f &&
            (st->cur_gain_db - st->cfg.start_gain_db) >= st->cfg.target_margin_db)
            return finish(st, RO_REASON_MARGIN, RO_DONE, st->cur_gain_db);

        if (st->cur_gain_db + st->cfg.step_db > st->cfg.ceiling_db)
            return finish(st, RO_REASON_CEILING, RO_DONE, st->cur_gain_db);

        /* Safe to raise. */
        st->cur_gain_db += st->cfg.step_db;
        st->phase        = RO_PHASE_SETTLE;
        st->settle_count = st->cfg.settle_frames;
        a.op      = RO_RAISE_GAIN;
        a.gain_db = st->cur_gain_db;
        return a;
    }

    return a; /* RO_HOLD — not yet stable enough to raise */
}

const char *ringout_reason_str(int reason) {
    switch (reason) {
        case RO_REASON_CEILING:     return "gain ceiling reached";
        case RO_REASON_MARGIN:      return "target margin achieved";
        case RO_REASON_MAX_NOTCHES: return "max notches placed";
        case RO_REASON_ABORT:       return "aborted (notched band rang again)";
        default:                    return "none";
    }
}
