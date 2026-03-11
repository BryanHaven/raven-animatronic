#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "raven_config.h"

// ── Position store ────────────────────────────────────────────────────────────
// All positions in microseconds. Loaded from /positions.json at boot.
// Falls back to raven_config.h defaults if file missing.

struct Positions {
    uint16_t beak_closed  = BEAK_CLOSED;
    uint16_t beak_open    = BEAK_OPEN;
    uint16_t head_centre  = HEAD_CENTRE;
    uint16_t head_left    = HEAD_LEFT;
    uint16_t head_right   = HEAD_RIGHT;
    uint16_t head_up      = HEAD_UP;
    uint16_t head_down    = HEAD_DOWN;
    uint16_t wing_fold    = WING_FOLD;
    uint16_t wing_half    = WING_HALF;
    uint16_t wing_full    = WING_FULL;
    uint16_t bob_mid      = BOB_MID;
    uint16_t bob_up       = BOB_UP;
    uint16_t bob_down     = BOB_DOWN;
};

static Positions pos;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void posFromJson(JsonDocument& doc) {
    pos.beak_closed  = doc["beak_closed"]  | pos.beak_closed;
    pos.beak_open    = doc["beak_open"]    | pos.beak_open;
    pos.head_centre  = doc["head_centre"]  | pos.head_centre;
    pos.head_left    = doc["head_left"]    | pos.head_left;
    pos.head_right   = doc["head_right"]   | pos.head_right;
    pos.head_up      = doc["head_up"]      | pos.head_up;
    pos.head_down    = doc["head_down"]    | pos.head_down;
    pos.wing_fold    = doc["wing_fold"]    | pos.wing_fold;
    pos.wing_half    = doc["wing_half"]    | pos.wing_half;
    pos.wing_full    = doc["wing_full"]    | pos.wing_full;
    pos.bob_mid      = doc["bob_mid"]      | pos.bob_mid;
    pos.bob_up       = doc["bob_up"]       | pos.bob_up;
    pos.bob_down     = doc["bob_down"]     | pos.bob_down;
}

static void posToJson(JsonDocument& doc) {
    doc["beak_closed"]  = pos.beak_closed;
    doc["beak_open"]    = pos.beak_open;
    doc["head_centre"]  = pos.head_centre;
    doc["head_left"]    = pos.head_left;
    doc["head_right"]   = pos.head_right;
    doc["head_up"]      = pos.head_up;
    doc["head_down"]    = pos.head_down;
    doc["wing_fold"]    = pos.wing_fold;
    doc["wing_half"]    = pos.wing_half;
    doc["wing_full"]    = pos.wing_full;
    doc["bob_mid"]      = pos.bob_mid;
    doc["bob_up"]       = pos.bob_up;
    doc["bob_down"]     = pos.bob_down;
}

// ── Load / save ───────────────────────────────────────────────────────────────
void positionsLoad() {
    File f = SPIFFS.open("/positions.json", "r");
    if (!f) {
        Serial.println("[pos] positions.json not found, using defaults");
        return;
    }
    JsonDocument doc;
    if (!deserializeJson(doc, f)) posFromJson(doc);
    f.close();
    Serial.println("[pos] Positions loaded");
}

void positionsSave() {
    File f = SPIFFS.open("/positions.json", "w");
    if (!f) { Serial.println("[pos] Save failed"); return; }
    JsonDocument doc;
    posToJson(doc);
    serializeJson(doc, f);
    f.close();
    Serial.println("[pos] Positions saved");
}

String positionsToJson() {
    JsonDocument doc;
    posToJson(doc);
    String out;
    serializeJson(doc, out);
    return out;
}

bool positionsFromJsonStr(const uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return false;
    posFromJson(doc);
    positionsSave();
    return true;
}
