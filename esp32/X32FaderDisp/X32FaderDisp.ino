#include <WiFi.h>
#include "config.h"
#include "app_config.h"
#include "osc_listener.h"
#include "fader_disp.h"
#include "web_config.h"

AppConfig      g_config;
volatile float g_current_bpm = 0.0f;   // unused here; satisfies the shared web_config

// osc_listener callback — log every message, then drive the scribble display.
static void on_osc(const osc_msg_t *m) {
    Serial.printf("[FDR] %s ,%s (%d)\n", m->address, m->tags ? m->tags : "", m->n_args);
    fader_disp_handle(m);
}

static bool wifi_try_connect() {
    Serial.printf("[FDR] connecting to %s\n", g_config.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000) { WiFi.disconnect(true); return false; }
        delay(500); Serial.print(".");
    }
    WiFi.setSleep(false);
    Serial.printf("\n[FDR] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static void wifi_connect() {
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\n[FDR] IP: %s\n", WiFi.localIP().toString().c_str());
}

static void start_config_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("X32FaderDisp-Config");
    Serial.printf("[FDR] WiFi failed — AP at http://%s\n", WiFi.softAPIP().toString().c_str());
    web_config_ap_begin();
    for (;;) web_config_handle();   // save handler reboots
}

static void check_factory_reset() {
    pinMode(0, INPUT_PULLUP);
    if (digitalRead(0) != LOW) return;
    unsigned long t = millis();
    while (digitalRead(0) == LOW && millis() - t < 5000) delay(100);
    if (millis() - t >= 5000) { config_clear(); ESP.restart(); }
}

static void start_listening() {
    int port = config_model_port(g_config.model);
    osc_listener_begin(g_config.mixer_ip, port, on_osc);
    fader_disp_begin(FDR_CHAN_COUNT);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("[FDR] booting");

    check_factory_reset();
    config_load(&g_config);
    Serial.printf("[FDR] model=%s mixer=%s:%d\n",
                  g_config.model == MODEL_X32 ? "X32" : "XR18",
                  g_config.mixer_ip, config_model_port(g_config.model));

    if (!wifi_try_connect()) start_config_ap();   // never returns on AP path
    Serial.println("[FDR] connected");

    start_listening();
    Serial.println("[FDR] handshake OK");
    web_config_begin();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FDR] WiFi lost — reconnecting");
        wifi_connect();
        start_listening();
    }
    osc_listener_poll();
    web_config_handle();

    static uint32_t last_log = 0;
    uint32_t now = millis();
    if (now - last_log >= 5000) {
        last_log = now;
        Serial.printf("[FDR] ip:%s heap:%lu\n",
                      WiFi.localIP().toString().c_str(),
                      (unsigned long)ESP.getFreeHeap());
    }
    delay(5);
}
