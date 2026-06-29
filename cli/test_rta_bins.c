#include <stdio.h>
#include <math.h>
#include "rta_bins.h"
#include "toast_logic.h"   /* TOAST_GEQ_BIN — anchors the table must honor */

static int passed = 0, failed = 0;
static void ok(const char *n)               { passed++; printf("PASS  %s\n", n); }
static void fail(const char *n, const char *r){ failed++; printf("FAIL  %s: %s\n", n, r); }

#define ASSERT_NEAR(name, got, want, tol) do { \
    float _g = (got), _w = (want); \
    if (fabsf(_g - _w) <= (tol)) ok(name); \
    else { char _b[80]; snprintf(_b, sizeof _b, "got %.2f want %.2f", _g, _w); fail(name, _b); } \
} while (0)

#define ASSERT_INT_EQ(name, got, want) do { \
    int _g = (got), _w = (want); \
    if (_g == _w) ok(name); \
    else { char _b[64]; snprintf(_b, sizeof _b, "got %d want %d", _g, _w); fail(name, _b); } \
} while (0)

/* ISO 1/3-oct centers — must align with the 31 GEQ-band anchors. */
static const float ISO[31] = {
    20,25,31.5f,40,50,63,80,100,125,160,200,250,315,400,500,630,800,
    1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000
};

int main(void) {
    /* anchor: 1 kHz lives on bin 56 (pistonphone reference). */
    ASSERT_NEAR("bin 56 = 1 kHz anchor", RTA_BIN_FREQ[56], 1000.0f, 1.0f);

    /* band endpoints */
    ASSERT_NEAR("bin 0 = 20 Hz",  RTA_BIN_FREQ[0],  20.0f,    0.5f);
    ASSERT_NEAR("bin 99 = 20 kHz", RTA_BIN_FREQ[99], 20000.0f, 5.0f);

    /* monotonic increasing across the whole band */
    int mono = 1;
    for (int i = 1; i < RTA_BIN_COUNT; i++)
        if (RTA_BIN_FREQ[i] <= RTA_BIN_FREQ[i - 1]) mono = 0;
    ASSERT_INT_EQ("table is monotonic increasing", mono, 1);

    /* every GEQ-band anchor bin reads its ISO center frequency */
    int anchors_ok = 1;
    for (int i = 0; i < 31; i++) {
        float got = RTA_BIN_FREQ[TOAST_GEQ_BIN[i]];
        if (fabsf(got - ISO[i]) > ISO[i] * 0.01f) anchors_ok = 0;  /* 1% */
    }
    ASSERT_INT_EQ("GEQ anchors map to ISO centers", anchors_ok, 1);

    /* inverse: nearest bin for a frequency, with clamping */
    ASSERT_INT_EQ("freq 1000 -> bin 56", rta_bin_for_freq(1000.0f), 56);
    ASSERT_INT_EQ("freq 100  -> bin 23", rta_bin_for_freq(100.0f), 23);
    ASSERT_INT_EQ("freq 10000-> bin 89", rta_bin_for_freq(10000.0f), 89);
    ASSERT_INT_EQ("sub-band clamps to 0",  rta_bin_for_freq(5.0f), 0);
    ASSERT_INT_EQ("super-band clamps to 99", rta_bin_for_freq(40000.0f), 99);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
