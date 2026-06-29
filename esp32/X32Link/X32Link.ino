#include <WiFi.h>
#include "config.h"
#include "app_config.h"
#include "tempo_source.h"
#include "link_listener.h"  // for the loop() status counters (rx/mcast) in Link mode
#include "bpm_publisher.h"
#include "osc_sender.h"
#include "web_config.h"

#define LED_FLASH_MS 30

AppConfig g_config;

static SemaphoreHandle_t g_bpm_mutex;
volatile float           g_current_bpm = LINK_DEFAULT_BPM;

// Beat LED. ESP32-S3 Super Mini: a plain LED on GPIO48, active-HIGH (proven by
// the original MIDI firmware). Define LED_ACTIVE_LOW for an active-low board
// (e.g. XIAO), or LED_RGB for a board with an addressable WS2812.
#define LED_PIN_NUM 48

static void led_set(bool on) {
#if defined(LED_RGB)
    rgbLedWrite(LED_PIN_NUM, 0, on ? 40 : 0, 0);   // green beat
#elif defined(LED_ACTIVE_LOW)
    digitalWrite(LED_PIN_NUM, on ? LOW : HIGH);
#else
    digitalWrite(LED_PIN_NUM, on ? HIGH : LOW);    // active-high (Super Mini)
#endif
}

static void led_setup() {
#if !defined(LED_RGB)
    pinMode(LED_PIN_NUM, OUTPUT);
#endif
    led_set(false);
}

static void led_task(void*) {
    led_setup();
    uint32_t last_flash_ms = 0;
    for (;;) {
        xSemaphoreTake(g_bpm_mutex, portMAX_DELAY);
        float bpm = g_current_bpm;
        xSemaphoreGive(g_bpm_mutex);

        if (bpm > 0.0f && tempo_source_active()) {
            uint32_t beat_ms = (uint32_t)(60000.0f / bpm);
            uint32_t now = (uint32_t)millis();
            if (now - last_flash_ms >= beat_ms) {
                led_set(true);
                vTaskDelay(pdMS_TO_TICKS(LED_FLASH_MS));
                led_set(false);
                last_flash_ms = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void bpm_task(void*) {
    bpm_publisher_init(tempo_source_threshold(), LINK_SEND_INTERVAL_MS, LINK_REFRESH_BARS);
    for (;;) {
        tempo_source_poll();
        PublishDecision d = bpm_publisher_step(tempo_source_bpm(),
                                               tempo_source_active(),
                                               (uint32_t)millis());
        if (d.send) {
            osc_send_bpm(d.bpm);
            xSemaphoreTake(g_bpm_mutex, portMAX_DELAY);
            g_current_bpm = d.bpm;
            xSemaphoreGive(g_bpm_mutex);
            Serial.printf("[X32Sync] BPM %.2f → OSC sent%s\n", d.bpm, d.refresh ? " (refresh)" : "");
        }
        vTaskDelay(pdMS_TO_TICKS(tempo_source_poll_ms()));
    }
}

// Returns true if connected within 30 s, false on timeout.
static bool wifi_try_connect() {
    Serial.printf("[X32Link] connecting to %s\n", g_config.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000) {
            WiFi.disconnect(true);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    WiFi.setSleep(false);  // modem power-save drops buffered multicast — keep radio awake
    Serial.println();
    Serial.printf("[X32Link] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// Blocking reconnect used by loop() — no timeout (we have nowhere else to go).
static void wifi_connect() {
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // Already connected (called right after status check fails then recovers),
    // or fall through from try_connect. Reinit if truly disconnected:
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(g_config.wifi_ssid, g_config.wifi_pass);
        while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    }
    Serial.println();
    Serial.printf("[X32Link] IP: %s\n", WiFi.localIP().toString().c_str());
}

static void start_config_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("X32Link-Config");
    Serial.printf("[X32Link] WiFi failed — AP started\n");
    Serial.printf("[X32Link] join 'X32Link-Config' → http://%s\n",
                  WiFi.softAPIP().toString().c_str());
    web_config_ap_begin();
    for (;;) web_config_handle();  // save handler calls ESP.restart()
}

static void check_factory_reset() {
    pinMode(0, INPUT_PULLUP);
    if (digitalRead(0) != LOW) return;
    Serial.println("[X32Link] BOOT held — factory reset in 5s, release to cancel");
    unsigned long t = millis();
    while (digitalRead(0) == LOW && millis() - t < 5000) delay(100);
    if (millis() - t >= 5000) {
        config_clear();
        Serial.println("[X32Link] NVS cleared — restarting");
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("[X32Link] booting");

    check_factory_reset();
    config_load(&g_config);
    Serial.printf("[X32Sync] src=%s model=%s slot=%d mixer=%s:%d\n",
                  g_config.input_source == TEMPO_SRC_MIDI ? "MIDI" : "LINK",
                  g_config.model == MODEL_X32 ? "X32" : "XR18",
                  g_config.fx_slot, g_config.mixer_ip,
                  config_model_port(g_config.model));

    tempo_source_select(g_config.input_source);
    tempo_source_pre_net();  // USB MIDI enumerates here (before WiFi); no-op for Link

    if (!wifi_try_connect()) start_config_ap();  // never returns on AP path

    tempo_source_begin();    // Link joins multicast here; no-op for MIDI
    osc_sender_begin();
    g_bpm_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(bpm_task, "bpm", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(led_task, "led", 2048, NULL, 1, NULL, 0);
    web_config_begin();

    Serial.print("[X32Link] ready — config at http://");
    Serial.println(WiFi.localIP());
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[X32Sync] WiFi lost — reconnecting");
        wifi_connect();
        tempo_source_begin();
        osc_sender_begin();
    }
    web_config_handle();  // service the web server every ~20 ms so /status polling is live

    static uint32_t last_log = 0;
    uint32_t now = millis();
    if (now - last_log >= 5000) {
        last_log = now;
        Serial.printf("[X32Link] ip:%s mcast:%d rx:%lu peers:%d bpm:%.2f heap:%lu\n",
                      WiFi.localIP().toString().c_str(),
                      link_listener_mcast_ok() ? 1 : 0,
                      (unsigned long)link_listener_rx_count(),
                      link_listener_peers(), g_current_bpm,
                      (unsigned long)ESP.getFreeHeap());
    }
    delay(20);
}
