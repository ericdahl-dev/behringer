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
#include "tempo_snapshot.h"  // ARC-001: atomic {bpm,phase,valid,quantum}

extern AppConfig g_config;
// Live tempo via tempo_snapshot_read() — one coherent read, no torn fields.
// (Do NOT call tempo_source_beat() here — it self-clears and led_task owns it.)

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

// Tap targets (screen coords after the rotation-4 map). Task 9: nav only.
static const ui_rect_t STATUS_SET_RECT     = {116, 4, 52, 28};   // "SET" on status
static const ui_rect_t SETTINGS_BACK_RECT  = {8, 4, 40, 28};     // "<" top-left
static const ui_rect_t SETTINGS_WRITE_RECT = {8, 272, 156, 40};  // Write & Reboot

#define ACT_BACK       100  // tap-table action ids (outside the UI_F_* range)
#define ACT_WRITE      101
#define ACT_IP_EDIT    102  // settings: open the IP keypad
#define ACT_KBD_BS     110  // keypad: backspace / OK / cancel
#define ACT_KBD_OK     111
#define ACT_KBD_CANCEL 112
// (digit/'.' keys carry their own ASCII char as the field id, all < 100)

// Settings edit state (Task 10): a working copy of g_config that taps mutate,
// plus the current screen's interactive rect table (rebuilt each draw). Task 12
// adds s_kbd_buf, the IP being typed on the keypad screen.
static AppConfig s_working;
static char      s_kbd_buf[16];
static ui_rect_t s_set_rects[24];
static int       s_set_fields[24];
static int       s_set_n = 0;

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

    // "SET" button (top-right) opens the settings screen (Task 9 nav).
    s_lcd.drawRoundRect(STATUS_SET_RECT.x, STATUS_SET_RECT.y,
                        STATUS_SET_RECT.w, STATUS_SET_RECT.h, 4, TFT_DARKGREY);
    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(1);
    s_lcd.setCursor(STATUS_SET_RECT.x + 14, STATUS_SET_RECT.y + 10);
    s_lcd.print("SET");

    // force touch_display_update() to repaint the dynamic parts next tick
    s_bpm_shown[0] = '\0';
    s_ip_shown[0]  = '\0';
    s_prev_mx = -1;
    s_wheel_valid_shown = false;
}

static ui_rect_t rect(int x, int y, int w, int h) {
    ui_rect_t r; r.x = (int16_t)x; r.y = (int16_t)y; r.w = (int16_t)w; r.h = (int16_t)h;
    return r;
}

static void set_add(ui_rect_t r, int field) {
    if (s_set_n < (int)(sizeof s_set_rects / sizeof s_set_rects[0])) {
        s_set_rects[s_set_n] = r;
        s_set_fields[s_set_n] = field;
        s_set_n++;
    }
}

// A labeled pill: filled when selected, outlined otherwise. Registers its tap rect.
static void set_seg(ui_rect_t r, const char *label, bool sel, int field) {
    if (sel) {
        s_lcd.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_DARKGREEN);
        s_lcd.setTextColor(TFT_BLACK, TFT_DARKGREEN);
    } else {
        s_lcd.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_DARKGREY);
        s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    s_lcd.setTextSize(2);
    int tw = (int)strlen(label) * 12;                 // ~12 px/char at size 2
    s_lcd.setCursor(r.x + (r.w - tw) / 2, r.y + (r.h - 16) / 2);
    s_lcd.print(label);
    set_add(r, field);
}

static void set_label(int y, const char *s) {
    s_lcd.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    s_lcd.setTextSize(1);
    s_lcd.setCursor(8, y);
    s_lcd.print(s);
}

