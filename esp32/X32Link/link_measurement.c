// Pure logic for the Ableton Link measurement (ping/pong) protocol.
// No WiFi/Arduino dependency — testable on host. See link_measurement.h for
// the wire format and algorithm references.

#include "link_measurement.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const uint8_t MAGIC[8] = {'_','l','i','n','k','_','v', 1};

static const uint8_t MSG_PING = 1;
static const uint8_t MSG_PONG = 2;

static const uint32_t HOST_TIME_KEY  = 0x5f5f6874u;  // '__ht'
static const uint32_t GHOST_TIME_KEY = 0x5f5f6774u;  // '__gt'
static const uint32_t PREV_GHOST_KEY = 0x5f706774u;  // '_pgt'
// SessionMembership ('sess') is present in real pongs but ignored here —
// we don't track session identity changes in this path.

/* ---------------------------------------------------------------------- */
/* Byte helpers (BE), same conventions as link_protocol.c                 */
/* ---------------------------------------------------------------------- */

static int64_t be64(const uint8_t* p) {
    int64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static void put_be64(uint8_t* p, int64_t v) {
    for (int i = 7; i >= 0; i--) { p[i] = (uint8_t)v; v >>= 8; }
}

// Append one TLV (key + 8-byte int64 value) at buf[i], return new offset.
static int put_tlv_i64(uint8_t* buf, int i, uint32_t key, int64_t value) {
    put_be32(buf + i, key);     i += 4;
    put_be32(buf + i, 8);       i += 4;
    put_be64(buf + i, value);   i += 8;
    return i;
}

/* ---------------------------------------------------------------------- */
/* GhostXForm                                                              */
/* ---------------------------------------------------------------------- */

int64_t link_ghost_xform_host_to_ghost(LinkGhostXForm xform, int64_t host_us) {
    return host_us + xform.intercept_us;
}

/* ---------------------------------------------------------------------- */
/* Ping build/parse                                                        */
/* ---------------------------------------------------------------------- */

int link_measurement_build_ping(uint8_t* buf, int buf_cap,
                                 int64_t host_time_us,
                                 bool has_prev_ghost, int64_t prev_ghost_us) {
    if (!buf) return 0;
    int need = 8 + 1 + 16 + (has_prev_ghost ? 16 : 0);
    if (buf_cap < need) return 0;

    int i = 0;
    memcpy(buf, MAGIC, 8); i += 8;
    buf[i++] = MSG_PING;
    i = put_tlv_i64(buf, i, HOST_TIME_KEY, host_time_us);
    if (has_prev_ghost) i = put_tlv_i64(buf, i, PREV_GHOST_KEY, prev_ghost_us);
    return i;
}

bool link_measurement_parse_ping(const uint8_t* buf, int len, LinkPingFields* out) {
    if (!buf || len < 9) return false;
    if (memcmp(buf, MAGIC, 8) != 0) return false;
    if (buf[8] != MSG_PING) return false;
    if (out) memset(out, 0, sizeof(*out));

    const uint8_t* p   = buf + 9;
    const uint8_t* end = buf + len;
    while (p + 8 <= end) {
        uint32_t key  = be32(p); p += 4;
        uint32_t size = be32(p); p += 4;
        if (p + size > end) break;
        if (key == HOST_TIME_KEY && size >= 8 && out) {
            out->has_host_time = true;
            out->host_time_us  = be64(p);
        } else if (key == PREV_GHOST_KEY && size >= 8 && out) {
            out->has_prev_ghost_time = true;
            out->prev_ghost_time_us  = be64(p);
        }
        p += size;
    }
    return true;
}

/* ---------------------------------------------------------------------- */
/* Pong parse                                                              */
/* ---------------------------------------------------------------------- */

bool link_measurement_parse_pong(const uint8_t* buf, int len, LinkPongFields* out) {
    if (!buf || len < 9) return false;
    if (memcmp(buf, MAGIC, 8) != 0) return false;
    if (buf[8] != MSG_PONG) return false;
    if (out) memset(out, 0, sizeof(*out));

    const uint8_t* p   = buf + 9;
    const uint8_t* end = buf + len;
    while (p + 8 <= end) {
        uint32_t key  = be32(p); p += 4;
        uint32_t size = be32(p); p += 4;
        if (p + size > end) break;
        if (key == GHOST_TIME_KEY && size >= 8 && out) {
            out->has_ghost_time = true;
            out->ghost_time_us  = be64(p);
        } else if (key == HOST_TIME_KEY && size >= 8 && out) {
            out->has_host_time = true;
            out->host_time_us  = be64(p);
        } else if (key == PREV_GHOST_KEY && size >= 8 && out) {
            out->has_prev_ghost_time = true;
            out->prev_ghost_time_us  = be64(p);
        }
        // unknown keys (e.g. SessionMembership 'sess') skipped via p += size below
        p += size;
    }
    return true;
}

/* ---------------------------------------------------------------------- */
/* Sample buffer / median                                                  */
/* ---------------------------------------------------------------------- */

static double s_samples[LINK_MEASUREMENT_MAX_SAMPLES];
static int    s_sample_count = 0;
static int    s_sample_next  = 0;  // ring write cursor

void link_measurement_samples_reset(void) {
    s_sample_count = 0;
    s_sample_next  = 0;
}

int link_measurement_samples_count(void) { return s_sample_count; }

static void push_sample(double v) {
    s_samples[s_sample_next] = v;
    s_sample_next = (s_sample_next + 1) % LINK_MEASUREMENT_MAX_SAMPLES;
    if (s_sample_count < LINK_MEASUREMENT_MAX_SAMPLES) s_sample_count++;
}

int link_measurement_add_pong_samples(int64_t h_recv, const LinkPongFields* pong) {
    if (!pong || !pong->has_ghost_time || !pong->has_host_time) return 0;

    // LNK-026 bug 2: drop a pong whose round trip is negative or implausibly
    // large — it's a stale/mismatched reply (e.g. left in the RX buffer across
    // the idle gap between attempts), and its ~2s-old echoed host_time would
    // poison the median by ~1s (~2 beats). See LINK_MEASUREMENT_MAX_RTT_US.
    int64_t rtt = h_recv - pong->host_time_us;
    if (rtt < 0 || rtt > LINK_MEASUREMENT_MAX_RTT_US) return 0;

    int added = 0;
    double g      = (double)pong->ghost_time_us;
    double h_sent = (double)pong->host_time_us;
    double h_r    = (double)h_recv;

    double sample_a = g - (h_r + h_sent) / 2.0;
    push_sample(sample_a);
    added++;

    if (pong->has_prev_ghost_time) {
        double prev_g    = (double)pong->prev_ghost_time_us;
        double sample_b  = (g + prev_g) / 2.0 - h_sent;
        push_sample(sample_b);
        added++;
    }

    return added;
}

static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a, db = *(const double*)b;
    return (da > db) - (da < db);
}

