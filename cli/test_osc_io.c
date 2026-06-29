#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "osc_io.h"

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

#define ASSERT_PTR_NULL(name, got) do { \
    if ((got) == NULL) ok(name); \
    else fail(name, "expected NULL"); \
} while (0)

#define ASSERT_PTR_NOTNULL(name, got) do { \
    if ((got) != NULL) ok(name); \
    else fail(name, "expected non-NULL"); \
} while (0)

int main(void) {

    /* ── osc_locate_blob ─────────────────────────────────────────────────── */

    {
        /* Too short: buffer shorter than the OSC address alone */
        char buf[4] = "/me";  /* no null within the expected padded range */
        buf[3] = '\0';
        ASSERT_PTR_NULL("osc_locate_blob: too-short buffer returns NULL",
                        osc_locate_blob(buf, 4, NULL));
    }

    {
        /* Synthetic /meters/4 packet (hand-built, known offsets):
         *   addr "/meters/4\0\0\0"  = 12 bytes  (strlen=9, padded=(9+1+3)&~3=12)
         *   tag  ",b\0\0"           =  4 bytes  (strlen=2, padded=(2+1+3)&~3=4)
         *   blob starts at byte 16
         *
         * Fill blob with 4-byte big-endian length (200) + 200 dummy bytes. */
        const int ADDR_PAD = 12;
        const int TAG_PAD  = 4;
        const int BLOB_OFF = ADDR_PAD + TAG_PAD; /* 16 */
        const int BLOB_LEN = 8;                  /* just enough for a tiny blob payload */
        const int BUFLEN   = BLOB_OFF + BLOB_LEN;

        char buf[BUFLEN];
        memset(buf, 0, sizeof(buf));
        memcpy(buf, "/meters/4", 9);     /* address (9 bytes, rest already zero) */
        memcpy(buf + ADDR_PAD, ",b", 2); /* type tag */
        /* blob payload: first 4 bytes = big-endian blob length (4) */
        buf[BLOB_OFF]     = 0;
        buf[BLOB_OFF + 1] = 0;
        buf[BLOB_OFF + 2] = 0;
        buf[BLOB_OFF + 3] = 4;

        int blob_len = -1;
        const uint8_t *blob = osc_locate_blob(buf, BUFLEN, &blob_len);

        ASSERT_PTR_NOTNULL("osc_locate_blob: valid /meters/4 returns non-NULL", blob);
        ASSERT_INT_EQ("osc_locate_blob: blob offset correct (byte 16)",
                      (int)(blob - (const uint8_t *)buf), BLOB_OFF);
        ASSERT_INT_EQ("osc_locate_blob: blob_len == BUFLEN - BLOB_OFF",
                      blob_len, BUFLEN - BLOB_OFF);
    }

    {
        /* Blob starts exactly at end of buffer (blob_start == buflen) → NULL */
        /* addr "/x\0\0" = 4 bytes, tag ",\0\0\0" = 4 bytes → blob_start=8 */
        char buf[8] = {'/','x',0,0, ',',0,0,0};
        ASSERT_PTR_NULL("osc_locate_blob: blob_start == buflen returns NULL",
                        osc_locate_blob(buf, 8, NULL));
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