// Task 10: full settings screen, drawn from s_working. Taps mutate s_working via
// ui_apply_settings_tap() + redraw. Persistence is Task 11 (Write & Reboot).
static void draw_settings(void) {
    s_lcd.fillScreen(TFT_BLACK);
    s_set_n = 0;

    // back "<" (top-left) + title
    s_lcd.drawRoundRect(SETTINGS_BACK_RECT.x, SETTINGS_BACK_RECT.y,
                        SETTINGS_BACK_RECT.w, SETTINGS_BACK_RECT.h, 4, TFT_GREEN);
    s_lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(SETTINGS_BACK_RECT.x + 13, SETTINGS_BACK_RECT.y + 7);
    s_lcd.print("<");
    set_add(SETTINGS_BACK_RECT, ACT_BACK);

    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(58, 8);
    s_lcd.print("SETTINGS");

    set_label(32, "SOURCE");
    set_seg(rect(8,  42, 76, 26), "LINK", s_working.input_source == 0, UI_F_SRC_LINK);
    set_seg(rect(88, 42, 76, 26), "MIDI", s_working.input_source == 1, UI_F_SRC_MIDI);

    set_label(72, "MODEL");
    set_seg(rect(8,  82, 76, 26), "XR",  s_working.model == MODEL_XR18, UI_F_MODEL_XR);
    set_seg(rect(88, 82, 76, 26), "X32", s_working.model == MODEL_X32,  UI_F_MODEL_X32);

    set_label(110, "FX SLOT");
    int smax = config_model_slot_max(s_working.model);
    for (int s = 1; s <= smax; s++) {
        int col = (s - 1) % 4, row = (s - 1) / 4;
        char num[3]; snprintf(num, sizeof num, "%d", s);
        set_seg(rect(8 + col * 40, 120 + row * 30, 34, 26),
                num, s_working.fx_slot == s, UI_F_SLOT_1 + (s - 1));
    }

    set_label(182, "QUANTUM (beats/bar)");
    set_seg(rect(8, 192, 34, 28), "-", false, UI_F_QUANTUM_DEC);
    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(3);
    s_lcd.setCursor(66, 194);
    s_lcd.printf("%d", s_working.quantum_beats);
    set_seg(rect(124, 192, 34, 28), "+", false, UI_F_QUANTUM_INC);

    set_label(226, "MIXER IP (tap to edit)");
    s_lcd.drawRoundRect(8, 236, 156, 28, 4, TFT_DARKGREY);
    s_lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(14, 241);
    s_lcd.print(s_working.mixer_ip);
    set_add(rect(8, 236, 156, 28), ACT_IP_EDIT);

    // Write & Reboot (Task 11): persist s_working + restart, like handle_save().
    s_lcd.fillRoundRect(SETTINGS_WRITE_RECT.x, SETTINGS_WRITE_RECT.y,
                        SETTINGS_WRITE_RECT.w, SETTINGS_WRITE_RECT.h, 6, TFT_DARKGREEN);
    s_lcd.setTextColor(TFT_BLACK, TFT_DARKGREEN);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(SETTINGS_WRITE_RECT.x + 12, SETTINGS_WRITE_RECT.y + 12);
    s_lcd.print("SAVE+REBOOT");
    set_add(SETTINGS_WRITE_RECT, ACT_WRITE);
}

static void enter_settings(void) {
    s_working = g_config;          // edit a copy; commit is Task 11 (Write & Reboot)
    s_screen = SCREEN_SETTINGS;
    draw_settings();
}

// Task 12: numeric keypad for the mixer IP. Alpha entry (SSID/password) stays on
// the web captive portal — see LNK-015 Notes. Digits/'.' carry their ASCII char
// as the tap field id; backspace/OK/cancel use ACT_KBD_*.
static void redraw_kbd_buf(void) {
    s_lcd.fillRect(8, 40, 156, 18, TFT_BLACK);
    s_lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(8, 40);
    s_lcd.print(s_kbd_buf[0] ? s_kbd_buf : "_");
}

static void draw_keyboard(void) {
    s_lcd.fillScreen(TFT_BLACK);
    s_set_n = 0;

    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(8, 8);
    s_lcd.print("MIXER IP");
    redraw_kbd_buf();

    // 4x3 phone pad: 1-9, then '.', '0', backspace
    static const char pad[11] = {'1','2','3','4','5','6','7','8','9','.','0'};
    for (int idx = 0; idx < 12; idx++) {
        int col = idx % 3, row = idx / 3;
        ui_rect_t r = rect(8 + col * 54, 80 + row * 46, 50, 40);
        char lbl[2]; int field;
        if (idx < 11) { lbl[0] = pad[idx]; lbl[1] = 0; field = pad[idx]; }
        else          { lbl[0] = '<';      lbl[1] = 0; field = ACT_KBD_BS; }
        s_lcd.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_DARKGREY);
        s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        s_lcd.setTextSize(3);
        s_lcd.setCursor(r.x + 16, r.y + 9);
        s_lcd.print(lbl);
        set_add(r, field);
    }

    s_lcd.fillRoundRect(8, 268, 76, 40, 6, TFT_DARKGREEN);
    s_lcd.setTextColor(TFT_BLACK, TFT_DARKGREEN);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(34, 280);
    s_lcd.print("OK");
    set_add(rect(8, 268, 76, 40), ACT_KBD_OK);

    s_lcd.drawRoundRect(88, 268, 76, 40, 6, TFT_DARKGREY);
    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.setTextSize(2);
    s_lcd.setCursor(102, 280);
    s_lcd.print("Cx");
    set_add(rect(88, 268, 76, 40), ACT_KBD_CANCEL);
}

