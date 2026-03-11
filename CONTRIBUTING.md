# Contributing to Raven Animatronic Controller

Thanks for your interest in contributing. This project controls animatronic props built around the [Mr. Chicken's Prop Shop Animatronic Raven Kit](https://chickenprops.com/products/animatronic-raven-kit) using an ESP32, Lynxmotion SSC-32U servo controller, and MAX98357A I2S audio amplifier.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Dev Environment Setup](#dev-environment-setup)
- [Project Structure](#project-structure)
- [Header-Only Architecture](#header-only-architecture)
- [Build, Flash, and Monitor](#build-flash-and-monitor)
- [Coding Conventions](#coding-conventions)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Good First Issues — Planned Features](#good-first-issues--planned-features)

---

## Project Overview

The firmware runs on an ESP32 and does five things:

1. **Servo control** — drives 6 servos through a Lynxmotion SSC-32U over UART (GPIO 22/23)
2. **Audio playback** — streams WAV files from SPIFFS to a MAX98357A I2S amplifier (GPIO 27/14/13)
3. **Web UI** — ESPAsyncWebServer + WebSocket control panel at `http://[hostname].local`
4. **MQTT** — three-level topic hierarchy (`location/room/device`) for show control from QLab, Home Assistant, or any MQTT broker
5. **WiFi setup** — captive portal on first boot; OTA flashing and auto-reconnect in normal operation

The boot flow is: SPIFFS → load `config.json` → WiFi (or captive portal if not configured) → full init → watchdog arm. See [src/main.cpp](src/main.cpp).

---

## Dev Environment Setup

### Required

- **[VS Code](https://code.visualstudio.com/)** with the **[PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)**
- PlatformIO handles the entire toolchain — no manual Arduino IDE or SDK install needed

### Steps

1. Clone the repo:
   ```bash
   git clone https://github.com/BryanHaven/raven-animatronic.git
   cd raven-animatronic
   ```

2. Open the folder in VS Code. PlatformIO auto-detects `platformio.ini` and installs dependencies on first build (ArduinoJson, ESPAsyncWebServer, AsyncTCP, PubSubClient).

3. Connect an ESP32 dev board via USB.

4. Build and upload — see [Build, Flash, and Monitor](#build-flash-and-monitor).

### No physical hardware?

You can still build and review code changes without hardware. Use `pio run` to compile — any error will appear there before it ever touches a device.

---

## Project Structure

```
raven-animatronic/
├── platformio.ini          ← Build config, library deps, OTA settings
├── src/
│   └── main.cpp            ← Boot flow only — setup() and loop()
├── include/
│   ├── raven_config.h      ← Hardware pins, channel assignments, compile-time constants
│   ├── raven_device.h      ← Runtime config: loads config.json, device identity, MQTT prefix
│   ├── raven_captive.h     ← Captive portal (AP mode), WiFi connect, WiFi reconnect loop
│   ├── raven_watchdog.h    ← Hardware watchdog — arm, feed, suspend/resume
│   ├── raven_positions.h   ← Load/save calibrated servo positions from positions.json
│   ├── raven_servo.h       ← SSC-32U UART driver, named moves, body bob task
│   ├── raven_audio.h       ← I2S driver, WAV playback task, sound library CRUD
│   ├── raven_sequences.h   ← Built-in cue sequences (caw, alert, wingflap, idle, sleep)
│   ├── raven_keyframes.h   ← User-created keyframe sequences: save/load/play/delete
│   ├── raven_webui.h       ← ESPAsyncWebServer, WebSocket, full HTTP API
│   ├── raven_mqtt.h        ← MQTT client, topic dispatch, identity announce
│   ├── raven_mdns.h        ← mDNS registration (http://hostname.local)
│   └── raven_ota.h         ← ArduinoOTA — firmware update over WiFi
└── data/
    ├── index.html          ← Web UI (Control, Sounds, Calibrate, Sequences, Settings tabs)
    ├── sounds.json         ← Sound library (file → label + bound sequence)
    ├── positions.json      ← Calibrated servo positions in µs
    └── sequences/          ← Saved keyframe sequences (one JSON file per sequence)
```

### Dependency order (include order in main.cpp)

Each module depends only on the ones above it:

```
raven_config.h          ← no deps
raven_device.h          ← config
raven_captive.h         ← device (WiFi connect + portal)
raven_watchdog.h        ← Arduino only
raven_positions.h       ← config
raven_servo.h           ← config, positions
raven_audio.h           ← config, sequences (forward refs)
raven_sequences.h       ← servo, audio
raven_keyframes.h       ← config, servo, audio
raven_webui.h           ← all of the above — defines wsBroadcast()
raven_mqtt.h            ← device, servo, audio, sequences, keyframes, webui
raven_mdns.h            ← device
raven_ota.h             ← Arduino OTA
```

`wsBroadcast()` is defined in `raven_webui.h` and `extern`-declared in modules that need it (`raven_sequences.h`, `raven_keyframes.h`, `raven_mqtt.h`). Keep this dependency direction — don't pull `webui` above `sequences`.

---

## Header-Only Architecture

All modules are **header-only** — everything lives in `.h` files, there are no `.cpp` files (except `main.cpp`). This keeps the build simple: one translation unit, no link-order issues, and every module is self-contained.

**Rules for new modules:**

- One `.h` file per module, named `raven_<feature>.h`
- `#pragma once` at the top
- Public API: a `featureBegin()` init function and a `featureLoop()` function if needed (called from `setup()` and `loop()` respectively)
- Module-private state goes in `static` variables at file scope
- Long-running operations use FreeRTOS tasks (`xTaskCreate`) — never `delay()` in `loop()`

---

## Build, Flash, and Monitor

All commands use the PlatformIO CLI (`pio`). In VS Code you can also use the PlatformIO sidebar buttons.

### Build only

```bash
pio run
```

### Upload filesystem (HTML, JSON data files)

Required on first flash, and any time you change `data/`:

```bash
pio run --target uploadfs --environment esp32dev
```

### Upload firmware

```bash
pio run --target upload --environment esp32dev
```

### Serial monitor

```bash
pio device monitor --environment esp32dev
```

Expected boot output after first-time setup:
```
[bird] Booting v4.1.1...
[device] Loaded: Raven 1  prefix: props/study/raven1
[wifi] Connecting to MyNetwork......
[wifi] IP: 192.168.x.x
[pos] Positions loaded
[audio] I2S initialised
[kf] Ready
[audio] Loaded 3 sounds
[mdns] http://raven1.local
[web] Started — http://raven1.local or http://192.168.x.x
[ota] Ready — hostname: raven1  password: raven1
[wdt] Watchdog armed — 30s timeout
[bird] Ready — Raven 1  http://raven1.local  prefix: props/study/raven1
```

### OTA upload (after first USB flash)

Uncomment three lines in `platformio.ini` and set the device hostname or IP:

```ini
upload_protocol = espota
upload_port     = raven1.local
upload_flags    = --auth=raven1
```

Then `pio run --target upload` will flash over WiFi.

> **Note:** Close the serial monitor before uploading — it holds the COM port open and will block USB uploads.

---

## Coding Conventions

These conventions are consistent across all existing files. New code should match them.

### Section banners

Use the established banner style for logical groups within a file:

```cpp
// ── Section title ─────────────────────────────────────────────────────────────
```

### Naming

| Thing | Convention | Example |
|-------|-----------|---------|
| Functions | `camelCase` | `servoMove()`, `audioPlay()` |
| Variables | `camelCase` | `audioTask`, `bobRunning` |
| Compile-time constants | `UPPER_CASE` | `MOVE_FAST`, `CH_BEAK` |
| JSON keys / MQTT topics | `snake_case` | `"device_name"`, `"mqtt_location"` |
| Module init function | `moduleBegin()` | `audioBegin()`, `mqttBegin()` |
| Module loop function | `moduleLoop()` | `mqttLoop()`, `webuiLoop()` |

### Servo positions and timing

- All servo positions are in **microseconds (µs)** — valid range is approximately 500–2500 µs
- All timing values are in **milliseconds (ms)**
- Always use the named timing constants (`MOVE_FAST` = 150ms, `MOVE_MED` = 300ms, `MOVE_SLOW` = 600ms) rather than raw numbers
- Always use runtime positions from the `pos` struct (loaded from `positions.json`) rather than hardcoded values — this is what allows per-unit calibration

### Serial logging

Tag every log line with the module name in brackets:

```cpp
Serial.printf("[audio] Playing %s\n", name.c_str());
Serial.println("[mqtt] connected");
```

Existing tags: `[bird]`, `[device]`, `[wifi]`, `[captive]`, `[wdt]`, `[pos]`, `[audio]`, `[kf]`, `[mdns]`, `[web]`, `[mqtt]`, `[ota]`.

### FreeRTOS tasks

Long-running operations (sequences, audio playback, body bob) run as FreeRTOS tasks so they don't block `loop()`:

```cpp
void myTask(void*) {
    // ... do work ...
    vTaskDelete(nullptr);   // always self-delete at the end
}

void myFeatureRun() {
    xTaskCreate(myTask, "task_name", 2048, nullptr, 1, &taskHandle);
}
```

- Use `vTaskDelay(pdMS_TO_TICKS(ms))` — not `delay()` — inside tasks
- Track the `TaskHandle_t` so you can cancel a running task before starting a new one
- Feed the watchdog (`wdtFeed()`) is called in `loop()` — tasks don't need to call it unless they run longer than 30 seconds

### ArduinoJson

This project uses **ArduinoJson v7**. Use `JsonDocument` (not the deprecated `DynamicJsonDocument`):

```cpp
JsonDocument doc;
deserializeJson(doc, source);
String value = doc["key"] | "default";   // | operator provides a default
```

### No blocking in loop()

`loop()` must return quickly so the watchdog gets fed and OTA/WebSocket remain responsive. All blocking work goes in FreeRTOS tasks.

---

## Submitting a Pull Request

1. **Fork** the repo and create a branch from `master`:
   ```bash
   git checkout -b feature/my-feature
   ```

2. **Build first** — `pio run` must succeed with no errors before opening a PR.

3. **Test on hardware** if your change touches servo, audio, WiFi, or MQTT behaviour. Serial monitor output confirming correct boot and operation is helpful to include in the PR description.

4. **Match the conventions** described above — banner style, naming, logging tags, no `delay()` in `loop()`.

5. **Update the README** version history table if your change is user-visible. Use a patch version (e.g. `v4.1.2`) for bug fixes, a minor version (e.g. `v4.2`) for new features.

6. **One concern per PR** — keep changes focused. A servo feature and an audio feature should be separate PRs.

7. Open the PR against `master` with a clear title and description of what changed and why.

---

## Good First Issues — Planned Features

These are all listed in the README as planned v5+ features. Each is well-scoped and self-contained.

---

### Random idle behaviour
**Difficulty:** Medium

A background FreeRTOS task that fires random small head/beak movements between show cues. Should be configurable (enable/disable, min/max interval) via the Settings tab and stored in `config.json`.

**Approach:**
- Add a task in a new `raven_idle.h`
- `idleBegin()` / `idleStop()` called from webUI and MQTT (`/command idle_start` / `idle_stop`)
- Use `random()` for interval and movement selection
- Must not conflict with running sequences — check a shared `sequenceRunning` flag before moving

---

### Physical trigger input
**Difficulty:** Easy–Medium

GPIO 34 is the designated input pin (3.5mm jack, contact closure). On a LOW pulse, fire a configurable sequence.

**Approach:**
- Add trigger pin handling in a new `raven_trigger.h`
- Configurable sequence name stored in `config.json` (default: `"caw"`)
- Debounce in software (50ms)
- Expose via Settings tab: enable/disable, sequence to fire

---

### Looping ambient audio
**Difficulty:** Medium

Add a `/audio/loop` MQTT command and web UI button that plays a WAV file continuously until stopped.

**Approach:**
- Extend `raven_audio.h` with `audioLoop(name)` and `audioLoopStop()`
- Loop flag checked at end of `audioPlayTask` — if set, re-queue the same file
- Add `/audio/stop` command to both MQTT and the web UI Sounds tab

---

### Cue numbers (`/cue` topic)
**Difficulty:** Easy

Map integer cue numbers to sequence names so QLab/Isadora can fire sequences by number rather than name.

**Approach:**
- Add a `cues.json` file in `data/` mapping integers to sequence names: `{"1":"caw","2":"alert"}`
- Subscribe to `{prefix}/cue` in `raven_mqtt.h`
- Add a Cues tab (or extend Settings) in `index.html` to edit the mapping
- Expose via HTTP API: `GET /api/cues`, `POST /api/cues`

---

### Status LED (NeoPixel)
**Difficulty:** Easy–Medium

A single NeoPixel showing firmware state at a glance.

| State | Colour |
|-------|--------|
| Captive portal / no WiFi | Amber pulse |
| WiFi connected, no MQTT | Blue pulse |
| Fully connected, idle | Green solid |
| Sequence running | White flash |
| Watchdog / error | Red |

**Approach:**
- Add `raven_led.h` using the Adafruit NeoPixel library
- Pin configurable in `raven_config.h` (suggested: GPIO 2, built-in LED on many dev boards)
- `ledSetState(LedState)` called from WiFi, MQTT, and sequence code

---

### Audio volume control
**Difficulty:** Easy

The MAX98357A GAIN pin controls output level (float = 9dB, GND = 12dB, 3.3V = 6dB). A DAC output on GPIO 25 through a resistor divider to the GAIN pin allows variable volume.

**Approach:**
- `dacWrite(25, value)` where 0–255 maps to 0–3.3V
- Expose volume (0–100%) in the Sounds tab and via `/volume` MQTT command
- Store in `config.json`

---

### Export / import config
**Difficulty:** Medium

Allow a complete device config (config.json, positions.json, sounds.json, sequences/) to be downloaded as a zip and uploaded to a new device — making it easy to clone a configured bird.

**Approach:**
- `GET /api/export` — streams a zip of all config files
- `POST /api/import` — accepts a zip, extracts to SPIFFS, reboots
- Requires a zip library (e.g. `esp32-micro-sdcard` or a lightweight zip writer)

---

### Conductor mode
**Difficulty:** Hard

One MQTT message fans out a timed sequence to multiple birds in a room simultaneously, with per-bird timing offsets.

**Approach:**
- New conductor JSON format: `{"sequence":"caw","offsets":{"raven1":0,"raven2":500}}`
- Published to `{location}/{room}/conductor`
- Each bird reads its own offset from the payload and delays accordingly
- Requires all birds to have synchronised clocks (NTP) or a start-signal approach

---

*Controller firmware developed with Claude (Anthropic). Raven mechanic kit by Mr. Chicken's Prop Shop / Jasper J. Anderson FX.*
