/* test_tui_helpers.c — unit tests for tui pure helper functions.
 * Compiled standalone (no ncurses) with inline copies of the three helpers. */
#include <stdio.h>
#include <assert.h>

/* ── inline implementations (mirror tui.c exactly) ─────────────────────── */

static int tui_bin_to_col(int bin, int cols) {
    if (cols <= 1) return 0;
    return bin * (cols - 1) / 99;
}

static int tui_col_to_bin(int col, int cols) {
    if (cols <= 1) return 0;
    return col * 99 / (cols - 1);
}

static int tui_excess_level(float excess, float threshold) {
    if (excess < 5.0f)       return 0;
    if (excess >= threshold) return 4;  /* threshold may be < 10 */
    if (excess < 10.0f)      return 1;
    if (excess < 15.0f)      return 2;
    return 3;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

static int failures = 0;

#define CHECK(expr) \
    do { if (!(expr)) { fprintf(stderr, "FAIL: %s line %d\n", #expr, __LINE__); failures++; } } while (0)

static void test_bin_to_col(void) {
    /* boundaries */
    CHECK(tui_bin_to_col(0,  100) == 0);
    CHECK(tui_bin_to_col(99, 100) == 99);

    /* 1kHz anchor: bin 56 of 100 -> col 56 of 100 */
    CHECK(tui_bin_to_col(56, 100) == 56);

    /* wider terminal: bin 0 -> col 0, bin 99 -> col COLS-1 */
    CHECK(tui_bin_to_col(0,  200) == 0);
    CHECK(tui_bin_to_col(99, 200) == 199);

    /* narrow: COLS=4 */
    CHECK(tui_bin_to_col(0,  4) == 0);
    CHECK(tui_bin_to_col(99, 4) == 3);

    /* degenerate */
    CHECK(tui_bin_to_col(50, 1) == 0);

    /* mid-range monotone: bin 33 < bin 66 for any cols > 1 */
    CHECK(tui_bin_to_col(33, 100) < tui_bin_to_col(66, 100));
}

static void test_col_to_bin(void) {
    CHECK(tui_col_to_bin(0,   100) == 0);
    CHECK(tui_col_to_bin(99,  100) == 99);

    /* degenerate */
    CHECK(tui_col_to_bin(50, 1) == 0);

    /* round-trip: col_to_bin(bin_to_col(b)) should be within 1 of b */
    for (int b = 0; b <= 99; b++) {
        int col  = tui_bin_to_col(b, 100);
        int back = tui_col_to_bin(col, 100);
        int diff = back - b;
        if (diff < 0) diff = -diff;
        CHECK(diff <= 1);
    }

    /* monotone */
    CHECK(tui_col_to_bin(0, 200) <= tui_col_to_bin(100, 200));
}

static void test_excess_level(void) {
    float thr = 20.0f;

    /* level 0: below 5 dB */
    CHECK(tui_excess_level(-10.0f, thr) == 0);
    CHECK(tui_excess_level(0.0f,   thr) == 0);
    CHECK(tui_excess_level(4.99f,  thr) == 0);

    /* level 1: 5–10 */
    CHECK(tui_excess_level(5.0f,  thr) == 1);
    CHECK(tui_excess_level(7.5f,  thr) == 1);
    CHECK(tui_excess_level(9.99f, thr) == 1);

    /* level 2: 10–15 */
    CHECK(tui_excess_level(10.0f, thr) == 2);
    CHECK(tui_excess_level(12.5f, thr) == 2);
    CHECK(tui_excess_level(14.99f, thr) == 2);

    /* level 3: 15 – threshold */
    CHECK(tui_excess_level(15.0f, thr) == 3);
    CHECK(tui_excess_level(19.99f, thr) == 3);

    /* level 4: at or above threshold */
    CHECK(tui_excess_level(20.0f,  thr) == 4);
    CHECK(tui_excess_level(30.0f,  thr) == 4);

    /* small threshold: threshold=5 means 5 dB is already peak */
    CHECK(tui_excess_level(5.0f, 5.0f) == 4);

    /* large threshold: level 3 spans a wide range */
    CHECK(tui_excess_level(50.0f, 100.0f) == 3);
    CHECK(tui_excess_level(100.0f, 100.0f) == 4);
}

int main(void) {
    test_bin_to_col();
    test_col_to_bin();
    test_excess_level();

    if (failures == 0) {
        printf("test_tui_helpers: all tests passed\n");
        return 0;
    }
    fprintf(stderr, "test_tui_helpers: %d failure(s)\n", failures);
    return 1;
}