static void enter_keyboard(void) {
    strncpy(s_kbd_buf, s_working.mixer_ip, sizeof s_kbd_buf - 1);
    s_kbd_buf[sizeof s_kbd_buf - 1] = '\0';
    s_screen = SCREEN_KEYBOARD;
    draw_keyboard();
}

void touch_display_update(void) {
    if (s_screen != SCREEN_STATUS) return;
    uint32_t now = millis();
    if (now - s_last_update_ms < 100) return;   // ~10 Hz is plenty for status
    s_last_update_ms = now;

    bool active = tempo_source_active();
    TempoSnapshot ts; tempo_snapshot_read(&ts);

    // BPM number — repaint only when the shown text changes (no flicker).
    char bpm[8];
    if (active) ui_bpm_str(bpm, sizeof bpm, ts.bpm);
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
    bool valid = ts.valid;                          // snapshot: valid ⇒ phase >= 0
    if (valid != s_wheel_valid_shown) {            // state change: clear interior
        s_lcd.fillCircle(WHEEL_CX, WHEEL_CY, WHEEL_R - 1, TFT_BLACK);
        s_lcd.drawCircle(WHEEL_CX, WHEEL_CY, WHEEL_R, TFT_DARKGREEN);
        s_prev_mx = -1;
        s_wheel_valid_shown = valid;
    }
    if (valid) {
        float ang = ui_phase_angle(ts.phase, (float)ts.quantum);
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
    // Poll the AXS5106L and dispatch taps. Coordinates are mapped to screen
    // space with LNK-014's rotation-4 transform (X direct, Y inverted). One tap
    // per press: act on the touch-down edge, rearm on release.
    static bool s_touch_down = false;

    axs_touch_t t;
    if (!axs_read(&t)) return;                 // I2C gap: keep prior state
    bool touched = t.points_len > 0;

    if (touched && !s_touch_down) {
        s_touch_down = true;
        int x = t.points[0].x;                 // rotation-4: X maps direct,
        int y = 319 - t.points[0].y;           //             Y is inverted
        if (s_screen == SCREEN_STATUS) {
            if (ui_hit(&STATUS_SET_RECT, 1, x, y) >= 0) enter_settings();
        } else if (s_screen == SCREEN_SETTINGS) {
            int i = ui_hit(s_set_rects, s_set_n, x, y);
            if (i >= 0) {
                int f = s_set_fields[i];
                if (f == ACT_BACK) {
                    s_screen = SCREEN_STATUS; draw_status();
                } else if (f == ACT_IP_EDIT) {
                    enter_keyboard();
                } else if (f == ACT_WRITE) {
                    // Persist exactly like web_config.cpp's handle_save().
                    if (config_validate(&s_working)) {
                        g_config = s_working;
                        config_save(&g_config);
                        s_lcd.fillScreen(TFT_BLACK);
                        s_lcd.setTextColor(TFT_GREEN, TFT_BLACK);
                        s_lcd.setTextSize(2);
                        s_lcd.setCursor(8, 140);
                        s_lcd.print("Saved.");
                        s_lcd.setCursor(8, 170);
                        s_lcd.print("Rebooting");
                        delay(700);
                        ESP.restart();
                    } else {                       // invalid: show error, don't save
                        s_lcd.fillScreen(TFT_BLACK);
                        s_lcd.setTextColor(TFT_RED, TFT_BLACK);
                        s_lcd.setTextSize(2);
                        s_lcd.setCursor(8, 130);
                        s_lcd.print("Invalid config");
                        s_lcd.setTextSize(1);
                        s_lcd.setCursor(8, 158);
                        s_lcd.print("check the mixer IP");
                        delay(1300);
                        draw_settings();
                    }
                } else {
                    ui_apply_settings_tap(&s_working, f);
                    draw_settings();
                }
            }
        } else if (s_screen == SCREEN_KEYBOARD) {
            int i = ui_hit(s_set_rects, s_set_n, x, y);
            if (i >= 0) {
                int f = s_set_fields[i];
                if (f == ACT_KBD_OK) {
                    strncpy(s_working.mixer_ip, s_kbd_buf, sizeof s_working.mixer_ip - 1);
                    s_working.mixer_ip[sizeof s_working.mixer_ip - 1] = '\0';
                    s_screen = SCREEN_SETTINGS; draw_settings();
                } else if (f == ACT_KBD_CANCEL) {
                    s_screen = SCREEN_SETTINGS; draw_settings();
                } else {                            // digit / '.' / backspace
                    ui_ip_apply(s_kbd_buf, sizeof s_kbd_buf,
                                f == ACT_KBD_BS ? '\b' : (char)f);
                    redraw_kbd_buf();
                }
            }
        }
    } else if (!touched) {
        s_touch_down = false;
    }
}

#endif // HAS_TOUCH_DISPLAY
