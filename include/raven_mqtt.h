#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "raven_device.h"
#include "raven_servo.h"
#include "raven_audio.h"
#include "raven_sequences.h"
#include "raven_keyframes.h"

static WiFiClient   mqttWifi;
static PubSubClient mqttClient(mqttWifi);

extern void wsBroadcast(const String& msg);

// ── Topic helpers — all derived from runtime device prefix ────────────────────
// Inbound (subscribe)
String tSub(const String& leaf) { return device.topic(leaf); }
// Outbound (publish)
String tPub(const String& leaf) { return device.topic(leaf); }

// ── Publish ───────────────────────────────────────────────────────────────────
void mqttPublish(const String& topic, const String& payload) {
    if (mqttClient.connected())
        mqttClient.publish(topic.c_str(), payload.c_str());
}

void mqttPublishStatus(const String& msg) {
    mqttPublish(tPub("status"), msg);
    wsBroadcast("status:" + msg);
    Serial.printf("[mqtt] %s → %s\n", tPub("status").c_str(), msg.c_str());
}

void mqttPublishSoundList() {
    mqttPublish(tPub("sounds/list"), soundsToJson());
}

void mqttPublishSeqList() {
    String kfJson = keySeqList();
    String merged = "[\"caw\",\"alert\",\"wingflap\",\"idle\",\"sleep\"";
    if (kfJson.length() > 2)
        merged += "," + kfJson.substring(1, kfJson.length() - 1);
    merged += "]";
    mqttPublish(tPub("sequences/list"), merged);
}

void mqttPublishIdentity() {
    // Announce this device to the show controller
    JsonDocument doc;
    doc["device_name"]   = device.device_name;
    doc["hostname"]      = device.mdns_hostname;
    doc["prefix"]        = device.mqttPrefix();
    doc["ip"]            = WiFi.localIP().toString();
    String out;
    serializeJson(doc, out);
    mqttPublish(tPub("identity"), out);
    Serial.printf("[mqtt] Announced: %s\n", device.mqttPrefix().c_str());
}

// ── Command dispatch ──────────────────────────────────────────────────────────
void mqttDispatchCommand(const String& cmd) {
    if      (cmd == "beak_open")   { beakOpen();    mqttPublishStatus("beak_open"); }
    else if (cmd == "beak_close")  { beakClose();   mqttPublishStatus("beak_close"); }
    else if (cmd == "head_left")   { headLeft();    mqttPublishStatus("head_left"); }
    else if (cmd == "head_right")  { headRight();   mqttPublishStatus("head_right"); }
    else if (cmd == "head_centre") { headCentre();  mqttPublishStatus("head_centre"); }
    else if (cmd == "head_up")     { headUp();      mqttPublishStatus("head_up"); }
    else if (cmd == "head_down")   { headDown();    mqttPublishStatus("head_down"); }
    else if (cmd == "wings_out")   { wingsOut();    mqttPublishStatus("wings_out"); }
    else if (cmd == "wings_in")    { wingsIn();     mqttPublishStatus("wings_in"); }
    else if (cmd == "bob_start")   { bobStart();    mqttPublishStatus("bob_start"); }
    else if (cmd == "bob_stop")    { bobStop();     mqttPublishStatus("bob_stop"); }
    else if (cmd == "neutral")     { neutral();     mqttPublishStatus("neutral"); }
    else if (cmd == "wing_flap")   { seqWingFlap(); mqttPublishStatus("seq_wingflap"); }
    else { mqttPublishStatus("unknown_cmd:" + cmd); }
}

// ── Callback ──────────────────────────────────────────────────────────────────
void mqttCallback(char* topicRaw, byte* payload, unsigned int len) {
    String t   = String(topicRaw);
    String msg = String((char*)payload, len);
    String pfx = device.mqttPrefix() + "/";
    // Strip prefix to get the leaf topic
    String leaf = t.startsWith(pfx) ? t.substring(pfx.length()) : t;
    Serial.printf("[mqtt] %s → %s\n", leaf.c_str(), msg.c_str());

    if      (leaf == "command")  { mqttDispatchCommand(msg); }
    else if (leaf == "sequence") {
        if      (msg == "caw")      { seqCaw();      mqttPublishStatus("seq_caw"); }
        else if (msg == "alert")    { seqAlert();    mqttPublishStatus("seq_alert"); }
        else if (msg == "idle")     { seqIdle();     mqttPublishStatus("seq_idle"); }
        else if (msg == "sleep")    { seqSleep();    mqttPublishStatus("seq_sleep"); }
        else if (msg == "wingflap") { seqWingFlap(); mqttPublishStatus("seq_wingflap"); }
        else { keySeqPlay(msg);       mqttPublishStatus("seq_kf_" + msg); }
    }
    else if (leaf == "sound") {
        soundPlay(msg);
        mqttPublishStatus("sound_" + msg);
    }
    else if (leaf == "audio") {
        audioPlay(msg);
        mqttPublishStatus("audio_" + msg);
    }
    else if (leaf == "servo") {
        int ch, p, t_ms = MOVE_MED;
        if (sscanf(msg.c_str(), "%d %d %d", &ch, &p, &t_ms) >= 2)
            servoMove(ch, p, t_ms);
        mqttPublishStatus("servo_raw");
    }
    else if (leaf == "query") {
        if (msg == "sounds"    || msg == "all") mqttPublishSoundList();
        if (msg == "sequences" || msg == "all") mqttPublishSeqList();
        if (msg == "identity"  || msg == "all") mqttPublishIdentity();
    }
}

// ── Connect / reconnect ───────────────────────────────────────────────────────
void mqttReconnect() {
    if (mqttClient.connected()) return;
    Serial.printf("[mqtt] connecting to %s:%d...", device.mqtt_server.c_str(), device.mqtt_port);
    String clientId = "bird_" + device.mqtt_device + "_" + String(random(0xffff), HEX);

    // Last will — broker publishes this automatically if we drop off unexpectedly
    String willTopic  = tPub("status");
    String willMsg    = "offline";

    if (mqttClient.connect(clientId.c_str(),
                           nullptr, nullptr,                    // no auth
                           willTopic.c_str(), 1, true,         // QoS 1, retained
                           willMsg.c_str())) {
        Serial.println(" OK");
        // Subscribe to this device's own topics
        mqttClient.subscribe(tSub("command").c_str());
        mqttClient.subscribe(tSub("sequence").c_str());
        mqttClient.subscribe(tSub("sound").c_str());
        mqttClient.subscribe(tSub("audio").c_str());
        mqttClient.subscribe(tSub("servo").c_str());
        mqttClient.subscribe(tSub("query").c_str());
        // Announce and publish inventory
        mqttPublishIdentity();
        mqttPublishSoundList();
        mqttPublishSeqList();
        mqttPublishStatus("online");    // clears the retained "offline" will
    } else {
        Serial.printf(" failed (rc=%d)\n", mqttClient.state());
    }
}

void mqttBegin() {
    if (!device.isConfigured()) return;
    mqttClient.setServer(device.mqtt_server.c_str(), device.mqtt_port);
    mqttClient.setCallback(mqttCallback);
}

void mqttLoop() {
    if (!device.isConfigured()) return;
    static unsigned long lastAttempt = 0;
    if (!mqttClient.connected()) {
        if (millis() - lastAttempt > 5000) {
            lastAttempt = millis();
            mqttReconnect();
        }
    }
    mqttClient.loop();
}
