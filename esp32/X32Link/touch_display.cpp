#include "touch_display.h"

// Whole module compiles only for the touch-LCD board — keeps LovyanGFX out of
// screenless builds (Super Mini / XIAO / QT Py). X32Link.ino calls these under
// the same guard, so no undefined-reference when it's off.
#ifdef HAS_TOUCH_DISPLAY

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>            // WiFi.localIP() for the on-screen config address
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "axs5106l.h"
#include "touch_ui.h"        // LNK-015: pure UI logic (hit-test, formatting, taps)
#include "tempo_source.h"    // tempo_source_active() (side-effect-free read)

// Shared globals maintained by X32Link.ino — same pattern web_config.cpp uses.
// (Do NOT call tempo_source_beat() here — it self-clears and led_task owns it.)
extern AppConfig      g_config;
extern volatile float g_current_bpm;
extern volatile float g_current_phase;   // -1.0f when no valid phase
extern volatile bool  g_phase_valid;

// ---- Panel (validated on glass, LNK-014 Progress) --------------------------
// JD9853 via LovyanGFX Panel_ST7789. Pins: SCK38 MOSI39 DC45 CS21 RST40, BL46.
// 172x320 visible in 240x320 controller RAM -> offset_x=34. invert=false,
// setRotation(4). Backlight driven manually (GPIO46 HIGH), not via Light_PWM.
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
public:
    LGFX() {
        { auto c = _bus.config();
          c.spi_host = SPI2_HOST; c.spi_mode = 0;
          c.freq_write = 40000000; c.freq_read = 16000000;
          c.pin_sclk = 38; c.pin_mosi = 39; c.pin_miso = -1; c.pin_dc = 45;
          c.dma_channel = SPI_DMA_CH_AUTO;
          _bus.config(c); _panel.setBus(&_bus); }
        { auto c = _panel.config();
          c.pin_cs = 21; c.pin_rst = 40; c.pin_busy = -1;
          c.memory_width = 240; c.memory_height = 320;
          c.panel_width = 172;  c.panel_height = 320;
          c.offset_x = 34; c.offset_y = 0;
          c.invert = false; c.rgb_order = false;
          _panel.config(c); }
        setPanel(&_panel);
    }
};

#define TP_SDA 42
#define TP_SCL 41
#define TP_RST 47
#define LCD_BL 46

static LGFX     s_lcd;
static uint32_t s_last_print_ms = 0;

static void backlight_on(void) {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
}

static void draw_splash(void) {
    s_lcd.fillScreen(TFT_BLACK);
    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(3);
    s_lcd.setCursor(8, 40);  s_lcd.println("X32");
    s_lcd.setCursor(8, 80);  s_lcd.println("SYNC");
    s_lcd.setTextSize(1);
    s_lcd.setCursor(8, 130); s_lcd.println("booting...");
}

// LNK-015 screen state (settings/keyboard screens land in later tasks).
enum ui_screen_t { SCREEN_STATUS, SCREEN_SETTINGS, SCREEN_KEYBOARD };
static ui_screen_t s_screen = SCREEN_STATUS;

// Status-screen phase wheel geometry (portrait 172x320, setRotation(4)).
#define WHEEL_CX 86
#define WHEEL_CY 252
#define WHEEL_R  44

// touch_display_update() repaint caches (reset by draw_status()).
static char     s_bpm_shown[8] = "";
static char     s_ip_shown[16] = "";
static int      s_prev_mx = -1, s_prev_my = -1;
static bool     s_wheel_valid_shown = false;
static uint32_t s_last_update_ms = 0;

// LNK-015 Task 7/8: status screen. draw_status() paints the static frame;
// touch_display_update() (Task 8) refreshes the live BPM number + phase wheel.
static void draw_status(void) {
    s_lcd.fillScreen(TFT_BLACK);

    s_lcd.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    s_lcd.setTextSize(1);
    s_lcd.setCursor(8, 84);
    s_lcd.print("BPM");

    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(8, 128);
    s_lcd.print(g_config.input_source == 0 ? "Ableton Link" : "USB MIDI");

    // OSC target (the mixer we send to). Device's own IP is drawn live by
    // touch_display_update() (below) since WiFi isn't up yet at begin() time.
    s_lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    s_lcd.setTextSize(1);
    s_lcd.setCursor(8, 178);
    s_lcd.printf("osc %s /fx%d", g_config.mixer_ip, g_config.fx_slot);

    s_lcd.drawCircle(WHEEL_CX, WHEEL_CY, WHEEL_R, TFT_DARKGREEN);

    // force touch_display_update() to repaint the dynamic parts next tick
    s_bpm_shown[0] = '\0';
    s_ip_shown[0]  = '\0';
    s_prev_mx = -1;
    s_wheel_valid_shown = false;
}

