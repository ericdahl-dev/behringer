#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "toast_logic.h"

static int passed = 0, failed = 0;

static void ok(const char *name) {
    passed++;
    printf("PASS  %s\n", name);
}

static void fail(const char *name, const char *reason) {
    failed++;
    printf("FAIL  %s: %s\n", name, reason);
}

#define ASSERT_INT_EQ(name, got, want) do { \
    int _g = (got), _w = (want); \
    if (_g == _w) ok(name); \
    else { char _b[64]; snprintf(_b, sizeof(_b), "got %d want %d", _g, _w); fail(name, _b); } \
} while (0)

#define ASSERT_FLOAT_EQ(name, got, want) do { \
    float _g = (got), _w = (want); \
    if (fabsf(_g - _w) < 0.001f) ok(name); \
    else { char _b[64]; snprintf(_b, sizeof(_b), "got %.4f want %.4f", _g, _w); fail(name, _b); } \
} while (0)

int main(void) {

    /* ── db_to_geq_float ─────────────────────────────────────────────────── */

    ASSERT_FLOAT_EQ("db_to_geq_float:  0 dB = 0.500", db_to_geq_float(0.0f),   0.5f);
    ASSERT_FLOAT_EQ("db_to_geq_float: -15 dB = 0.000", db_to_geq_float(-15.0f), 0.0f);
    ASSERT_FLOAT_EQ("db_to_geq_float: +15 dB = 1.000", db_to_geq_float(15.0f),  1.0f);
    ASSERT_FLOAT_EQ("db_to_geq_float:  -6 dB = 0.300", db_to_geq_float(-6.0f),  0.3f);
    ASSERT_FLOAT_EQ("db_to_geq_float:  -9 dB = 0.200", db_to_geq_float(-9.0f),  0.2f);

    /* ── bin_to_geq_par ──────────────────────────────────────────────────── */

    ASSERT_INT_EQ("TOAST_GEQ_BIN[17] == 56 (1kHz calibration point)", TOAST_GEQ_BIN[17], 56);

    ASSERT_INT_EQ("bin_to_geq_par: bin 56 = par 18 (1 kHz)",  bin_to_geq_par(56), 18);
    ASSERT_INT_EQ("bin_to_geq_par: bin  0 = par  1 (20 Hz)",  bin_to_geq_par(0),   1);
    ASSERT_INT_EQ("bin_to_geq_par: bin 99 = par 31 (20 kHz)", bin_to_geq_par(99), 31);
    ASSERT_INT_EQ("bin_to_geq_par: bin 23 = par  8 (100 Hz)", bin_to_geq_par(23),  8);
    /* bin 50: between 630Hz[49] and 800Hz[53] — closer to 630Hz, so par 16 */
    ASSERT_INT_EQ("bin_to_geq_par: bin 50 = par 16 (630 Hz, nearest)", bin_to_geq_par(50), 16);

    /* ── parse_meters4_blob ──────────────────────────────────────────────── */

    {
        /* Build a synthetic /meters/4 blob:
         *   bytes [0-3]  big-endian uint32 = 208 (total byte count)
         *   bytes [4-7]  little-endian uint32 = 100 (sample count)
         *   bytes [8-207] 100 × little-endian int16 samples (all zero by default) */
        uint8_t blob[208];
        memset(blob, 0, sizeof(blob));
        blob[0] = 0; blob[1] = 0; blob[2] = 0; blob[3] = 208;  /* BE size */
        blob[4] = 100; blob[5] = 0; blob[6] = 0; blob[7] = 0;  /* LE count */

        /* Set bin 56 to -20 dBFS: int16 = -20 * 256 = -5120 = 0xEB00 */
        int16_t val = (int16_t)(-20 * 256);
        memcpy(blob + 8 + 56 * 2, &val, 2);

        float bins[100];

        ASSERT_INT_EQ("parse_meters4_blob: returns 0 for valid blob",
            parse_meters4_blob(blob, sizeof(blob), bins), 0);
        ASSERT_FLOAT_EQ("parse_meters4_blob: bin 56 = -20.0 dBFS",
            bins[56], -20.0f);
        ASSERT_FLOAT_EQ("parse_meters4_blob: bin 0 = 0.0 dBFS (zero sample)",
            bins[0], 0.0f);

        /* Too short */
        ASSERT_INT_EQ("parse_meters4_blob: returns -1 for blob shorter than 8 bytes",
            parse_meters4_blob(blob, 7, bins), -1);

        /* Wrong sample count: set n_vals = 44 (mimics /meters/5) */
        uint8_t bad[208];
        memcpy(bad, blob, sizeof(bad));
        bad[4] = 44; bad[5] = 0; bad[6] = 0; bad[7] = 0;
        ASSERT_INT_EQ("parse_meters4_blob: returns -1 when n_vals != 100",
            parse_meters4_blob(bad, sizeof(bad), bins), -1);
    }

    /* ── detect_peak ─────────────────────────────────────────────────────── */

    {
        float bins[100], baseline[100];
        int peak_bin;

        memset(bins,     0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));

        /* No spike: all bins at baseline */
        ASSERT_INT_EQ("detect_peak: returns 0 when no bin exceeds threshold",
            detect_peak(bins, baseline, 20.0f, &peak_bin), 0);

        /* Single spike: bin 56 = 25 dB above baseline */
        bins[56] = 25.0f;
        ASSERT_INT_EQ("detect_peak: returns 1 when bin 56 is 25 dB above baseline",
            detect_peak(bins, baseline, 20.0f, &peak_bin), 1);
        ASSERT_INT_EQ("detect_peak: peak_bin is 56 for single spike",
            peak_bin, 56);

        /* Two spikes: bin 23 +25 dB, bin 56 +30 dB — higher level wins */
        bins[23] = 25.0f;
        bins[56] = 30.0f;
        ASSERT_INT_EQ("detect_peak: picks higher-level bin (56 over 23)",
            (detect_peak(bins, baseline, 20.0f, &peak_bin), peak_bin), 56);

        /* Exactly at threshold: must NOT trigger (strict greater-than) */
        memset(bins, 0, sizeof(bins));
        bins[0] = 20.0f;  /* excess = 20.0 - 0.0 = exactly threshold */
        ASSERT_INT_EQ("detect_peak: returns 0 when excess == threshold (strict >)",
            detect_peak(bins, baseline, 20.0f, &peak_bin), 0);
    }

    /* ── update_baseline ─────────────────────────────────────────────────── */

    {
        float baseline[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float bins[4]     = {100.0f, 100.0f, 100.0f, 100.0f};
        float sentinel    = 99.9f;

        /* alpha = 0.5: after 1 call, baseline[0] should be 50.0 */
        update_baseline(baseline, bins, 4, 0.5f);
        ASSERT_FLOAT_EQ("update_baseline: alpha=0.5 converges halfway in one step",
            baseline[0], 50.0f);

        /* alpha = 0: baseline stays unchanged */
        float bl_zero[4] = {10.0f, 10.0f, 10.0f, 10.0f};
        update_baseline(bl_zero, bins, 4, 0.0f);
        ASSERT_FLOAT_EQ("update_baseline: alpha=0 leaves baseline unchanged",
            bl_zero[0], 10.0f);

        /* alpha = 1: baseline becomes bins immediately */
        float bl_one[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        update_baseline(bl_one, bins, 4, 1.0f);
        ASSERT_FLOAT_EQ("update_baseline: alpha=1 sets baseline equal to bins",
            bl_one[0], 100.0f);

        /* n=2: only first 2 elements updated; element [2] must be untouched */
        float bl_n[4] = {0.0f, 0.0f, sentinel, sentinel};
        float src[4]  = {50.0f, 50.0f, 50.0f, 50.0f};
        update_baseline(bl_n, src, 2, 1.0f);
        ASSERT_FLOAT_EQ("update_baseline: only n elements updated (element 2 untouched)",
            bl_n[2], sentinel);
    }

    /* ── is_narrow_peak ──────────────────────────────────────────────────── */

    {
        float bins[100];
        memset(bins, 0, sizeof(bins));

        /* Tracer: isolated spike at bin 50 — must be detected as narrow */
        bins[50] = 25.0f;
        ASSERT_INT_EQ("is_narrow_peak: isolated spike returns 1",
            is_narrow_peak(bins, 100, 50, 2, 3, 10.0f), 1);

        /* Broadband plateau — neighbors at same level, not narrow */
        for (int i = 44; i <= 56; i++) bins[i] = 25.0f;
        ASSERT_INT_EQ("is_narrow_peak: flat broadband plateau returns 0",
            is_narrow_peak(bins, 100, 50, 2, 3, 10.0f), 0);

        /* Too close to left edge — fewer than 2 neighbors, must return 0 */
        memset(bins, 0, sizeof(bins));
        bins[1] = 25.0f;
        ASSERT_INT_EQ("is_narrow_peak: peak at bin 1 with skip=2 span=3 has no left neighbors, returns 0",
            is_narrow_peak(bins, 100, 1, 2, 3, 10.0f), 0);

        /* Speech-like gentle slope spanning bins 40-60:
         * energy must cover the neighbor sampling range (peak±3 to peak±5)
         * so the average isn't pulled down by zeros outside the signal */
        memset(bins, 0, sizeof(bins));
        for (int i = 40; i <= 60; i++) {
            float dist = (float)(i < 50 ? 50 - i : i - 50);
            bins[i] = 25.0f - dist * 0.5f;  /* peak 25, falls 0.5dB/bin → 22.5 at ±5 */
        }
        /* bins[45..47]=22.5/23/23.5, bins[50]=25, bins[53..55]=23.5/23/22.5
         * avg neighbors ≈ 23 dB → advantage ≈ 2 dB < 10 dB threshold */
        ASSERT_INT_EQ("is_narrow_peak: gentle vocal slope (2dB over neighbors) returns 0",
            is_narrow_peak(bins, 100, 50, 2, 3, 10.0f), 0);

        /* Feedback spike riding on top of vocal floor: narrow spike 15dB above the
         * surrounding vocal energy — should still be classified as narrow */
        for (int i = 40; i <= 60; i++) bins[i] = 10.0f;  /* vocal floor */
        bins[50] = 25.0f;                                  /* feedback spike +15dB */
        ASSERT_INT_EQ("is_narrow_peak: narrow spike 15dB above vocal floor returns 1",
            is_narrow_peak(bins, 100, 50, 2, 3, 10.0f), 1);
    }

    /* ── fader_db_to_float / fader_float_to_db ───────────────────────────── */

    {
        ASSERT_FLOAT_EQ("fader_db_to_float: +10 dB = 1.000",   fader_db_to_float(10.0f),   1.0f);
        ASSERT_FLOAT_EQ("fader_db_to_float:   0 dB = 0.750",   fader_db_to_float(0.0f),    0.75f);
        ASSERT_FLOAT_EQ("fader_db_to_float: -10 dB = 0.500",   fader_db_to_float(-10.0f),  0.5f);
        ASSERT_FLOAT_EQ("fader_db_to_float: -30 dB = 0.250",   fader_db_to_float(-30.0f),  0.25f);
        ASSERT_FLOAT_EQ("fader_db_to_float: -60 dB = 0.0625",  fader_db_to_float(-60.0f),  0.0625f);
        ASSERT_FLOAT_EQ("fader_db_to_float: -90 dB = 0.000",   fader_db_to_float(-90.0f),  0.0f);
        ASSERT_FLOAT_EQ("fader_db_to_float: -120 dB clamps 0", fader_db_to_float(-120.0f), 0.0f);

        ASSERT_FLOAT_EQ("fader_float_to_db: 1.000  = +10 dB",  fader_float_to_db(1.0f),     10.0f);
        ASSERT_FLOAT_EQ("fader_float_to_db: 0.750  =   0 dB",  fader_float_to_db(0.75f),     0.0f);
        ASSERT_FLOAT_EQ("fader_float_to_db: 0.500  = -10 dB",  fader_float_to_db(0.5f),    -10.0f);
        ASSERT_FLOAT_EQ("fader_float_to_db: 0.250  = -30 dB",  fader_float_to_db(0.25f),   -30.0f);
        ASSERT_FLOAT_EQ("fader_float_to_db: 0.0625 = -60 dB",  fader_float_to_db(0.0625f), -60.0f);
        ASSERT_FLOAT_EQ("fader_float_to_db: 0.000  = -90 dB",  fader_float_to_db(0.0f),    -90.0f);

        ASSERT_FLOAT_EQ("fader round-trip:   0 dB", fader_float_to_db(fader_db_to_float(0.0f)),   0.0f);
        ASSERT_FLOAT_EQ("fader round-trip: -20 dB", fader_float_to_db(fader_db_to_float(-20.0f)), -20.0f);
    }

    /* ── toast_step ─────────────────────────────────────────────────────── */

    {
        /* Spike at bin 56 (par 18, 1kHz) for 3 frames → NOTCH on frame 3 */
        float bins[100], baseline[100];
        memset(bins,     0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));
        bins[56] = 25.0f; /* 25 dB above baseline, threshold = 20 dB */

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 10.0f, 3, 10.0f, 2, 3);

        ToastAction out[32];

        /* frame 1: confirm_hold[17] == 1, no notch */
        int n = toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("toast_step tracer: frame 1 → 0 actions", n, 0);

        /* frame 2: confirm_hold[17] == 2, still no notch */
        n = toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("toast_step tracer: frame 2 → 0 actions", n, 0);

        /* frame 3: confirm_hold[17] == 3 >= confirm_frames → NOTCH */
        n = toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("toast_step tracer: frame 3 → 1 action", n, 1);
        ASSERT_INT_EQ("toast_step tracer: action is TOAST_NOTCH", out[0].op, TOAST_NOTCH);
        ASSERT_INT_EQ("toast_step tracer: geq_par == 18", out[0].geq_par, 18);
    }

    {
        /* After notch: feed bin 56 below release_thr for release_frames → RELEASE_RAMP each, RELEASE_DONE at end */
        /* threshold=20, release_thr=10; baseline=0, so bins[56] < 10 triggers release */
        float bins[100], baseline[100];
        memset(bins, 0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));
        bins[56] = 25.0f;

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);
        /* release_frames = 5 * 20 = 100 */

        ToastAction out[32];
        /* Get a notch placed first (3 frames of spike) */
        for (int i = 0; i < 3; i++) toast_step(&st, bins, baseline, out, 32);

        /* Now go quiet: bins[56] = 0 < baseline[56] + release_thr (=10) */
        bins[56] = 0.0f;

        int ramp_seen = 0, done_seen = 0;
        for (int f = 0; f < 100; f++) {
            int n = toast_step(&st, bins, baseline, out, 32);
            for (int i = 0; i < n; i++) {
                if (out[i].op == TOAST_RELEASE_RAMP && out[i].geq_par == 18) ramp_seen++;
                if (out[i].op == TOAST_RELEASE_DONE && out[i].geq_par == 18) done_seen++;
            }
        }
        ASSERT_INT_EQ("toast_step release: RELEASE_RAMP seen 99 times", ramp_seen, 99);
        ASSERT_INT_EQ("toast_step release: RELEASE_DONE seen once", done_seen, 1);
        ASSERT_INT_EQ("toast_step release: band inactive after done", st.active[17], 0);
    }

    {
        /* After RELEASE_DONE, spike again → notch fires again (re-trigger) */
        float bins[100], baseline[100];
        memset(bins, 0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));
        bins[56] = 25.0f;

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);

        ToastAction out[32];
        /* Place notch (3 frames spike) */
        for (int i = 0; i < 3; i++) toast_step(&st, bins, baseline, out, 32);
        /* Full release (100 quiet frames) */
        bins[56] = 0.0f;
        for (int i = 0; i < 100; i++) toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("re-trigger: band inactive after release", st.active[17], 0);

        /* Spike again for 3 frames → new NOTCH */
        bins[56] = 25.0f;
        int notch2 = 0;
        for (int i = 0; i < 3; i++) {
            int cnt = toast_step(&st, bins, baseline, out, 32);
            for (int k = 0; k < cnt; k++)
                if (out[k].op == TOAST_NOTCH && out[k].geq_par == 18) notch2++;
        }
        ASSERT_INT_EQ("re-trigger: second notch fires", notch2, 1);
        ASSERT_INT_EQ("re-trigger: band active again", st.active[17], 1);
    }

    {
        /* Mid-release: spike returns before release_frames → snap back to cut_val */
        float bins[100], baseline[100];
        memset(bins, 0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));
        bins[56] = 25.0f;

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);

        ToastAction out[32];
        /* Place notch */
        for (int i = 0; i < 3; i++) toast_step(&st, bins, baseline, out, 32);
        float saved_cut = st.cut_val[17];

        /* 5 quiet frames (partial release) */
        bins[56] = 0.0f;
        for (int i = 0; i < 5; i++) toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("snap-back: release_hold > 0 after partial release",
                      st.release_hold[17] > 0, 1);

        /* Spike returns — next step should snap back */
        bins[56] = 25.0f;
        int cnt = toast_step(&st, bins, baseline, out, 32);
        int snap_found = 0;
        for (int i = 0; i < cnt; i++) {
            if (out[i].op == TOAST_RELEASE_RAMP && out[i].geq_par == 18
                    && fabsf(out[i].ramp_val - saved_cut) < 0.001f)
                snap_found = 1;
        }
        ASSERT_INT_EQ("snap-back: RELEASE_RAMP emitted with ramp_val == cut_val", snap_found, 1);
        ASSERT_INT_EQ("snap-back: release_hold reset to 0", st.release_hold[17], 0);
    }

    {
        /* Narrowness gate: broadband — bin 56 is highest but neighbors nearly equal */
        float bins[100], baseline[100];
        memset(bins, 0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));
        /* 24 dBFS across 50-62, spike to 25 at 56 (1 dB above neighbors, < 10 dB min) */
        for (int i = 50; i <= 62; i++) bins[i] = 24.0f;
        bins[56] = 25.0f;

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);

        ToastAction out[32];
        int notch_seen = 0;
        for (int i = 0; i < 5; i++) {
            int cnt = toast_step(&st, bins, baseline, out, 32);
            for (int k = 0; k < cnt; k++)
                if (out[k].op == TOAST_NOTCH) notch_seen++;
        }
        ASSERT_INT_EQ("narrowness gate: broadband → no notch", notch_seen, 0);
        ASSERT_INT_EQ("narrowness gate: no bands active", st.active[17], 0);
    }

    {
        /* from_profile: band 5 (par 5, index 4) pinned — never auto-released */
        float bins[100], baseline[100];
        memset(bins, 0, sizeof(bins));
        memset(baseline, 0, sizeof(baseline));

        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);

        /* Pin band 4 (par 5) as a profile notch */
        st.active[4]       = 1;
        st.from_profile[4] = 1;
        st.cut_val[4]      = db_to_geq_float(-6.0f);

        ToastAction out[32];
        int release_seen = 0;
        /* Feed quiet for release_frames + 1 = 101 frames */
        for (int f = 0; f < 101; f++) {
            int cnt = toast_step(&st, bins, baseline, out, 32);
            for (int i = 0; i < cnt; i++) {
                if ((out[i].op == TOAST_RELEASE_RAMP || out[i].op == TOAST_RELEASE_DONE)
                        && out[i].geq_par == 5)
                    release_seen++;
            }
        }
        ASSERT_INT_EQ("from_profile: no RELEASE_* emitted for pinned band", release_seen, 0);
        ASSERT_INT_EQ("from_profile: band still active", st.active[4], 1);
    }

    {
        /* Confirm-counter reset: frame 1 peaks at par 10 (bin 33), frame 2 at par 15 (bin 46)
         * → confirm_hold[9] must be 0 after frame 2 */
        float bins[100], baseline[100];
        memset(baseline, 0, sizeof(baseline));

        /* par 10 = TOAST_GEQ_BIN[9] = bin 30, par 15 = TOAST_GEQ_BIN[14] = bin 46 */
        ToastState st;
        toast_state_init(&st, 20.0f, -6.0f, 5.0f, 3, 10.0f, 2, 3);

        ToastAction out[32];

        /* Frame 1: spike at bin 30 (par 10) */
        memset(bins, 0, sizeof(bins));
        bins[30] = 25.0f;
        toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("confirm-reset: confirm_hold[9] == 1 after frame 1", st.confirm_hold[9], 1);

        /* Frame 2: spike at bin 46 (par 15 — different par) */
        memset(bins, 0, sizeof(bins));
        bins[46] = 25.0f;
        toast_step(&st, bins, baseline, out, 32);
        ASSERT_INT_EQ("confirm-reset: confirm_hold[9] == 0 after frame 2 (different par)",
                      st.confirm_hold[9], 0);
        ASSERT_INT_EQ("confirm-reset: confirm_hold[14] == 1 after frame 2", st.confirm_hold[14], 1);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
