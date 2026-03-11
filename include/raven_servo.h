#pragma once
#include <Arduino.h>
#include "raven_config.h"
#include "raven_positions.h"

// ── SSC-32U serial ────────────────────────────────────────────────────────────
void servoBegin() {
    Serial2.begin(9600, SERIAL_8N1, SSC_RX_PIN, SSC_TX_PIN);
}

// Wait for SSC-32U '.' done signal
void sscWaitDone(uint32_t timeoutMs = 3000) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (Serial2.available() && Serial2.read() == '.') return;
        vTaskDelay(1);
    }
}

// ── Low-level move ────────────────────────────────────────────────────────────
void servoMove(uint8_t ch, uint16_t p, uint16_t timeMs = MOVE_MED) {
    char buf[32];
    snprintf(buf, sizeof(buf), "#%d P%d T%d\r", ch, p, timeMs);
    Serial2.print(buf);
}

// Move multiple channels simultaneously
// pairs[] = { ch, pos, ch, pos, ... }
void servoMoveMulti(const uint16_t* pairs, uint8_t len, uint16_t timeMs = MOVE_MED) {
    String cmd = "";
    for (uint8_t i = 0; i + 1 < len; i += 2) {
        char seg[16];
        snprintf(seg, sizeof(seg), "#%d P%d ", pairs[i], pairs[i+1]);
        cmd += seg;
    }
    char t[12];
    snprintf(t, sizeof(t), "T%d\r", timeMs);
    cmd += t;
    Serial2.print(cmd);
}

// ── Named moves — use runtime-calibrated positions ────────────────────────────
void beakOpen(uint16_t t = MOVE_FAST)   { servoMove(CH_BEAK,      pos.beak_open,   t); }
void beakClose(uint16_t t = MOVE_FAST)  { servoMove(CH_BEAK,      pos.beak_closed, t); }
void headLeft(uint16_t t = MOVE_MED)    { servoMove(CH_HEAD_PAN,  pos.head_left,   t); }
void headRight(uint16_t t = MOVE_MED)   { servoMove(CH_HEAD_PAN,  pos.head_right,  t); }
void headCentre(uint16_t t = MOVE_MED)  { servoMove(CH_HEAD_PAN,  pos.head_centre, t); }
void headUp(uint16_t t = MOVE_MED)      { servoMove(CH_HEAD_TILT, pos.head_up,     t); }
void headDown(uint16_t t = MOVE_MED)    { servoMove(CH_HEAD_TILT, pos.head_down,   t); }

void wingsOut(uint16_t t = MOVE_MED) { servoMove(CH_WING, pos.wing_full, t); }
void wingsIn(uint16_t t = MOVE_MED)  { servoMove(CH_WING, pos.wing_fold, t); }

void neutral(uint16_t t = MOVE_MED) {
    const uint16_t p[] = {
        CH_BEAK,      pos.beak_closed,
        CH_HEAD_PAN,  pos.head_centre,
        CH_HEAD_TILT, pos.head_centre,
        CH_WING,      pos.wing_fold,
        CH_BODY_BOB,  pos.bob_mid
    };
    servoMoveMulti(p, 10, t);
}

// ── Body bob task ─────────────────────────────────────────────────────────────
static bool        bobRunning = false;
static TaskHandle_t bobTask   = nullptr;

void bobLoop(void*) {
    while (bobRunning) {
        servoMove(CH_BODY_BOB, pos.bob_up,   MOVE_SLOW);
        vTaskDelay(pdMS_TO_TICKS(MOVE_SLOW + 100));
        servoMove(CH_BODY_BOB, pos.bob_down, MOVE_SLOW);
        vTaskDelay(pdMS_TO_TICKS(MOVE_SLOW + 100));
    }
    servoMove(CH_BODY_BOB, pos.bob_mid, MOVE_MED);
    bobTask = nullptr;
    vTaskDelete(nullptr);
}

void bobStart() {
    if (!bobRunning) {
        bobRunning = true;
        xTaskCreate(bobLoop, "bob", 1024, nullptr, 1, &bobTask);
    }
}
void bobStop() { bobRunning = false; }
