#include "osc_sender.h"
#include "osc_out.h"
#include "app_config.h"
#include <WiFiUdp.h>
#include <Arduino.h>

extern AppConfig g_config;

static WiFiUDP s_udp;

void osc_sender_begin() {
    s_udp.begin(2390);  // ephemeral send port
}

void osc_send_bpm(float bpm) {
    uint8_t pkt[32];
    int len = osc_build_fx_delay(pkt, g_config.fx_slot, bpm_to_normalized(bpm));
    s_udp.beginPacket(g_config.mixer_ip, config_model_port(g_config.model));
    s_udp.write(pkt, (size_t)len);
    if (!s_udp.endPacket()) {
        Serial.println("[X32Link] OSC send failed");
    }
}
