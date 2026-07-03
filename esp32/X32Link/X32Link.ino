#include <WiFi.h>
#include "config.h"
#include "app_config.h"
#include "tempo_source.h"
#include "led_phase.h"
#include "wifi_down_blink.h"
#include "link_listener.h"  // for the loop() status counters (rx/mcast) in Link mode
#include "bpm_publisher.h"
#include "osc_sender.h"
#include "web_config.h"
#ifdef HAS_TOUCH_DISPLAY
#include "touch_display.h"   // LNK-014: 1.47" JD9853 LCD + AXS5106L touch
#endif

#define LED_FLASH_MS 30

// WiFi-down status indicator (LNK-023): occasional triple-blink, distinct
// from the single per-beat flash, while WiFi.status() != WL_CONNECTED
// (covers both the initial connect attempt and parked-in-AP-fallback).
#define WIFI_DOWN_BLINK_ON_MS        60
#define WIFI_DOWN_BLINK_GAP_MS       80
#define WIFI_DOWN_BLINK_COUNT        3
#define WIFI_DOWN_BLINK_INTERVAL_MS  4000

AppConfig g_config;

static SemaphoreHandle_t g_bpm_mutex;
volatile float           g_current_bpm = LINK_DEFAULT_BPM;

// True once start_config_ap() has parked us in AP fallback (terminal until
// the user reconfigures via the captive portal — handle_save() reboots).
// Checked by loop() so its STA-reconnect block doesn't fight AP mode. See
// LNK-023.
static bool s_ap_mode = false;

// Phase snapshot for the web beat dot (LNK-022) — read by web_config.cpp's
// handle_status(), refreshed by bpm_task() below from tempo_source_phase()/
// tempo_source_phase_valid(). See the extern decl comment in web_config.cpp
// for why this goes through a global instead of a direct call (that file is
// symlinked into X32MidiClock, which has no tempo_source.h).
volatile float g_current_phase = -1.0f;
volatile bool  g_phase_valid   = false;

// Beat LED. ESP32-S3 Super Mini: a plain LED on GPIO48, active-HIGH (proven by
// the original MIDI firmware). Define LED_ACTIVE_LOW for an active-low board
// (e.g. XIAO), or LED_RGB for a board with an addressable WS2812. Define
// BOARD_QTPY_ESP32S3 for the Adafruit QT Py ESP32-S3: its beat LED is an
// onboard NeoPixel on GPIO39 (implies LED_RGB) whose level shifter needs
// GPIO38 driven HIGH before it'll light — LED_PWR_PIN_NUM handles that.
// Define BOARD_WAVESHARE_S3_TOUCH_LCD_147 for the Waveshare ESP32-S3-Touch-
// LCD-1.47 (the Type-C touch board — NOT the non-touch USB-A dongle, which is
// a different PCB). This board has NO addressable WS2812: its only LEDs are a
// charger-driven charge-status LED and an always-on power LED, neither under
// MCU control (verified against the Waveshare schematic GPIO map). GPIO38 there
// is LCD_SCL, not an LED — driving it as a beat LED silently toggles the display
// clock, so don't. This board runs headless (LED_NONE): the beat is shown by the
// web /status beat dot (LNK-022), and the 1.47" LCD (backlight on GPIO46) is left
// untouched for a real display UI later rather than hijacked as a strobe.
#if defined(BOARD_QTPY_ESP32S3)
    #define LED_RGB
    #define LED_PIN_NUM 39
    #define LED_PWR_PIN_NUM 38
#elif defined(BOARD_WAVESHARE_S3_TOUCH_LCD_147)
    #define LED_NONE                // no MCU-controlled LED; beat shown via web /status
#else
    #define LED_PIN_NUM 48
#endif

static void led_set(bool on) {
#if defined(LED_NONE)
    (void)on;                                      // headless: beat via web /status dot
#elif defined(LED_RGB)
    rgbLedWrite(LED_PIN_NUM, 0, on ? 40 : 0, 0);   // green beat
#elif defined(LED_ACTIVE_LOW)
    digitalWrite(LED_PIN_NUM, on ? LOW : HIGH);
#else
    digitalWrite(LED_PIN_NUM, on ? HIGH : LOW);    // active-high (Super Mini)
#endif
}

static void led_setup() {
#if defined(LED_PWR_PIN_NUM)
    pinMode(LED_PWR_PIN_NUM, OUTPUT);
    digitalWrite(LED_PWR_PIN_NUM, HIGH);
#endif
#if !defined(LED_RGB) && !defined(LED_NONE)
    pinMode(LED_PIN_NUM, OUTPUT);
#endif
    led_set(false);
}

