#include <WiFi.h>
#include "config.h"

void setup() {
    Serial.begin(115200);
    delay(2000);   // give USB CDC time to connect to host

    Serial.println("[X32Link] booting");
    Serial.print("[X32Link] connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[X32Link] IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("[X32Link] XR18 target: %s:%d  FX slot: %d\n",
                  XR18_IP, XR18_PORT, LINK_FX_SLOT);
}

void loop() {
    delay(5000);
    Serial.printf("[X32Link] WiFi: %s  RSSI: %d dBm\n",
                  WiFi.status() == WL_CONNECTED ? "OK" : "LOST",
                  WiFi.RSSI());
}
