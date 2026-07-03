#include "touch_display.h"

// Whole module compiles only for the touch-LCD board — keeps LovyanGFX out of
// screenless builds (Super Mini / XIAO / QT Py). X32Link.ino calls these under
// the same guard, so no undefined-reference when it's off.
#ifdef HAS_TOUCH_DISPLAY

#include <Arduino.h>
#include <Wire.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "axs5106l.h"

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
