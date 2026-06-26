#include <Arduino.h>

#define LED_PIN 21

static String report;

void buildReport() {
    report = "\n=== XIAO ESP32-S3 board check ===\n";
    report += "CPU freq:      " + String(getCpuFrequencyMhz()) + " MHz\n";
    report += "Flash size:    " + String(ESP.getFlashChipSize() / (1024*1024)) + " MB\n";
    report += "Internal heap: " + String(ESP.getFreeHeap() / 1024) + " KB free / "
                                + String(ESP.getHeapSize() / 1024) + " KB total\n";
    if (psramFound()) {
        report += "PSRAM:         " + String(ESP.getFreePsram() / 1024) + " KB free / "
                                    + String(ESP.getPsramSize() / 1024) + " KB total  GOOD\n";
        const size_t TEST = 1024 * 1024;
        uint8_t *buf = (uint8_t *)ps_malloc(TEST);
        if (buf) {
            memset(buf, 0xA5, TEST);
            bool ok = (buf[0] == 0xA5 && buf[TEST - 1] == 0xA5);
            free(buf);
            report += String("PSRAM 1MB rw:  ") + (ok ? "PASS" : "FAIL") + "\n";
        } else {
            report += "PSRAM 1MB alloc: FAILED\n";
        }
    } else {
        report += "PSRAM:         NOT FOUND\n";
    }
    report += "=================================\n";
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    buildReport();
}

void loop() {
    // Blink
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);

    // Print report every 5 seconds so we catch it whenever we connect
    Serial.print(report);
    delay(4900);
}
