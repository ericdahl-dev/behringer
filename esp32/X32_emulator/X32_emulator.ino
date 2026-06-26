#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>

#define WIFI_SSID_HOME "Skeyelab"
#define WIFI_PASS_HOME "diamond2"
#define WIFI_SSID_GUEST "ND-guest"
#define WIFI_AP_SSID "X32-Emulator"

extern "C" {
    void x32_init(const char *wifi_ip);
    void x32_tick(void);
}

static bool tryConnect(const char *ssid, const char *pass, unsigned long timeout_ms) {
    WiFi.begin(ssid, pass);
    Serial.print("Trying ");
    Serial.print(ssid);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < timeout_ms) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        while (1);
    }

    WiFi.mode(WIFI_AP_STA);

    static char ip_buf[32];

    if (tryConnect(WIFI_SSID_HOME, WIFI_PASS_HOME, 10000)) {
        Serial.println("Connected: " WIFI_SSID_HOME);
        strncpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf) - 1);
    } else if (tryConnect(WIFI_SSID_GUEST, "", 10000)) {
        Serial.println("Connected: " WIFI_SSID_GUEST);
        strncpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf) - 1);
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        delay(200);
        WiFi.softAP(WIFI_AP_SSID);
        Serial.println("AP mode: " WIFI_AP_SSID);
        strncpy(ip_buf, WiFi.softAPIP().toString().c_str(), sizeof(ip_buf) - 1);
    }
    ip_buf[sizeof(ip_buf) - 1] = '\0';
    const char *ip_str = ip_buf;

    Serial.println("=====================================");
    Serial.print("IP: ");
    Serial.println(ip_str);
    Serial.println("=====================================");

    ArduinoOTA.setHostname("x32-emulator");
    ArduinoOTA.onStart([]() {
        Serial.println("OTA start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA done");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        Serial.printf("OTA error[%u]\n", e);
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready (x32-emulator.local)");

    x32_init(ip_str);
    Serial.println("X32 emulator ready on UDP port 10023");
}

void loop() {
    ArduinoOTA.handle();
    x32_tick();
}
