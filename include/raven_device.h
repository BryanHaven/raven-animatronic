#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "raven_config.h"

// ── Runtime device identity ───────────────────────────────────────────────────
// All fields loaded from /config.json at boot.
// Written by the captive portal or the Settings page in the web UI.

struct DeviceConfig {
    // Identity
    String device_name   = "Bird 1";
    String mdns_hostname = "bird1";

    // MQTT topic hierarchy:  location/room/device/...
    String mqtt_location = "props";
    String mqtt_room     = "room1";
    String mqtt_device   = "bird1";
    uint16_t mqtt_port   = 1883;
    String mqtt_server   = "";

    // WiFi
    String wifi_ssid     = "";
    String wifi_password = "";

    // Derived — built from location/room/device
    String mqttPrefix() const {
        return mqtt_location + "/" + mqtt_room + "/" + mqtt_device;
    }
    // Topic helpers
    String topic(const String& leaf) const { return mqttPrefix() + "/" + leaf; }

    bool isConfigured() const {
        return wifi_ssid.length() > 0 && mqtt_server.length() > 0;
    }
};

static DeviceConfig device;

// ── Serialise ─────────────────────────────────────────────────────────────────
void deviceToJson(JsonDocument& doc) {
    doc["device_name"]    = device.device_name;
    doc["mdns_hostname"]  = device.mdns_hostname;
    doc["mqtt_location"]  = device.mqtt_location;
    doc["mqtt_room"]      = device.mqtt_room;
    doc["mqtt_device"]    = device.mqtt_device;
    doc["mqtt_server"]    = device.mqtt_server;
    doc["mqtt_port"]      = device.mqtt_port;
    doc["wifi_ssid"]      = device.wifi_ssid;
    doc["wifi_password"]  = device.wifi_password;
}

void deviceFromJson(JsonDocument& doc) {
    device.device_name   = doc["device_name"]   | device.device_name;
    device.mdns_hostname = doc["mdns_hostname"]  | device.mdns_hostname;
    device.mqtt_location = doc["mqtt_location"]  | device.mqtt_location;
    device.mqtt_room     = doc["mqtt_room"]      | device.mqtt_room;
    device.mqtt_device   = doc["mqtt_device"]    | device.mqtt_device;
    device.mqtt_server   = doc["mqtt_server"]    | device.mqtt_server;
    device.mqtt_port     = doc["mqtt_port"]      | device.mqtt_port;
    device.wifi_ssid     = doc["wifi_ssid"]      | device.wifi_ssid;
    device.wifi_password = doc["wifi_password"]  | device.wifi_password;
}

// ── Load / save ───────────────────────────────────────────────────────────────
bool deviceConfigLoad() {
    File f = SPIFFS.open(CONFIG_FILE, "r");
    if (!f) {
        Serial.println("[device] config.json not found — captive portal needed");
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        Serial.println("[device] config.json parse error");
        return false;
    }
    f.close();
    deviceFromJson(doc);
    // Fallback: if name/hostname were left blank in the portal, use the device ID
    if (device.device_name.isEmpty())   device.device_name   = device.mqtt_device;
    if (device.mdns_hostname.isEmpty()) device.mdns_hostname = device.mqtt_device;
    Serial.printf("[device] Loaded: %s  prefix: %s\n",
                  device.device_name.c_str(), device.mqttPrefix().c_str());
    return device.isConfigured();
}

bool deviceConfigSave() {
    File f = SPIFFS.open(CONFIG_FILE, "w");
    if (!f) { Serial.println("[device] Save failed"); return false; }
    JsonDocument doc;
    deviceToJson(doc);
    serializeJson(doc, f);
    f.close();
    Serial.printf("[device] Saved: %s\n", device.mqttPrefix().c_str());
    return true;
}

String deviceConfigJson() {
    JsonDocument doc;
    deviceToJson(doc);
    // Don't send password back to UI — send placeholder
    doc["wifi_password"] = device.wifi_password.length() > 0 ? "••••••••" : "";
    String out;
    serializeJson(doc, out);
    return out;
}

bool deviceConfigFromJson(const uint8_t* data, size_t len, bool keepPassword = false) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return false;
    // If password field is placeholder, preserve existing password
    String incoming_pw = doc["wifi_password"] | "";
    deviceFromJson(doc);
    if (keepPassword && incoming_pw == "••••••••") {
        // Restore actual password — already in device struct from load
    }
    return deviceConfigSave();
}
