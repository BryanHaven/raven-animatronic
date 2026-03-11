#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "raven_config.h"
#include "raven_servo.h"
#include "raven_audio.h"

// ── Data structures ───────────────────────────────────────────────────────────
struct KeyFrame {
    uint32_t t;                     // time offset from sequence start (ms)
    uint16_t ch[NUM_CHANNELS];      // position for each channel (µs)
};

struct KeySequence {
    String            name;
    String            sound;        // sound file to play ("" = none)
    std::vector<KeyFrame> frames;
};

// ── Storage path ──────────────────────────────────────────────────────────────
static String seqPath(const String& name) {
    return "/sequences/" + name + ".json";
}

// ── Serialise / deserialise ───────────────────────────────────────────────────
String keySeqToJson(const KeySequence& seq) {
    JsonDocument doc;
    doc["name"]  = seq.name;
    doc["sound"] = seq.sound;
    JsonArray frames = doc["frames"].to<JsonArray>();
    for (auto& f : seq.frames) {
        JsonObject obj = frames.add<JsonObject>();
        obj["t"] = f.t;
        JsonArray ch = obj["ch"].to<JsonArray>();
        for (int i = 0; i < NUM_CHANNELS; i++) ch.add(f.ch[i]);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool keySeqFromJson(const String& json, KeySequence& seq) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    seq.name  = doc["name"]  | "";
    seq.sound = doc["sound"] | "";
    seq.frames.clear();
    for (JsonObject obj : doc["frames"].as<JsonArray>()) {
        KeyFrame f;
        f.t = obj["t"] | 0;
        JsonArray ch = obj["ch"];
        for (int i = 0; i < NUM_CHANNELS; i++)
            f.ch[i] = ch[i] | pos.head_centre;
        seq.frames.push_back(f);
    }
    return seq.name.length() > 0;
}

// ── Save / load / delete ──────────────────────────────────────────────────────
bool keySeqSave(const KeySequence& seq) {
    if (!seq.name.length()) return false;
    File f = SPIFFS.open(seqPath(seq.name), "w");
    if (!f) return false;
    f.print(keySeqToJson(seq));
    f.close();
    Serial.printf("[kf] Saved: %s (%d frames)\n",
                  seq.name.c_str(), seq.frames.size());
    return true;
}

bool keySeqLoad(const String& name, KeySequence& seq) {
    File f = SPIFFS.open(seqPath(name), "r");
    if (!f) return false;
    String json = f.readString();
    f.close();
    return keySeqFromJson(json, seq);
}

bool keySeqDelete(const String& name) {
    String path = seqPath(name);
    if (!SPIFFS.exists(path)) return false;
    SPIFFS.remove(path);
    Serial.printf("[kf] Deleted: %s\n", name.c_str());
    return true;
}

// ── List all saved sequences ──────────────────────────────────────────────────
String keySeqList() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    File root = SPIFFS.open("/sequences");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String n = String(file.name());
            // name may be full path on some ESP32 SPIFFS versions
            if (n.startsWith("/sequences/")) n = n.substring(11);
            if (n.endsWith(".json")) n = n.substring(0, n.length() - 5);
            if (n.length()) arr.add(n);
            file = root.openNextFile();
        }
    }
    String out;
    serializeJson(doc, out);
    return out;
}

// ── Playback ──────────────────────────────────────────────────────────────────
static TaskHandle_t kfTask = nullptr;

// Forward declared — implemented in raven_webui.h
extern void wsBroadcast(const String& msg);

struct KfPlayArgs {
    KeySequence seq;
};

void kfPlayTask(void* arg) {
    KfPlayArgs* a = (KfPlayArgs*)arg;
    KeySequence& seq = a->seq;

    wsBroadcast("active:kf_" + seq.name);
    Serial.printf("[kf] Playing: %s (%d frames)\n",
                  seq.name.c_str(), seq.frames.size());

    if (seq.sound.length()) soundPlay(seq.sound);

    uint32_t start = millis();
    for (size_t i = 0; i < seq.frames.size(); i++) {
        auto& f = seq.frames[i];

        // Wait until this frame's time offset
        uint32_t now = millis() - start;
        if (f.t > now) vTaskDelay(pdMS_TO_TICKS(f.t - now));

        // Calculate move time = time to next frame (or MOVE_MED for last)
        uint16_t moveTime = MOVE_MED;
        if (i + 1 < seq.frames.size())
            moveTime = (uint16_t)min((uint32_t)(seq.frames[i+1].t - f.t),
                                     (uint32_t)2000);

        // Build multi-move command
        uint16_t pairs[NUM_CHANNELS * 2];
        for (int c = 0; c < NUM_CHANNELS; c++) {
            pairs[c * 2]     = c;
            pairs[c * 2 + 1] = f.ch[c];
        }
        servoMoveMulti(pairs, NUM_CHANNELS * 2, moveTime);
    }

    wsBroadcast("active:none");
    delete a;
    kfTask = nullptr;
    vTaskDelete(nullptr);
}

void keySeqPlay(const String& name) {
    KeySequence seq;
    if (!keySeqLoad(name, seq)) {
        Serial.printf("[kf] Not found: %s\n", name.c_str());
        return;
    }
    if (kfTask) { vTaskDelete(kfTask); kfTask = nullptr; }
    KfPlayArgs* args = new KfPlayArgs{ seq };
    xTaskCreate(kfPlayTask, "kf_play", 8192, args, 1, &kfTask);
}

// ── Ensure /sequences directory exists ───────────────────────────────────────
void keyframesBegin() {
    if (!SPIFFS.exists("/sequences")) {
        // SPIFFS doesn't have real dirs — writing a file creates the path
        File f = SPIFFS.open("/sequences/.keep", "w");
        if (f) f.close();
    }
    Serial.println("[kf] Ready");
}
