// Thin Arduino glue: WiFiUDP unicast ping/pong exchange against a Link peer's
// mep4 endpoint (discovered by LNK-017's link_protocol.c). All decision policy —
// peer targeting, re-measure scheduling, the transport re-origin response, the
// RX flush, the silence watchdog — lives in the pure, host-tested
// link_measurement_session.c (LNK-031); the wire format + sample/offset math is
// in link_measurement.c. This file owns only sockets, timing, and translating
// the session's actions into I/O.
#include "link_measurement_io.h"
#include "link_measurement.h"
#include "link_measurement_session.h"
#include "link_protocol.h"
#include <WiFiUdp.h>
#include "esp_timer.h"

// Independent socket from link_listener.cpp's multicast discovery conversation
// (port 20808) — a separate unicast conversation. Port 0 = OS-assigned ephemeral
// local port; we only ever talk to the peer's advertised mep4 endpoint.
static WiFiUDP    s_udp;
static bool       s_socket_open = false;
static LinkSession s_session;

// esp_timer_get_time(), not millis()/micros() — see LNK-019 for why microsecond
// precision matters specifically in this path.
static int64_t now_us() { return esp_timer_get_time(); }

// Build an IPAddress from our wire-format uint32_t (big-endian octets). The
// 4-octet ctor is used deliberately — IPAddress(uint32_t) assigns the raw word
// into a little-endian layout with no byteswap, which would mis-order the octets.
static IPAddress ip_from_be32(uint32_t ip) {
    return IPAddress((ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                      (ip >> 8) & 0xFF, ip & 0xFF);
}

static void send_ping(uint32_t ip, uint16_t port, bool has_prev, int64_t prev_ghost,
                      int64_t when_us) {
    uint8_t buf[LINK_MEASUREMENT_PING_MAX_LEN];
    int n = link_measurement_build_ping(buf, sizeof(buf), when_us, has_prev, prev_ghost);
    if (n <= 0) return;  // shouldn't happen; buffer is sized for worst case
    s_udp.beginPacket(ip_from_be32(ip), port);
    s_udp.write(buf, n);
    s_udp.endPacket();
}

// Execute one session action against real I/O.
static void run_action(const LinkSessionAct& a) {
    switch (a.type) {
        case LS_FLUSH_RX: {
            uint8_t junk[128];
            while (s_udp.parsePacket() > 0) s_udp.read(junk, sizeof(junk));
            break;
        }
        case LS_START_ATTEMPT: link_measurement_attempt_begin(); break;
        case LS_SEND_PING:     send_ping(a.ip, a.port, a.has_prev_ghost, a.prev_ghost_us,
                                         a.send_time_us); break;
        case LS_END_OK:        link_measurement_attempt_end(true);  break;
        case LS_END_FAIL:      link_measurement_attempt_end(false); break;
        case LS_RESET_XFORM:   link_measurement_reset();            break;
    }
}

static void run_all(const LinkSessionAct* acts, int n) {
    for (int i = 0; i < n; i++) run_action(acts[i]);
}

void link_measurement_io_begin() {
    s_udp.stop();
    s_socket_open = s_udp.begin((uint16_t)0) != 0;
    link_session_reset(&s_session);
    link_measurement_reset();
}

void link_measurement_io_poll() {
    if (!s_socket_open) return;

    LinkSessionAct acts[4];
    int n;

    // 1. Timeline gossip → epoch-reset detection.
    LinkTimeline tl;
    bool tl_valid = link_proto_timeline(&tl);
    n = link_session_on_timeline(&s_session, tl_valid, tl_valid ? tl.time_origin_us : 0,
                                 acts, 4);
    run_all(acts, n);

    // 2. Trigger: pick the first peer with an advertised endpoint, let the
    //    session decide whether to (re)start an attempt.
    uint32_t ip = 0; uint16_t port = 0; bool found = false;
    int peer_count = link_proto_peers();
    for (int i = 0; i < peer_count && !found; i++) {
        if (link_proto_peer_endpoint(i, &ip, &port)) found = true;
    }
    n = link_session_on_trigger(&s_session, found, ip, port, now_us(),
                                link_measurement_active(), acts, 4);
    run_all(acts, n);

    if (!link_measurement_active()) return;

    // 3. Drain pongs; the session decides commit-vs-reping per pong.
    while (s_udp.parsePacket() > 0) {
        uint8_t buf[128];
        int len = s_udp.read(buf, sizeof(buf));
        int64_t h_recv = now_us();
        if (len <= 0) continue;

        LinkPongFields fields;
        if (!link_measurement_parse_pong(buf, len, &fields)) continue;

        link_measurement_add_pong_samples(h_recv, &fields);   // side effect: sample buffer
        n = link_session_on_pong(&s_session, h_recv, fields.has_ghost_time,
                                 fields.ghost_time_us, link_measurement_samples_count(),
                                 acts, 4);
        bool ended = false;
        for (int i = 0; i < n; i++) { run_action(acts[i]); if (acts[i].type == LS_END_OK) ended = true; }
        if (ended) return;   // attempt committed this poll; nothing left to drive
    }

    // 4. Silence watchdog.
    n = link_session_on_watchdog(&s_session, now_us(), acts, 4);
    run_all(acts, n);
}
