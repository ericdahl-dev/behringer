#include "osc_listener.h"
#include <WiFiUdp.h>
#include <Arduino.h>
#include <string.h>

static WiFiUDP          s_udp;
static IPAddress        s_mixer;
static uint16_t         s_port;
static osc_on_message_t s_cb;
static uint32_t         s_last_xremote;

// Mixer keeps an xremote client live for ~11 s; renew well inside that.
static const uint32_t XREMOTE_RENEW_MS = 8000;

// Build a no-arg OSC message: just the address, NUL-terminated, 4-byte padded.
static void send_addr(const char *addr) {
    uint8_t b[32];
    int n = (int)strlen(addr) + 1;
    if (n > (int)sizeof(b)) return;
    memcpy(b, addr, n);
    while (n % 4) b[n++] = 0;
    s_udp.beginPacket(s_mixer, s_port);
    s_udp.write(b, n);
    s_udp.endPacket();
}

void osc_listener_begin(const char *mixer_ip, int port, osc_on_message_t cb) {
    s_mixer.fromString(mixer_ip);
    s_port = (uint16_t)port;
    s_cb   = cb;
    s_udp.begin(0);          // ephemeral local port — replies come back here
    send_addr("/xinfo");
    send_addr("/xremote");
    s_last_xremote = millis();
}

void osc_listener_poll(void) {
    uint32_t now = (uint32_t)millis();
    if (now - s_last_xremote >= XREMOTE_RENEW_MS) {
        send_addr("/xremote");
        s_last_xremote = now;
    }
    while (s_udp.parsePacket() > 0) {
        uint8_t buf[512];
        int n = s_udp.read(buf, sizeof(buf));
        if (n > 0 && s_cb) {
            osc_msg_t m;
            if (osc_in_parse(buf, n, &m) == 0) s_cb(&m);
        }
    }
}

void osc_listener_end(void) {
    s_udp.stop();
}
