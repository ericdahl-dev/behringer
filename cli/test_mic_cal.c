#include <stdio.h>
#include <math.h>
#include <string.h>
#include "mic_cal.h"

static int passed = 0, failed = 0;
static void ok(const char *n)                { passed++; printf("PASS  %s\n", n); }
static void fail(const char *n, const char *r){ failed++; printf("FAIL  %s: %s\n", n, r); }

#define ASSERT_INT_EQ(name, got, want) do { \
    int _g=(got),_w=(want); if(_g==_w) ok(name); \
    else { char _b[64]; snprintf(_b,sizeof _b,"got %d want %d",_g,_w); fail(name,_b);} } while(0)
#define ASSERT_NEAR(name, got, want, tol) do { \
    float _g=(got),_w=(want); if(fabsf(_g-_w)<=(tol)) ok(name); \
    else { char _b[80]; snprintf(_b,sizeof _b,"got %.4f want %.4f",_g,_w); fail(name,_b);} } while(0)

static const char REW[] =
    "Sensitivity -12.34 dBFS\n"
    "20 -0.5\n"
    "1000 0.0\n"
    "10000 1.5\n";

static const char FACTORY[] =
    "*1000Hz\t-38.6\n"
    "\n"
    "20.00\t0.1\n"
    "1000.00\t0.0\n"
    "10000.00\t2.2\n"
    "20000.00\t2.5\n";

int main(void) {
    MicCal c;
    ASSERT_INT_EQ("REW .cal parses", mic_cal_parse(REW, (int)strlen(REW), &c), 0);
    ASSERT_NEAR("value at exact 1 kHz point", mic_cal_at(&c, 1000.0f), 0.0f, 1e-4f);

    /* log-frequency interpolation: midpoint of 1k(0.0)→10k(1.5) is 3162 Hz → 0.75 */
    ASSERT_NEAR("log-interp midpoint", mic_cal_at(&c, 3162.28f), 0.75f, 0.01f);

    /* clamp outside the cal range to the endpoints */
    ASSERT_NEAR("clamp below = first gain", mic_cal_at(&c, 5.0f),     -0.5f, 1e-4f);
    ASSERT_NEAR("clamp above = last gain",  mic_cal_at(&c, 40000.0f),  1.5f, 1e-4f);

    /* REW Sensitivity line captured */
    ASSERT_INT_EQ("REW has sensitivity", c.has_sensitivity, 1);
    ASSERT_NEAR("REW sensitivity value", c.sensitivity_dbfs, -12.34f, 1e-3f);

    /* per-bin correction */
    float binf[3] = {20.0f, 1000.0f, 10000.0f}, corr[3];
    mic_cal_bin_corrections(&c, binf, corr, 3);
    ASSERT_NEAR("bin corr[0] (20 Hz)",   corr[0], -0.5f, 1e-4f);
    ASSERT_NEAR("bin corr[2] (10 kHz)",  corr[2],  1.5f, 1e-4f);

    /* factory .txt format: tab-separated rows + *<f>Hz sensitivity line */
    MicCal f;
    ASSERT_INT_EQ("factory .txt parses", mic_cal_parse(FACTORY, (int)strlen(FACTORY), &f), 0);
    ASSERT_INT_EQ("factory point count", f.n, 4);
    ASSERT_NEAR("factory @1 kHz", mic_cal_at(&f, 1000.0f), 0.0f, 1e-4f);
    ASSERT_NEAR("factory @20 kHz", mic_cal_at(&f, 20000.0f), 2.5f, 1e-4f);
    ASSERT_NEAR("factory sensitivity", f.sensitivity_dbfs, -38.6f, 1e-3f);

    /* reject buffers with fewer than 2 usable points */
    MicCal bad;
    ASSERT_INT_EQ("garbage rejected",  mic_cal_parse("hello\nworld\n", 12, &bad), -1);
    ASSERT_INT_EQ("single point rejected", mic_cal_parse("1000 0.0\n", 9, &bad), -1);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
