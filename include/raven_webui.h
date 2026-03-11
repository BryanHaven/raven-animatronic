#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "raven_config.h"
#include "raven_device.h"
#include "raven_positions.h"
#include "raven_servo.h"
#include "raven_audio.h"
#include "raven_sequences.h"
#include "raven_keyframes.h"

static AsyncWebServer wsServer(80);
static AsyncWebSocket wsSocket("/ws");

// ── Broadcast to all WS clients ───────────────────────────────────────────────
void wsBroadcast(const String& msg) { wsSocket.textAll(msg); }

// ── Current servo positions (for keyframe capture) ────────────────────────────
static uint16_t livePos[NUM_CHANNELS] = {
    BEAK_CLOSED, HEAD_CENTRE, HEAD_CENTRE,
    WING_FOLD,   WING_FOLD,   BOB_MID
};

void updateLivePos(uint8_t ch, uint16_t p) {
    if (ch < NUM_CHANNELS) livePos[ch] = p;
}

String livePosToJson() {
    String s = "[";
    for (int i = 0; i < NUM_CHANNELS; i++) {
        s += livePos[i];
        if (i < NUM_CHANNELS - 1) s += ",";
    }
    return s + "]";
}

// ── WS command dispatcher ─────────────────────────────────────────────────────
void wsDispatch(const String& msg) {
    Serial.printf("[ws] %s\n", msg.c_str());

    // Built-in sequences
    if      (msg == "seq:caw")         { seqCaw();      wsBroadcast("status:seq_caw"); }
    else if (msg == "seq:alert")       { seqAlert();    wsBroadcast("status:seq_alert"); }
    else if (msg == "seq:wingflap")    { seqWingFlap(); wsBroadcast("status:seq_wingflap"); }
    else if (msg == "seq:idle")        { seqIdle();     wsBroadcast("status:seq_idle"); }
    else if (msg == "seq:sleep")       { seqSleep();    wsBroadcast("status:seq_sleep"); }

    // Single moves
    else if (msg == "cmd:beak_open")   { beakOpen();   updateLivePos(CH_BEAK, pos.beak_open);    wsBroadcast("status:beak_open"); }
    else if (msg == "cmd:beak_close")  { beakClose();  updateLivePos(CH_BEAK, pos.beak_closed);  wsBroadcast("status:beak_close"); }
    else if (msg == "cmd:head_left")   { headLeft();   updateLivePos(CH_HEAD_PAN, pos.head_left); wsBroadcast("status:head_left"); }
    else if (msg == "cmd:head_right")  { headRight();  updateLivePos(CH_HEAD_PAN, pos.head_right); wsBroadcast("status:head_right"); }
    else if (msg == "cmd:head_centre") { headCentre(); updateLivePos(CH_HEAD_PAN, pos.head_centre); wsBroadcast("status:head_centre"); }
    else if (msg == "cmd:head_up")     { headUp();     updateLivePos(CH_HEAD_TILT, pos.head_up);  wsBroadcast("status:head_up"); }
    else if (msg == "cmd:head_down")   { headDown();   updateLivePos(CH_HEAD_TILT, pos.head_down); wsBroadcast("status:head_down"); }
    else if (msg == "cmd:wings_out")   { wingsOut();   updateLivePos(CH_WING, pos.wing_full); wsBroadcast("status:wings_out"); }
    else if (msg == "cmd:wings_in")    { wingsIn();    updateLivePos(CH_WING, pos.wing_fold);  wsBroadcast("status:wings_in"); }
    else if (msg == "cmd:bob_start")   { bobStart();   wsBroadcast("status:bob_start"); }
    else if (msg == "cmd:bob_stop")    { bobStop();    wsBroadcast("status:bob_stop"); }
    else if (msg == "cmd:neutral")     { neutral();
        for (int i = 0; i < NUM_CHANNELS; i++) livePos[i] = pos.head_centre;
        livePos[CH_BEAK]     = pos.beak_closed;
        livePos[CH_WING]     = pos.wing_fold;
        livePos[CH_BODY_BOB] = pos.bob_mid;
        wsBroadcast("status:neutral");
    }

    // Sound + sequence
    else if (msg.startsWith("sound:")) {
        String name = msg.substring(6);
        soundPlay(name);
        wsBroadcast("status:sound_" + name);
    }

    // Keyframe sequence playback
    else if (msg.startsWith("kf:play:")) {
        String name = msg.substring(8);
        keySeqPlay(name);
        wsBroadcast("status:kf_play_" + name);
    }

    // Raw servo  "servo:CH:POS:TIME"
    else if (msg.startsWith("servo:")) {
        int ch, p, t_ms = MOVE_MED;
        if (sscanf(msg.c_str(), "servo:%d:%d:%d", &ch, &p, &t_ms) >= 2) {
            servoMove(ch, p, t_ms);
            updateLivePos(ch, p);
            wsBroadcast("livepos:" + livePosToJson());
            wsBroadcast("status:servo_raw");
        }
    }

    // Request current live positions (e.g. on page load)
    else if (msg == "get:livepos") {
        wsBroadcast("livepos:" + livePosToJson());
    }

    else { wsBroadcast("status:unknown:" + msg); }
}

