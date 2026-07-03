#include <WiFi.h>
#include <USB.h>
#include <USBMIDI.h>
#include "config.h"
#include "app_config.h"
#include "midi_clock.h"
#include "midi_bpm.h"
#include "midi_bpm_calc.h"
#include "bpm_tracker.h"
#include "osc_sender.h"
#include "web_config.h"
#include "tempo_snapshot.h"  // ARC-001: atomic {bpm,phase,valid,quantum} read path

#define LED_PIN      48
#define LED_FLASH_MS 30

AppConfig g_config;

// Live tempo goes through the tempo_snapshot seam (ARC-001) — bpm_task publishes
// a coherent {bpm,phase,valid,quantum} that web_config.cpp reads atomically. The
// old g_current_bpm / g_current_phase / g_phase_valid globals are gone (and this
// firmware previously had NO lock on them at all).

static void bpm_task(void*) {
    uint32_t last_send_ms = 0;
    int      beat_count   = 0;
    float    pub_bpm      = MCK_DEFAULT_BPM;
    for (;;) {
        uint32_t pulses = midi_clock_pulse_count();
        bool  valid = midi_phase_valid(pulses, g_config.quantum_beats);
        float phase = valid ? midi_phase_calc(pulses, g_config.quantum_beats) : -1.0f;

        bool bar_tick = false;
        if (midi_clock_beat_flag()) {
            digitalWrite(LED_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(LED_FLASH_MS));
            digitalWrite(LED_PIN, LOW);
            if (++beat_count >= 4 * MCK_REFRESH_BARS) {
                beat_count = 0;
                bar_tick   = true;
            }
        }

        float bpm = midi_bpm_update();
        uint32_t now = (uint32_t)millis();
        bool changed = bpm > 0.0f && bpm_tracker_update(bpm);
        bool refresh = bpm > 0.0f && bar_tick;
        if ((changed || refresh) && (now - last_send_ms) >= MCK_SEND_INTERVAL_MS) {
            osc_send_bpm(bpm);
            last_send_ms = now;
            pub_bpm = bpm;
            Serial0.printf("[X32MidiClock] BPM %.2f → OSC sent%s\n", bpm, changed ? "" : " (refresh)");
        }

        // One coherent snapshot per tick (ARC-001) — readers take it atomically.
        tempo_snapshot_publish(pub_bpm, phase, valid, g_config.quantum_beats);

        vTaskDelay(pdMS_TO_TICKS(MCK_POLL_MS));
    }
}

static bool wifi_try_connect() {
    Serial0.printf("[X32MidiClock] connecting to %s\n", g_config.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000) { WiFi.disconnect(true); return false; }
        delay(500);
        Serial0.print(".");
    }
    Serial0.println();
    Serial0.printf("[X32MidiClock] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static void start_config_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("X32MidiClock-Config");
    Serial0.printf("[X32MidiClock] AP started — join 'X32MidiClock-Config' → http://%s\n",
                   WiFi.softAPIP().toString().c_str());
    web_config_ap_begin();
    for (;;) web_config_handle();  // save handler calls ESP.restart()
}

static void check_factory_reset() {
    pinMode(0, INPUT_PULLUP);
    if (digitalRead(0) != LOW) return;
    Serial0.println("[X32MidiClock] BOOT held — factory reset in 5s, release to cancel");
    unsigned long t = millis();
    while (digitalRead(0) == LOW && millis() - t < 5000) delay(100);
    if (millis() - t >= 5000) {
        config_clear();
        Serial0.println("[X32MidiClock] NVS cleared — restarting");
        ESP.restart();
    }
}

void setup() {
    Serial0.begin(115200);
    delay(500);
    Serial0.println("[X32MidiClock] booting");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    check_factory_reset();
    config_load(&g_config);
    Serial0.printf("[X32MidiClock] model=%s slot=%d mixer=%s:%d\n",
                   g_config.model == MODEL_X32 ? "X32" : "XR18",
                   g_config.fx_slot, g_config.mixer_ip,
                   config_model_port(g_config.model));

    // USB MIDI must be initialised before WiFi to ensure USB enumeration is fast
    // Device name is set in midi_clock.cpp via USBMIDI constructor
    USB.manufacturerName("X32Link");
    midi_clock_init();  // calls MidiUSB.begin() + spawns poll task
    USB.begin();

    if (!wifi_try_connect()) start_config_ap();  // never returns on AP path

    osc_sender_begin();
    midi_bpm_init();
    bpm_tracker_init(0.0f, MCK_BPM_THRESHOLD);
    xTaskCreatePinnedToCore(bpm_task, "bpm", 4096, NULL, 1, NULL, 0);
    web_config_begin();

    Serial0.print("[X32MidiClock] ready — config at http://");
    Serial0.println(WiFi.localIP());
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial0.println("[X32MidiClock] WiFi lost — reconnecting");
        WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
        while (WiFi.status() != WL_CONNECTED) { delay(500); Serial0.print("."); }
        Serial0.println();
        osc_sender_begin();
    }
    web_config_handle();
    TempoSnapshot ts; tempo_snapshot_read(&ts);
    Serial0.printf("[X32MidiClock] bpm:%.2f pulses:%lu heap:%lu\n",
                   ts.bpm,
                   (unsigned long)midi_clock_pulse_count(),
                   (unsigned long)ESP.getFreeHeap());
    delay(5000);
}
