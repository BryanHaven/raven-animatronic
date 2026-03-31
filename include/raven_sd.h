#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "raven_config.h"

// ── SD card state ─────────────────────────────────────────────────────────────
static bool     sdMounted = false;
static SPIClass sdSpi(VSPI);

// ── Init ──────────────────────────────────────────────────────────────────────
// Call once after SPIFFS.begin(). Uses PCB SPI pins from raven_config.h.
//
// SD_USE_CD: define this (in platformio.ini build_flags) to enable card-detect
// on GPIO 36 (PCB hardware pull-up, active LOW). Leave undefined when testing
// with a breakout that has no CD pin (e.g. Adafruit #254) — floating GPIO 36
// reads HIGH and would block the mount attempt.
void sdBegin() {
#ifdef SD_USE_CD
    // Card detect — GPIO 36 is input-only, no internal pull-up on ESP32
    pinMode(SD_CD, INPUT);
    if (digitalRead(SD_CD) == HIGH) {
        Serial.println("[sd] No card detected (CD high)");
        return;
    }
#endif

    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSpi)) {
        Serial.println("[sd] Mount failed — no card or wiring issue");
        return;
    }

    sdMounted = true;
    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();
    Serial.printf("[sd] Mounted — %.1f MB total, %.1f MB used\n",
        (double)total / 1e6, (double)used / 1e6);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
bool sdExists(const String& path) {
    return sdMounted && SD.exists(path);
}

bool sdRemove(const String& path) {
    return sdMounted && SD.remove(path);
}

File sdOpen(const String& path, const char* mode = FILE_READ) {
    return SD.open(path, mode);
}

// Status JSON for /api/sd
String sdStatusJson() {
    if (!sdMounted) return "{\"mounted\":false}";
    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();
    return "{\"mounted\":true"
           ",\"total\":" + String((uint32_t)(total / 1024)) +
           ",\"used\":"  + String((uint32_t)(used  / 1024)) +
           ",\"free\":"  + String((uint32_t)((total - used) / 1024)) + "}";
}