// ── WS event handler ──────────────────────────────────────────────────────────
void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[ws] client #%u connected\n", client->id());
        client->text("status:connected");
        client->text("sounds:"    + soundsToJson());
        client->text("positions:" + positionsToJson());
        client->text("seqlist:"   + keySeqList());
        client->text("livepos:"   + livePosToJson());
        client->text("config:"    + deviceConfigJson());
        client->text("version:"   + String(FW_VERSION) + " (" + String(FW_BUILD_DATE) + ")");
    }
    else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[ws] client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len
            && info->opcode == WS_TEXT) {
            wsDispatch(String((char*)data, len));
        }
    }
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
void webuiBegin() {

    // Static files
    wsServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // ── Sounds ───────────────────────────────────────────────────────────────
    wsServer.on("/api/sounds", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", soundsToJson());
    });

    wsServer.on("/api/sounds", HTTP_POST, [](AsyncWebServerRequest* req){},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) { req->send(400); return; }
            soundsUpsert(doc["file"]|"", doc["sequence"]|"none", doc["label"]|"");
            wsBroadcast("sounds:" + soundsToJson());
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    wsServer.on("/api/sounds", HTTP_DELETE, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("file")) { req->send(400); return; }
        String file = req->getParam("file")->value();
        if (SPIFFS.exists("/" + file + ".wav")) SPIFFS.remove("/" + file + ".wav");
        soundsRemove(file);
        wsBroadcast("sounds:" + soundsToJson());
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // ── WAV upload ────────────────────────────────────────────────────────────
    wsServer.on("/api/upload", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            req->send(200, "application/json", "{\"ok\":true}");
        },
        [](AsyncWebServerRequest* req, String filename,
           size_t index, uint8_t* data, size_t len, bool final) {
            static File uploadFile;
            static String uploadName;
            if (index == 0) {
                uploadName = filename;
                if (uploadName.endsWith(".wav") || uploadName.endsWith(".WAV"))
                    uploadName = uploadName.substring(0, uploadName.length() - 4);
                uploadName.replace(" ", "_");
                uploadFile = SPIFFS.open("/" + uploadName + ".wav", "w");
            }
            if (uploadFile) uploadFile.write(data, len);
            if (final) {
                if (uploadFile) uploadFile.close();
                soundsUpsert(uploadName, "none", uploadName);
                wsBroadcast("sounds:" + soundsToJson());
            }
        }
    );

    // ── Positions ─────────────────────────────────────────────────────────────
    wsServer.on("/api/positions", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", positionsToJson());
    });

    wsServer.on("/api/positions", HTTP_POST, [](AsyncWebServerRequest* req){},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (positionsFromJsonStr(data, len)) {
                wsBroadcast("positions:" + positionsToJson());
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
            }
        }
    );

    // ── Keyframe sequences ────────────────────────────────────────────────────
    wsServer.on("/api/sequences", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (req->hasParam("name")) {
            KeySequence seq;
            if (keySeqLoad(req->getParam("name")->value(), seq))
                req->send(200, "application/json", keySeqToJson(seq));
            else
                req->send(404, "application/json", "{\"error\":\"not found\"}");
        } else {
            req->send(200, "application/json", keySeqList());
        }
    });

    wsServer.on("/api/sequences", HTTP_POST, [](AsyncWebServerRequest* req){},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            KeySequence seq;
            String json = String((char*)data, len);
            if (keySeqFromJson(json, seq) && keySeqSave(seq)) {
                wsBroadcast("seqlist:" + keySeqList());
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(400, "application/json", "{\"error\":\"bad sequence\"}");
            }
        }
    );

    wsServer.on("/api/sequences", HTTP_DELETE, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("name")) { req->send(400); return; }
        keySeqDelete(req->getParam("name")->value());
        wsBroadcast("seqlist:" + keySeqList());
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // ── SPIFFS info ───────────────────────────────────────────────────────────
    wsServer.on("/api/spiffs", HTTP_GET, [](AsyncWebServerRequest* req) {
        size_t total = SPIFFS.totalBytes();
        size_t used  = SPIFFS.usedBytes();
        req->send(200, "application/json",
            "{\"total\":" + String(total) +
            ",\"used\":"  + String(used)  +
            ",\"free\":"  + String(total - used) + "}");
    });

    // ── Firmware info ─────────────────────────────────────────────────────────
    wsServer.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["version"]     = FW_VERSION;
        doc["build_date"]  = FW_BUILD_DATE;
        doc["device_name"] = device.device_name;
        doc["hostname"]    = device.mdns_hostname;
        doc["prefix"]      = device.mqttPrefix();
        doc["ip"]          = WiFi.localIP().toString();
        doc["free_heap"]   = ESP.getFreeHeap();
        doc["uptime_s"]    = millis() / 1000;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── Device config ─────────────────────────────────────────────────────────
    wsServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", deviceConfigJson());
    });

    wsServer.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req){},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            // Check if WiFi creds changed — if so, reboot to reconnect
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String newSsid = doc["wifi_ssid"] | device.wifi_ssid;
            String newPass = doc["wifi_password"] | String("••••••••");
            bool wifiChanged = (newSsid != device.wifi_ssid) ||
                               (newPass != "••••••••" && newPass != device.wifi_password);
            bool mqttChanged = (String(doc["mqtt_server"]|"") != device.mqtt_server);

            if (deviceConfigFromJson(data, len, true)) {
                wsBroadcast("config:" + deviceConfigJson());
                bool needReboot = wifiChanged || mqttChanged;
                req->send(200, "application/json",
                    needReboot ? "{\"ok\":true,\"reboot\":true}" : "{\"ok\":true}");
                if (needReboot) {
                    Serial.println("[config] WiFi/MQTT changed — rebooting");
                    delay(1000);
                    ESP.restart();
                }
            } else {
                req->send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
            }
        }
    );

    // ── Reconfigure — wipe config and restart as AP ───────────────────────────
    wsServer.on("/api/reconfigure", HTTP_POST, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"ok\":true}");
        Serial.println("[config] Reconfigure requested — erasing config and rebooting");
        delay(500);
        SPIFFS.remove(CONFIG_FILE);
        delay(500);
        ESP.restart();
    });

    // ── WebSocket ─────────────────────────────────────────────────────────────
    wsSocket.onEvent(onWsEvent);
    wsServer.addHandler(&wsSocket);

    wsServer.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    wsServer.begin();
    Serial.printf("[web] Started — http://%s.local or http://%s\n",
                  device.mdns_hostname.c_str(), WiFi.localIP().toString().c_str());
}

void webuiLoop() { wsSocket.cleanupClients(); }
