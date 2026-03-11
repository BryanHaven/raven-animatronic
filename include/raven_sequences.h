#pragma once
#include <Arduino.h>
#include "raven_servo.h"
#include "raven_audio.h"

extern void wsBroadcast(const String& msg);

// ── Built-in sequences ────────────────────────────────────────────────────────

void taskCaw(void*) {
    wsBroadcast("active:seq_caw");
    audioPlay("caw");
    beakOpen(MOVE_FAST);
    headUp(MOVE_MED);
    vTaskDelay(pdMS_TO_TICKS(400));
    beakClose(MOVE_FAST);
    headCentre(MOVE_MED);
    vTaskDelay(pdMS_TO_TICKS(200));
    beakOpen(MOVE_FAST);
    vTaskDelay(pdMS_TO_TICKS(300));
    beakClose(MOVE_FAST);
    wsBroadcast("active:none");
    vTaskDelete(nullptr);
}

void taskAlert(void*) {
    wsBroadcast("active:seq_alert");
    audioPlay("ambient");
    for (int i = 0; i < 3; i++) {
        headLeft(MOVE_FAST);
        vTaskDelay(pdMS_TO_TICKS(400));
        headRight(MOVE_FAST);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    headCentre(MOVE_MED);
    wsBroadcast("active:none");
    vTaskDelete(nullptr);
}

void taskWingFlap(void*) {
    wsBroadcast("active:seq_wingflap");
    audioPlay("flap");
    wingsOut(MOVE_FAST);
    vTaskDelay(pdMS_TO_TICKS(300));
    wingsIn(MOVE_FAST);
    vTaskDelay(pdMS_TO_TICKS(200));
    wingsOut(MOVE_MED);
    vTaskDelay(pdMS_TO_TICKS(400));
    wingsIn(MOVE_MED);
    wsBroadcast("active:none");
    vTaskDelete(nullptr);
}

void taskIdle(void*) {
    wsBroadcast("active:seq_idle");
    for (int i = 0; i < 5; i++) {
        servoMove(CH_HEAD_PAN,  1400 + (i % 3) * 100, MOVE_SLOW);
        servoMove(CH_HEAD_TILT, 1450 + (i % 2) * 100, MOVE_SLOW);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    headCentre(MOVE_SLOW);
    wsBroadcast("active:none");
    vTaskDelete(nullptr);
}

void taskSleep(void*) {
    wsBroadcast("active:seq_sleep");
    beakClose(MOVE_SLOW);
    headDown(MOVE_SLOW);
    wingsIn(MOVE_SLOW);
    bobStop();
    vTaskDelay(pdMS_TO_TICKS(MOVE_SLOW));
    servoMove(CH_HEAD_TILT, pos.head_down - 100, MOVE_SLOW);
    wsBroadcast("active:none");
    vTaskDelete(nullptr);
}

void seqCaw()      { xTaskCreate(taskCaw,      "seq_caw",   2048, nullptr, 1, nullptr); }
void seqAlert()    { xTaskCreate(taskAlert,    "seq_alert", 2048, nullptr, 1, nullptr); }
void seqWingFlap() { xTaskCreate(taskWingFlap, "seq_flap",  2048, nullptr, 1, nullptr); }
void seqIdle()     { xTaskCreate(taskIdle,     "seq_idle",  2048, nullptr, 1, nullptr); }
void seqSleep()    { xTaskCreate(taskSleep,    "seq_sleep", 2048, nullptr, 1, nullptr); }
