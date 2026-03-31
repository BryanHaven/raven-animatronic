#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>

#include "raven_config.h"
#include "raven_sd.h"           // SD card — before audio (audio includes it too)
#include "raven_device.h"       // load config.json — must be first
#include "raven_captive.h"      // AP mode + portal + wifiConnect + wifiReconnectLoop
#include "raven_watchdog.h"     // hardware watchdog — armed after WiFi connects
#include "raven_positions.h"
#include "raven_servo.h"
#include "raven_audio.h"
#include "raven_sequences.h"
#include "raven_keyframes.h"
#include "raven_webui.h"        // defines wsBroadcast()
#include "raven_mqtt.h"         // uses wsBroadcast() — after webui
#include "raven_mdns.h"
// raven_power.h already pulled in via raven_webui.h / raven_mqtt.h
#include "raven_ota.h"          // OTA — after webui/mdns so hostname is set

// ── Boot ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n[bird] Booting v%s...\n", FW_VERSION);

    if (!SPIFFS.begin(true)) {
        Serial.println("[spiffs] Mount failed — halting");
        while (true) delay(1000);
    }
    sdBegin();      // optional — continues if no card present

    // ── Load device config (WiFi, MQTT, identity) ─────────────────────────────
    bool configured = deviceConfigLoad();

    if (!configured) {
        // ── No config — launch captive portal ────────────────────────────────
        Serial.println("[bird] Not configured — starting captive portal");
        captiveStart();
        // Portal loop — never returns unless user saves config (triggers reboot)
        while (true) {
            captiveLoop();
            delay(10);
        }
    }

    // ── Connect to WiFi ───────────────────────────────────────────────────────
    if (!wifiConnect()) {
        // WiFi failed — fall back to captive portal so user can fix credentials
        Serial.println("[bird] WiFi failed — falling back to captive portal");
        captiveStart();
        while (true) {
            captiveLoop();
            delay(10);
        }
    }

    // ── Full init ─────────────────────────────────────────────────────────────
    positionsLoad();
    servoBegin();
    audioBegin();
    powerBegin();
    keyframesBegin();
    soundsLoad();

    mdnsBegin();
    webuiBegin();
    mqttBegin();
    otaBegin();         // OTA ready — firmware flashable over WiFi from here

    // ── Arm watchdog AFTER all init is complete ───────────────────────────────
    // Arming before init risks false trips during slow SPIFFS/WiFi operations
    wdtBegin();

    neutral(MOVE_SLOW);

    Serial.printf("[bird] Ready — %s  http://%s.local  prefix: %s\n",
                  device.device_name.c_str(),
                  device.mdns_hostname.c_str(),
                  device.mqttPrefix().c_str());
}

void loop() {
    wdtFeed();              // feed watchdog first — always
    otaLoop();              // OTA check — before anything blocking
    webuiLoop();
    mqttLoop();
    wifiReconnectLoop();    // reconnect with backoff if WiFi drops
}