void touch_display_update(void) {
    if (s_screen != SCREEN_STATUS) return;
    uint32_t now = millis();
    if (now - s_last_update_ms < 100) return;   // ~10 Hz is plenty for status
    s_last_update_ms = now;

    bool active = tempo_source_active();

    // BPM number — repaint only when the shown text changes (no flicker).
    char bpm[8];
    if (active) ui_bpm_str(bpm, sizeof bpm, g_current_bpm);
    else        snprintf(bpm, sizeof bpm, "--.-");   // no live signal
    if (strcmp(bpm, s_bpm_shown) != 0) {
        s_lcd.fillRect(8, 44, 160, 34, TFT_BLACK);
        s_lcd.setTextColor(active ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        s_lcd.setTextSize(4);
        s_lcd.setCursor(8, 44);
        s_lcd.print(bpm);
        strncpy(s_bpm_shown, bpm, sizeof s_bpm_shown - 1);
        s_bpm_shown[sizeof s_bpm_shown - 1] = '\0';
    }

    // Device IP (where to browse for config) — reads 0.0.0.0 until WiFi is up.
    IPAddress ip = WiFi.localIP();
    char ipstr[16];
    snprintf(ipstr, sizeof ipstr, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    if (strcmp(ipstr, s_ip_shown) != 0) {
        s_lcd.fillRect(8, 158, 160, 12, TFT_BLACK);
        s_lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        s_lcd.setTextSize(1);
        s_lcd.setCursor(8, 158);
        s_lcd.printf("web %s", ipstr);
        strncpy(s_ip_shown, ipstr, sizeof s_ip_shown - 1);
        s_ip_shown[sizeof s_ip_shown - 1] = '\0';
    }

    // Phase wheel: sweeping marker once phase is valid, else "syncing".
    bool valid = g_phase_valid && g_current_phase >= 0.0f;
    if (valid != s_wheel_valid_shown) {            // state change: clear interior
        s_lcd.fillCircle(WHEEL_CX, WHEEL_CY, WHEEL_R - 1, TFT_BLACK);
        s_lcd.drawCircle(WHEEL_CX, WHEEL_CY, WHEEL_R, TFT_DARKGREEN);
        s_prev_mx = -1;
        s_wheel_valid_shown = valid;
    }
    if (valid) {
        float ang = ui_phase_angle(g_current_phase, (float)g_config.quantum_beats);
        float rad = (ang - 90.0f) * 0.01745329f;   // deg->rad; 0deg at top
        int mx = WHEEL_CX + (int)(cosf(rad) * (WHEEL_R - 8));
        int my = WHEEL_CY + (int)(sinf(rad) * (WHEEL_R - 8));
        if (mx != s_prev_mx || my != s_prev_my) {
            if (s_prev_mx >= 0) s_lcd.fillCircle(s_prev_mx, s_prev_my, 7, TFT_BLACK);
            s_lcd.drawCircle(WHEEL_CX, WHEEL_CY, WHEEL_R, TFT_DARKGREEN);  // repair outline
            s_lcd.fillCircle(mx, my, 6, TFT_GREEN);
            s_prev_mx = mx; s_prev_my = my;
        }
    } else {
        s_lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
        s_lcd.setTextSize(1);
        s_lcd.setCursor(WHEEL_CX - 21, WHEEL_CY - 3);
        s_lcd.print("syncing");
    }
}

static bool axs_read(axs_touch_t *t) {
    Wire.beginTransmission(AXS5106L_I2C_ADDR);
    Wire.write(AXS5106L_TOUCH_REG);
    if (Wire.endTransmission(true) != 0) return false;   // STOP, then a fresh read
    uint8_t buf[AXS5106L_REPORT_LEN];
    int n = Wire.requestFrom((int)AXS5106L_I2C_ADDR, (int)AXS5106L_REPORT_LEN);
    if (n < AXS5106L_REPORT_LEN) return false;
    for (int i = 0; i < AXS5106L_REPORT_LEN; i++) buf[i] = Wire.read();
    return axs5106l_parse(buf, sizeof(buf), t) == 0;
}

void touch_display_begin(void) {
    s_lcd.init();
    s_lcd.setRotation(4);
    backlight_on();            // manual GPIO46 HIGH — LovyanGFX Light_PWM/LEDC
                               // failed to drive the backlight in the full build
    draw_splash();

    pinMode(TP_RST, OUTPUT);
    digitalWrite(TP_RST, LOW);  delay(20);
    digitalWrite(TP_RST, HIGH); delay(80);
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setClock(400000);
    Serial.printf("[X32Link] touch_display up (%dx%d)\n", (int)s_lcd.width(), (int)s_lcd.height());

    draw_status();             // LNK-015 Task 7: show the status screen (BPM --.- until tempo)
}

void touch_display_tick(void) {
    // LNK-014 scope: poll the controller and echo raw coordinates to Serial on
    // contact. No on-screen touch UI here — drawing touch state on the panel
    // (and the raw->screen coordinate mapping, e.g. the Y flip) is LNK-015's
    // job. The static boot splash stays on screen.
    uint32_t now = millis();
    if (now - s_last_print_ms < 150) return;
    s_last_print_ms = now;

    axs_touch_t t;
    if (!axs_read(&t)) return;                 // I2C gap / no data: stay quiet
    if (t.points_len)
        Serial.printf("[td] n=%u x=%d y=%d\n", t.count, t.points[0].x, t.points[0].y);
}

#endif // HAS_TOUCH_DISPLAY
