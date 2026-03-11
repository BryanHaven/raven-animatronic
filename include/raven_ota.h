#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h>
#include "raven_device.h"

// ── OTA firmware update over WiFi ────────────────────────────────────────────
// Flash new firmware without USB:
//   pio run --target upload --upload-port <ip-or-hostname.local>
// Or use PlatformIO UI — the device appears as a network port.
//
// OTA is password-protected using the device's mqtt_device ID as the password.
// Change ota_password in config.json via the Settings tab to customise.

extern void wsBroadcast(const String& msg);

void otaBegin() {
    // Use device ID as default OTA password — simple but effective
    String otaPass = device.mqtt_device.length() > 0 ? device.mqtt_device : "birdota";

    ArduinoOTA.setHostname(device.mdns_hostname.c_str());
    ArduinoOTA.setPassword(otaPass.c_str());

    ArduinoOTA.onStart([]() {
        String type = ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem";
        Serial.printf("[ota] Starting %s update\n", type.c_str());
        wsBroadcast("status:ota_start");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("[ota] Complete — rebooting");
        wsBroadcast("status:ota_complete");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPct = 255;
        uint8_t pct = progress / (total / 100);
        if (pct != lastPct && pct % 10 == 0) {   // broadcast every 10%
            Serial.printf("[ota] %u%%\n", pct);
            wsBroadcast("status:ota_" + String(pct) + "pct");
            lastPct = pct;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        String msg;
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "auth_failed";     break;
            case OTA_BEGIN_ERROR:   msg = "begin_failed";    break;
            case OTA_CONNECT_ERROR: msg = "connect_failed";  break;
            case OTA_RECEIVE_ERROR: msg = "receive_failed";  break;
            case OTA_END_ERROR:     msg = "end_failed";      break;
            default:                msg = "unknown";
        }
        Serial.printf("[ota] Error: %s\n", msg.c_str());
        wsBroadcast("status:ota_error_" + msg);
    });

    ArduinoOTA.begin();
    Serial.printf("[ota] Ready — hostname: %s  password: %s\n",
                  device.mdns_hostname.c_str(), otaPass.c_str());
}

void otaLoop() {
    ArduinoOTA.handle();
}