// Phase-locked beat LED (LNK-021). Source-agnostic: only calls the
// tempo_source_* seam, same as the rest of this file — Link vs MIDI is
// invisible here. Once tempo_source_phase_valid() is true, flashes on
// real phase-zero crossings (tempo_source_phase(1.0f), per-beat quantum —
// not the bar-level quantum the touch UI's phase wheel uses). Before
// that's available (sync gap: mid-measurement on Link, or
// tempo_source_phase_valid()'s underlying g_config.quantum_beats-sized
// bar not yet fully observed on MIDI — see tempo_source.cpp), falls back
// to tempo_source_beat()'s self-clearing per-beat flag so the LED never
// goes dark while waiting on phase. Polls every 5ms, so wrap detection has
// up to ~5ms of slop — within tolerance for a "looks synced" LED.
static void led_task(void*) {
    led_setup();
    float prev_phase = -1.0f;       // -1.0f: no reading since (re)sync; seed-only
    uint32_t last_wifi_blink_ms = 0;
    for (;;) {
        if (tempo_source_active()) {
            bool valid = tempo_source_phase_valid();
            if (valid) {
                float phase = tempo_source_phase(1.0f);
                if (led_phase_should_flash(prev_phase, phase, valid)) {
                    led_set(true);
                    vTaskDelay(pdMS_TO_TICKS(LED_FLASH_MS));
                    led_set(false);
                }
                prev_phase = phase;
            } else {
                // Not synced yet — fall back to the per-beat flag so the
                // LED doesn't go dark during the sync gap. Re-seed
                // prev_phase so the invalid->valid transition's first
                // phase reading is treated as a fresh seed, not compared
                // against a stale value (no double-flash/stutter).
                prev_phase = -1.0f;
                if (tempo_source_beat()) {
                    led_set(true);
                    vTaskDelay(pdMS_TO_TICKS(LED_FLASH_MS));
                    led_set(false);
                }
            }
        } else {
            prev_phase = -1.0f;  // no source: re-seed for whenever it returns
        }

        // WiFi-down triple-blink (LNK-023). Can visually collide with the
        // beat-flash above (e.g. MIDI clock running while WiFi is down —
        // the exact scenario found live); accepted rather than building
        // priority/queueing — 1x30ms vs 3x60ms read as visually distinct.
        // Blocking ~420ms here is fine: led_task is low-priority/dedicated.
        uint32_t now_ms = (uint32_t)millis();
        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        if (wifi_down_blink_due(now_ms, last_wifi_blink_ms,
                                 WIFI_DOWN_BLINK_INTERVAL_MS, wifi_connected)) {
            last_wifi_blink_ms = now_ms;
            for (int i = 0; i < WIFI_DOWN_BLINK_COUNT; i++) {
                led_set(true);
                vTaskDelay(pdMS_TO_TICKS(WIFI_DOWN_BLINK_ON_MS));
                led_set(false);
                vTaskDelay(pdMS_TO_TICKS(WIFI_DOWN_BLINK_GAP_MS));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void bpm_task(void*) {
    bpm_publisher_init(tempo_source_threshold(), LINK_SEND_INTERVAL_MS, LINK_REFRESH_BARS);
    for (;;) {
        tempo_source_poll();

        // Refresh the web status phase snapshot every tick (not gated on
        // d.send below — phase moves continuously, unlike the
        // threshold/refresh-gated bpm publish decision).
        g_phase_valid   = tempo_source_phase_valid();
        g_current_phase = g_phase_valid ? tempo_source_phase((float)g_config.quantum_beats) : -1.0f;

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

// Non-blocking (LNK-023): configures AP + starts the captive-portal web
// server, then returns — does NOT loop. loop() already calls
// web_config_handle() every iteration, and that function is already
// AP/STA-agnostic (services the captive DNS only when s_captive, which
// web_config_ap_begin() sets), so no separate service loop is needed here.
// Sets s_ap_mode so loop()'s STA-reconnect block backs off. AP mode is
// terminal until the user reconfigures via the captive portal — handle_save()
// calls ESP.restart() — retrying STA without a reboot is explicitly out of
// scope (see ticket Notes).
static void start_config_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("X32Link-Config");
    s_ap_mode = true;
    Serial.printf("[X32Link] WiFi failed — AP started\n");
    Serial.printf("[X32Link] join 'X32Link-Config' → http://%s\n",
                  WiFi.softAPIP().toString().c_str());
    web_config_ap_begin();
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

#ifdef HAS_TOUCH_DISPLAY
    touch_display_begin();   // splash up early, before the (up-to-30s) WiFi connect
#endif

    // LNK-023: bpm_task/led_task created unconditionally, before WiFi is
    // even attempted — tempo/LED processing (incl. USB MIDI clock, which
    // needs no network at all) must not depend on WiFi succeeding.
    // tempo_source_begin() (Link's multicast join) happens after, only on
    // the STA-connected path — see the wifi_ok branch below.
    g_bpm_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(bpm_task, "bpm", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(led_task, "led", 2048, NULL, 1, NULL, 0);

    bool wifi_ok = wifi_try_connect();
    if (wifi_ok) {
        tempo_source_begin();    // Link joins multicast here; no-op for MIDI
        osc_sender_begin();
        web_config_begin();
        Serial.print("[X32Link] ready — config at http://");
        Serial.println(WiFi.localIP());
    } else {
        start_config_ap();       // non-blocking — configures AP + web server, returns
    }
}

void loop() {
#ifdef HAS_TOUCH_DISPLAY
    touch_display_tick();    // LNK-014: poll AXS5106L, echo coords to LCD + Serial
    touch_display_update();  // LNK-015: refresh status screen live fields (BPM, phase wheel)
#endif
    // Gated on !s_ap_mode (LNK-023): once parked in AP fallback, WiFi.status()
    // is never WL_CONNECTED (we're in WIFI_AP mode, not WIFI_STA) — without
    // this gate this block would call wifi_connect() (STA reconnect) every
    // iteration and fight the AP radio config. AP mode is terminal until a
    // reboot (handle_save()), so this intentionally never re-enters STA on
    // its own — see start_config_ap()'s comment / ticket Notes.
    if (!s_ap_mode && WiFi.status() != WL_CONNECTED) {
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