bool link_measurement_median(double* out_median) {
    if (s_sample_count == 0) return false;

    double tmp[LINK_MEASUREMENT_MAX_SAMPLES];
    memcpy(tmp, s_samples, sizeof(double) * (size_t)s_sample_count);
    qsort(tmp, (size_t)s_sample_count, sizeof(double), cmp_double);

    int n = s_sample_count;
    double m = (n % 2 == 1) ? tmp[n / 2]
                            : (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0;
    if (out_median) *out_median = m;
    return true;
}

/* ---------------------------------------------------------------------- */
/* Attempt lifecycle                                                       */
/* ---------------------------------------------------------------------- */

static bool           s_active = false;
static LinkGhostXForm s_xform  = {0, false};

void link_measurement_reset(void) {
    link_measurement_samples_reset();
    s_active = false;
    s_xform.intercept_us = 0;
    s_xform.valid = false;
}

void link_measurement_attempt_begin(void) {
    link_measurement_samples_reset();
    s_active = true;
}

void link_measurement_attempt_end(bool success) {
    if (success) {
        double m;
        if (link_measurement_median(&m)) {
            s_xform.intercept_us = (int64_t)llround(m);
            s_xform.valid = true;
        }
        // empty sample buffer despite a "success" call: leave existing
        // xform (if any) untouched rather than committing garbage.
    }
    s_active = false;
}

LinkGhostXForm link_measurement_current_xform(void) { return s_xform; }
bool           link_measurement_active(void)        { return s_active; }
