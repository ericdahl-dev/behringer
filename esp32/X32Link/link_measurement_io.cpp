// Thin Arduino glue: WiFiUDP unicast ping/pong exchange against a Link
// peer's mep4 endpoint (discovered by LNK-017's link_protocol.c). All wire
// format + offset math lives in pure, host-tested link_measurement.c — this
// file only owns sockets, timing, and the retry/failure state machine.
//
// Not host-testable (needs real WiFiUDP/hardware) — kept deliberately thin
// per LNK-018: don't unit test this, just implement it per spec.
#include "link_measurement_io.h"
#include "link_measurement.h"
#include "link_protocol.h"
#include <WiFiUdp.h>
#include "esp_timer.h"

// Independent socket from link_listener.cpp's multicast discovery
// conversation (port 20808) — this is a separate unicast UDP conversation.
// Port 0 = let the OS assign an ephemeral local port; we only ever talk to
// whatever mep4 endpoint the peer advertised, so we don't need a fixed
// local port.
static WiFiUDP s_udp;
static bool    s_socket_open = false;

// Reference peer currently targeted (or last successfully measured).
static bool     s_have_ref  = false;
static uint32_t s_ref_ip    = 0;
static uint16_t s_ref_port  = 0;

// In-flight attempt bookkeeping (round count / PrevGHostTime carry-over /
// watchdog timing). Pure arithmetic, but per the ticket this lives here in
// the glue layer rather than in link_measurement.c.
static bool    s_have_prev_ghost     = false;
static int64_t s_prev_ghost_us       = 0;
static int64_t s_last_send_us        = 0;
static int     s_consecutive_timeouts = 0;

static const int64_t WATCHDOG_US  = 50000;  // 50ms silence watchdog
static const int      MAX_TIMEOUTS = 5;      // 5 * 50ms = 250ms -> abandon attempt

// Periodic re-measurement (LNK-018 deferred this as a follow-up). Re-measure on
// an interval so we (a) recover when the peer's advertised mep4 was an
// unreachable interface on a multi-homed host (Ableton on a Mac advertises its
// measurement endpoint per-interface; some aren't routable from our subnet), and
// (b) refresh the GhostXForm as clocks drift. A failed re-measure leaves the
// committed xform untouched, so a good measurement keeps phase valid meanwhile.
static const int64_t REMEASURE_INTERVAL_US = 2000000;  // 2 s
static int64_t       s_next_measure_us = 0;

// esp_timer_get_time(), not millis()/micros() — see LNK-019 for why
// microsecond precision matters specifically in this path.
static int64_t now_us() { return esp_timer_get_time(); }

// Build an IPAddress from our wire-format uint32_t (big-endian octets, i.e.
// numerically `192.168.1.5` == 0xC0A80105 — see link_protocol.c's be32()).
// Deliberately NOT using IPAddress(uint32_t) here: that single-arg ctor
// assigns the raw 32-bit word into IPAddress's internal little-endian byte
// layout with no byteswap, so a 0xC0A80105 input would print as
// "5.1.168.192" instead of "192.168.1.5". The 4-octet constructor is
// unambiguous regardless of platform endianness.
static IPAddress ip_from_be32(uint32_t ip) {
    return IPAddress((ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                      (ip >> 8) & 0xFF, ip & 0xFF);
}

static void send_ping(int64_t when_us) {
    uint8_t buf[LINK_MEASUREMENT_PING_MAX_LEN];
    int n = link_measurement_build_ping(buf, sizeof(buf), when_us,
                                         s_have_prev_ghost, s_prev_ghost_us);
    if (n <= 0) return;  // shouldn't happen; buffer is sized for worst case

    s_udp.beginPacket(ip_from_be32(s_ref_ip), s_ref_port);
    s_udp.write(buf, n);
    s_udp.endPacket();
    s_last_send_us = when_us;
}

static void start_attempt(uint32_t ip, uint16_t port) {
    s_ref_ip   = ip;
    s_ref_port = port;
    s_have_ref = true;

    s_have_prev_ghost      = false;
    s_prev_ghost_us        = 0;
    s_consecutive_timeouts = 0;
    s_next_measure_us      = now_us() + REMEASURE_INTERVAL_US;  // schedule next re-measure

    link_measurement_attempt_begin();
    send_ping(now_us());
}

// Trigger rule: start a fresh attempt on first peer discovered (peer count
// 0->1), when a different peer/endpoint appears, OR when the periodic
// re-measure timer is due (see REMEASURE_INTERVAL_US). The periodic path is
// what makes phase actually converge on a multi-homed peer: a single one-shot
// attempt can latch an unreachable advertised interface and never retry.
// Single reference peer — no multi-peer consensus.
static void check_trigger() {
    int      peer_count = link_proto_peers();
    uint32_t ip = 0;
    uint16_t port = 0;
    bool     found = false;

    for (int i = 0; i < peer_count && !found; i++) {
        if (link_proto_peer_endpoint(i, &ip, &port)) found = true;
    }

    if (!found) {
        // Reference peer (if any) has no advertised endpoint right now —
        // it either vanished or hasn't published mep4 yet. Forget it so a
        // reappearing/new peer re-triggers a fresh attempt.
        s_have_ref = false;
        return;
    }

    bool different_peer = !s_have_ref || ip != s_ref_ip || port != s_ref_port;
    bool due            = now_us() >= s_next_measure_us;   // periodic re-measure
    if ((different_peer || due) && !link_measurement_active()) {
        start_attempt(ip, port);
    }
}

void link_measurement_io_begin() {
    s_udp.stop();
    s_socket_open = s_udp.begin((uint16_t)0) != 0;

    s_have_ref = false;
    s_have_prev_ghost = false;
    s_consecutive_timeouts = 0;
    link_measurement_reset();
}

void link_measurement_io_poll() {
    if (!s_socket_open) return;

    check_trigger();

    if (!link_measurement_active()) return;

    // Drain any pending pongs.
    int pkt;
    while ((pkt = s_udp.parsePacket()) > 0) {
        uint8_t buf[128];
        int len = s_udp.read(buf, sizeof(buf));
        int64_t h_recv = now_us();
        if (len <= 0) continue;

        LinkPongFields fields;
        if (!link_measurement_parse_pong(buf, len, &fields)) continue;

        link_measurement_add_pong_samples(h_recv, &fields);
        s_consecutive_timeouts = 0;

        if (link_measurement_samples_count() >= LINK_MEASUREMENT_READY_SAMPLES) {
            link_measurement_attempt_end(true);
            return;  // attempt finished this poll; nothing left to drive
        }

        // Immediate re-ping on pong receipt — don't wait for the watchdog;
        // the real cadence is response-driven, the 50ms timer is only a
        // silence watchdog.
        s_have_prev_ghost = fields.has_ghost_time;
        s_prev_ghost_us   = fields.ghost_time_us;
        send_ping(h_recv);
    }

    // 50ms silence watchdog -> retry; 5 consecutive timeouts (250ms
    // cumulative silence since the last received pong) -> abandon, leaving
    // any previously-committed GhostXForm untouched.
    int64_t now = now_us();
    if (now - s_last_send_us >= WATCHDOG_US) {
        s_consecutive_timeouts++;
        if (s_consecutive_timeouts >= MAX_TIMEOUTS) {
            link_measurement_attempt_end(false);
        } else {
            send_ping(now);
        }
    }
}
