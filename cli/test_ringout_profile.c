#include <stdio.h>
#include <math.h>
#include <string.h>
#include "ringout_profile.h"

static int passed = 0, failed = 0;
static void ok(const char *n) { passed++; printf("PASS  %s\n", n); }
static void fail(const char *n, const char *r) { failed++; printf("FAIL  %s: %s\n", n, r); }

#define ASSERT_INT_EQ(name, got, want) do { \
    int _g = (got), _w = (want); \
    if (_g == _w) ok(name); \
    else { char _b[80]; snprintf(_b, sizeof(_b), "got %d want %d", _g, _w); fail(name, _b); } \
} while (0)

#define ASSERT_FLOAT_EQ(name, got, want) do { \
    float _g = (got), _w = (want); \
    if (fabsf(_g - _w) < 0.001f) ok(name); \
    else { char _b[80]; snprintf(_b, sizeof(_b), "got %.3f want %.3f", _g, _w); fail(name, _b); } \
} while (0)

#define ASSERT_STR_EQ(name, got, want) do { \
    if (strcmp((got),(want)) == 0) ok(name); \
    else { char _b[96]; snprintf(_b, sizeof(_b), "got '%s' want '%s'", got, want); fail(name, _b); } \
} while (0)

int main(void) {
    /* ── round trip: serialize → parse preserves every field ─────────────── */
    {
        RingoutProfile p;
        memset(&p, 0, sizeof(p));
        strcpy(p.model, "XR18");
        p.bus = 3; p.fx_slot = 4; p.margin_db = 6.0f;
        strcpy(p.created, "2026-06-29T00:00:00");
        p.band_db[17] = -6.0f;  /* par 18, 1 kHz */
        p.band_db[7]  = -9.0f;  /* par 8, 100 Hz */

        char buf[2048];
        int n = ringout_profile_serialize(&p, buf, sizeof(buf));
        ASSERT_INT_EQ("serialize: returns positive length", n > 0, 1);

        RingoutProfile q;
        ASSERT_INT_EQ("parse: returns 0 on valid text", ringout_profile_parse(buf, &q), 0);
        ASSERT_STR_EQ ("round-trip: model", q.model, "XR18");
        ASSERT_INT_EQ ("round-trip: bus", q.bus, 3);
        ASSERT_INT_EQ ("round-trip: fx_slot", q.fx_slot, 4);
        ASSERT_FLOAT_EQ("round-trip: margin_db", q.margin_db, 6.0f);
        ASSERT_STR_EQ ("round-trip: created", q.created, "2026-06-29T00:00:00");
        ASSERT_FLOAT_EQ("round-trip: band 18 = -6", q.band_db[17], -6.0f);
        ASSERT_FLOAT_EQ("round-trip: band 8 = -9",  q.band_db[7],  -9.0f);
        ASSERT_FLOAT_EQ("round-trip: band 1 = flat", q.band_db[0],  0.0f);

        /* serialize→parse→serialize is byte-identical */
        char buf2[2048];
        ringout_profile_serialize(&q, buf2, sizeof(buf2));
        ASSERT_INT_EQ("round-trip: re-serialize byte-identical", strcmp(buf, buf2), 0);
    }

    /* ── parse hand-written text ─────────────────────────────────────────── */
    {
        const char *text =
            "# comment line\n"
            "model X32\n"
            "bus 6\n"
            "fx_slot 2\n"
            "margin_db 4.5\n"
            "band 05 -12.0\n";
        RingoutProfile p;
        ASSERT_INT_EQ("parse hand-written: ok", ringout_profile_parse(text, &p), 0);
        ASSERT_STR_EQ("parse: model X32", p.model, "X32");
        ASSERT_INT_EQ("parse: bus 6", p.bus, 6);
        ASSERT_FLOAT_EQ("parse: band 5 = -12", p.band_db[4], -12.0f);
        ASSERT_FLOAT_EQ("parse: unnamed band defaults flat", p.band_db[30], 0.0f);
    }

    /* ── missing identifying fields → error ──────────────────────────────── */
    {
        RingoutProfile p;
        ASSERT_INT_EQ("parse: missing model/bus/slot rejected",
                      ringout_profile_parse("margin_db 3.0\n", &p), -1);
    }

    /* ── buffer too small → -1 ───────────────────────────────────────────── */
    {
        RingoutProfile p;
        memset(&p, 0, sizeof(p));
        strcpy(p.model, "XR18");
        char tiny[16];
        ASSERT_INT_EQ("serialize: tiny buffer returns -1",
                      ringout_profile_serialize(&p, tiny, sizeof(tiny)), -1);
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
