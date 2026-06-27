#include "link_listener.h"
#include "link_protocol.h"
#include <WiFiUdp.h>

static const IPAddress LINK_MCAST(224, 76, 78, 75);
static const uint16_t  LINK_PORT = 20808;

static WiFiUDP s_udp;
static volatile uint32_t s_rx_count = 0;  // raw datagrams seen on the Link port
static volatile bool     s_mcast_ok = false;

// link_proto_millis() weak symbol — provide Arduino millis() on device
extern "C" uint32_t link_proto_millis(void) { return millis(); }

void link_listener_begin() {
    s_udp.stop();
    link_proto_reset();
    s_mcast_ok = s_udp.beginMulticast(LINK_MCAST, LINK_PORT) != 0;
}

void link_listener_poll() {
    int pkt;
    while ((pkt = s_udp.parsePacket()) > 0) {
        s_rx_count++;
        uint8_t buf[512];
        int n = s_udp.read(buf, sizeof(buf));
        if (n > 0) link_proto_parse(buf, n);
    }
}

uint32_t link_listener_rx_count() { return s_rx_count; }
bool     link_listener_mcast_ok() { return s_mcast_ok; }

void link_listener_tick()    { link_proto_tick(); }

double link_listener_bpm()   { return link_proto_bpm(); }
int    link_listener_peers() { return link_proto_peers(); }
