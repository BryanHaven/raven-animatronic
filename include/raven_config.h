#pragma once

// ── Firmware version ──────────────────────────────────────────────────────────
#define FW_VERSION      "4.1.3"
#define FW_BUILD_DATE   __DATE__ " " __TIME__

// ── Compile-time defaults ─────────────────────────────────────────────────────
// These are fallback values only. At runtime everything is overridden
// by config.json loaded from SPIFFS via raven_device.h.
// Edit config.json through the captive portal or the Settings page in the UI.

// ── Hardware pins ─────────────────────────────────────────────────────────────
#define SSC_TX_PIN      17
#define SSC_RX_PIN      16

#define INA219_SDA      22
#define INA219_SCL      21
#define INA219_ADDR     0x40

#define I2S_BCLK        26
#define I2S_LRC         27
#define I2S_DOUT        25

// ── SSC-32U servo channels ────────────────────────────────────────────────────
#define CH_BEAK         0
#define CH_HEAD_PAN     1
#define CH_HEAD_TILT    2
#define CH_WING         3       // single servo drives both wings mechanically
#define CH_BODY_BOB     5
#define NUM_CHANNELS    6

// ── Default servo positions (µs) — overridden by positions.json ───────────────
#define BEAK_CLOSED     1500
#define BEAK_OPEN       1900
#define HEAD_CENTRE     1500
#define HEAD_LEFT       1200
#define HEAD_RIGHT      1800
#define HEAD_UP         1600
#define HEAD_DOWN       1400
#define WING_FOLD       1000
#define WING_HALF       1500
#define WING_FULL       2000
#define BOB_MID         1500
#define BOB_UP          1700
#define BOB_DOWN        1300

// ── Timing defaults (ms) ─────────────────────────────────────────────────────
#define MOVE_FAST       150
#define MOVE_MED        300
#define MOVE_SLOW       600

// ── Captive portal AP ─────────────────────────────────────────────────────────
#define AP_SSID         "BirdSetup"
#define AP_PASSWORD     ""              // open AP — add password if desired
#define AP_IP           "192.168.4.1"
#define CONFIG_FILE     "/config.json"
#define WIFI_TIMEOUT_MS 15000           // ms to wait for WiFi before falling back to AP
