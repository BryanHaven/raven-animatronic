#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>

// ── Hardware watchdog ─────────────────────────────────────────────────────────
// If the firmware hangs for more than WDT_TIMEOUT_S seconds, the ESP32
// automatically reboots. loop() must call wdtFeed() at least once per cycle.
//
// During OTA the watchdog is automatically suspended by ArduinoOTA.
// During long sequences (> WDT_TIMEOUT_S) call wdtFeed() mid-sequence
// or increase the timeout — 30s is generous for all current sequences.

#define WDT_TIMEOUT_S   30      // seconds before forced reboot

void wdtBegin() {
    // esp_task_wdt_init(timeout_s, panic) is the IDF 4.x API used by the installed
    // Arduino ESP32 framework (framework-arduinoespressif32 3.x / espressif32 6.x).
    // The IDF 5.x struct-based API (esp_task_wdt_config_t / esp_task_wdt_reconfigure)
    // is NOT available in this toolchain and causes a compile error.
    esp_task_wdt_init(WDT_TIMEOUT_S, true);  // timeout in seconds, panic/reboot on trip
    esp_task_wdt_add(NULL);                   // subscribe current (loop) task
    Serial.printf("[wdt] Watchdog armed — %ds timeout\n", WDT_TIMEOUT_S);
}

// Call this every loop() iteration
inline void wdtFeed() {
    esp_task_wdt_reset();
}

// Call at start/end of long blocking operations (sequences, file writes)
// to prevent false trips
inline void wdtSuspend() { esp_task_wdt_delete(NULL); }
inline void wdtResume()  { esp_task_wdt_add(NULL);    }
