# Animatronic Bird Controller — v4.1

ESP32-based WiFi controller for animatronic props built around the [Mr. Chicken's Prop Shop Animatronic Raven Kit](https://chickenprops.com/products/animatronic-raven-kit) and compatible builds. v4 adds a **captive portal for first-time WiFi/MQTT setup**, **full runtime device configuration** (no hardcoded credentials), and a **three-level MQTT topic hierarchy** designed for multi-prop show environments. v4.1 hardens the firmware for show reliability with OTA updates, a hardware watchdog, MQTT last will, and WiFi auto-reconnect.

## Interactive Demo
👉 [Try the live demo](https://bryanhaven.github.io/raven-animatronic/)

> **Upgrading from v3?** The only breaking change is that WiFi and MQTT credentials are no longer in `raven_config.h`. On first boot v4 will launch the setup AP — configure once, then it behaves exactly like v3.

---

## Version History

| Version | Summary |
|---------|---------|
| **v4.1.1** | Bug fixes: watchdog API corrected for installed toolchain (`esp_task_wdt_init` replaces IDF 5.x struct API); captive portal now validates Device Name and Hostname as required fields; firmware fallback populates blank name/hostname from Device ID so mDNS always starts |
| **v4.1** | Show hardening: OTA firmware updates over WiFi, hardware watchdog (30s), MQTT last will (`offline`/`online` on `{prefix}/status`), WiFi auto-reconnect with exponential backoff |
| **v4.0** | Captive portal first-time setup (BirdSetup AP), runtime config.json, three-level MQTT topic hierarchy (location/room/device), Settings tab in web UI, Reset to AP mode button, identity announcement on MQTT connect |
| **v3.0** | mDNS (`http://bird.local`), live sequence indicator, SPIFFS-persisted servo calibration, keyframe sequence editor, MQTT Home Assistant discovery |
| **v2.0** | I2S audio (MAX98357A), dynamic sound library, WAV upload via web UI |
| **v1.0** | Initial release: DAC audio, static soundboard, ESPAsyncWebServer, WebSocket control |

---

## Planned Features (v5+)

| Feature | Notes |
|---------|-------|
| Random idle behavior | Background task — random head/beak movements between cues, configurable interval |
| Conductor mode | One MQTT message fans out a timed sequence to multiple birds in a room |
| Startup sync | All birds in a room wait for a sync message before moving to neutral |
| Export/import config | Download full config as zip, clone to new bird by uploading |
| OTA SPIFFS update | Currently OTA covers firmware only — add filesystem update support |
| Physical trigger input | 3.5mm jack → GPIO 34, fires configurable sequence on contact closure |
| Current sensing | INA219 on VS+ rail — detect stalled/disconnected servos |
| Audio volume control | DAC on GPIO 25 → MAX98357A GAIN pin via resistor divider |
| Audio ducking | Lower ambient audio level during cue sequences |
| Looping ambient audio | `/audio/loop` command for continuous background sound |
| Cue numbers | Numeric `/cue` topic mapping integers to sequence names for QLab/Isadora |
| Status LED | NeoPixel showing WiFi/MQTT/sequence state |
| Dynamic servo mapping | Config-driven channel map in config.json — define channel number, name, µs range, and neutral per servo. Sequences and commands reference channels by name rather than hardcoded numbers. Kit presets (raven, parrot) selectable at captive portal setup. Makes firmware kit-agnostic for parrot and future builds. |



---

## What's New in v4.1 — Show Hardening

- **OTA firmware updates** — flash new firmware over WiFi without USB. Password defaults to the device's Device ID (e.g. `raven1`). To use, uncomment three lines in `platformio.ini` and run upload normally — PlatformIO finds the bird on the network automatically.
- **Hardware watchdog** — 30-second timeout. If the firmware hangs for any reason the ESP32 automatically reboots. Armed after all init completes to avoid false trips during startup.
- **MQTT last will** — the broker automatically publishes `offline` to `{prefix}/status` the moment a bird drops off unexpectedly. When the bird reconnects it publishes `online`, clearing the retained message. Your show controller always knows the true state of every prop.
- **WiFi auto-reconnect** — if WiFi drops mid-show, the firmware retries automatically with exponential backoff: 5s → 10s → 20s → 40s → 60s (cap), resetting to 5s on successful reconnect. MQTT reconnects automatically once WiFi is restored.

---

## What's New in v4.0

- **Captive portal setup** — on first boot (or after a reset) the ESP32 becomes a WiFi access point called `BirdSetup`. Connect any phone or laptop, get an automatic popup, fill in WiFi and MQTT details, save. Done.
- **Runtime device configuration** — all identity, WiFi, and MQTT settings live in `config.json` on SPIFFS. Edit from the new Settings tab in the web UI without reflashing.
- **Three-level MQTT topic hierarchy** — `location/room/device` (e.g. `props/tiki_bar/parrot1`). Supports MQTT wildcard broadcasting to whole rooms.
- **Settings tab** — edit device name, hostname, MQTT prefix, broker, and WiFi from the browser. Auto-reboots if WiFi or MQTT settings change.
- **Reset to AP mode** — one button in Settings wipes `config.json` and restarts as `BirdSetup` for reconfiguration.
- **Identity announcement** — on MQTT connect the bird publishes its name, hostname, IP, and full topic prefix to `{prefix}/identity` so show controllers always know what's on the network.
- **Multi-prop ready** — flash the same firmware to every bird. Configure each individually through the portal. Use MQTT wildcards to control whole rooms at once.

---

## Hardware

### Raven Mechanic Kit
**Mr. Chicken's Prop Shop — Animatronic Raven Kit**
- https://chickenprops.com/products/animatronic-raven-kit
- Available in **Basic** (mechanical parts only), **Deluxe** (Basic + servos), and **Bare Bones** (custom parts only) configurations

### Servos (per kit documentation)
| Qty | Part | Role |
|-----|------|------|
| 1× | Hitec HS-53 | Beak open/close |
| 3× | Hitec HS-425BB | Head pan, head tilt, body bob |
| 1× | Hitec HS-645MG | Wings (single servo, both wings mechanically linked) |

> The Deluxe Kit includes all servos. Check specs before substituting — voltage and torque ranges matter.

### Electronics
| Component | Exact Part | Notes |
|-----------|-----------|-------|
| Microcontroller | ELEGOO ESP32 Dev Board (38-pin WROOM, CP2102) | Any `esp32dev` board works |
| Servo Controller | Lynxmotion SSC-32U | USB + full-duplex UART |
| Audio Amplifier | Adafruit MAX98357A I2S Breakout — PID: 3006 | 3W Class D, I2S input |
| Speaker | Adafruit Mono Enclosed Speaker 3W 4Ω — PID: 4445 | 2.8" × 1.2", bare wire leads |
| Servo PSU | Mean Well GST60A07-P1J (7.5V 5A, 37.5W) | See Power Supplies section |
| Logic PSU | Mean Well GST25A05-P1J (5V 4A, 20W) | Powers ESP32 + MAX98357A |

---

## Power Supplies

Three separate power domains — **keep them isolated**.

### Domain 1 — Servos (6–7.5V, high current)

Mean Well's GST desktop adapter series doesn't offer 6V — it jumps from 5V to 7V to 7.5V. The recommended approach is:

**Mean Well GST60A07-P1J** — 7.5V, 5A, 37.5W desktop adapter.
The SSC-32U has an adjustable trimmer on the VS+ rail — trim down to 6V if your servos prefer it.

- IEC C14 inlet — requires IEC C13 power cord (not included)
- 2.1×5.5mm barrel output, centre positive
- Connects to SSC-32U **VS+** (red) and **GND** (black) terminals

### Domain 2 — ESP32 + Audio (5V, low current)

**Mean Well GST25A05-P1J** — 5V, 4A, 20W desktop adapter.

- IEC C14 inlet — requires IEC C13 power cord (not included)
- Connects to ESP32 **VIN**; MAX98357A VIN connects to ESP32 5V pin

### Critical ground tie

```
SSC-32U GND  ──→  ESP32 GND    ← REQUIRED — UART won't work without this
```

### Power wiring summary

```
[Mean Well GST60A07-P1J]  ──→  SSC-32U VS+ / GND   (servo power only)
[Mean Well GST25A05-P1J]  ──→  ESP32 VIN / GND
                          ──→  MAX98357A VIN / GND
[Common ground tie]
  SSC-32U GND             ──→  ESP32 GND
```

> Add 100µF + 100nF bypass caps on MAX98357A VIN to suppress noise. Both GST units need an IEC C13 cord — not included.

---

## Wiring

### ESP32 → SSC-32U (UART Serial)

| ESP32 Pin | SSC-32U Pin | Notes |
|-----------|-------------|-------|
| GPIO 22 (TX) | RX | Commands to servo controller |
| GPIO 23 (RX) | TX | Done signal back to ESP32 |
| GND | GND | Common ground — required |

### ESP32 → MAX98357A (I2S Audio)

| ESP32 Pin | MAX98357A Pin | Notes |
|-----------|--------------|-------|
| GPIO 27 | BCLK | Bit clock |
| GPIO 14 | LRC | Left/Right clock |
| GPIO 13 | DIN | Serial audio data |
| 3.3V | SD_MODE | Tie high — always on |
| 5V | VIN | Power |
| GND | GND | Common ground |

**GAIN pin:** Float = 9dB (default) · GND = 12dB · 3.3V = 6dB

**Speaker (PID:4445):** connect bare wires directly to OUT+ and OUT−. No capacitor needed.

### SSC-32U Servo Channel Map

| Channel | Servo | Movement |
|---------|-------|----------|
| CH 0 | Hitec HS-53 | Beak open / close |
| CH 1 | Hitec HS-425BB | Head pan — left / right |
| CH 2 | Hitec HS-425BB | Head tilt — up / down |
| CH 3 | Hitec HS-645MG | Wings (single servo, both wings mechanically linked) |
| CH 4 | — | Unused |
| CH 5 | Hitec HS-425BB | Body bob |

---

## Software Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension recommended)

### 1. Upload filesystem

```bash
pio run --target uploadfs --environment esp32dev
```

Uploads `index.html`, `sounds.json`, and `positions.json`. Note: `config.json` is **not** shipped — it is created by the captive portal on first setup.

### 2. Upload firmware

```bash
pio run --target upload --environment esp32dev
```

### 3. First-time setup via captive portal

1. Power on the ESP32 — no `config.json` exists yet
2. On your phone or laptop, connect to the **`BirdSetup`** WiFi network
3. A setup page should pop up automatically (captive portal). If it doesn't, navigate to **http://192.168.4.1**
4. Fill in all fields and click **Save & Connect**
5. The device reboots and joins your WiFi network
6. Navigate to **http://`[hostname]`.local** (e.g. `http://raven1.local`)

### 4. Monitor serial output

```bash
pio device monitor --environment esp32dev
```

Expected boot output after setup:
```
[bird] Booting v4...
[device] Loaded: Raven 1  prefix: props/study/raven1
[wifi] Connected, IP: 192.168.x.x
[mdns] http://raven1.local
[web] Started
[bird] Ready — Raven 1  http://raven1.local  prefix: props/study/raven1
```

---

## Captive Portal Setup Fields

| Field | Example | Notes |
|-------|---------|-------|
| Device Name | `Raven 1` | Display label in the web UI header |
| Hostname | `raven1` | Used for `http://raven1.local` — lowercase, no spaces |
| Location | `props` | Top level of MQTT topic hierarchy |
| Room | `study` | Middle level — group of props in a space |
| Device ID | `raven1` | Bottom level — unique per bird |
| MQTT Broker | `homeassistant.local` | Hostname or IP of your broker |
| MQTT Port | `1883` | Standard MQTT port |
| WiFi SSID | `MyNetwork` | Your WiFi network name |
| WiFi Password | `••••••••` | Your WiFi password |

The three MQTT fields build the full prefix automatically:
```
props / study / raven1  →  props/study/raven1/sequence
```

---

## Multi-Prop Setup

Flash the **same firmware binary** to every bird. Configure each individually through the portal:

| Bird | Location | Room | Device | Full prefix |
|------|----------|------|--------|-------------|
| Raven 1 | props | study | raven1 | `props/study/raven1` |
| Raven 2 | props | study | raven2 | `props/study/raven2` |
| Parrot 1 | props | tiki_bar | parrot1 | `props/tiki_bar/parrot1` |
| Parrot 2 | props | tiki_bar | parrot2 | `props/tiki_bar/parrot2` |

### MQTT Wildcard Broadcasting

MQTT `+` wildcards let your show controller target whole rooms with one message:

```
# One specific bird
props/study/raven1/sequence

# All birds in the tiki bar simultaneously
props/tiki_bar/+/sequence

# Every bird in the entire show
props/+/+/sequence
```

### MMM Discovery — All Birds at Once

On show startup, publish `all` to `props/+/+/query`. Every bird will respond on its own `{prefix}/identity`, `{prefix}/sounds/list`, and `{prefix}/sequences/list` topics.

---

## MQTT Topics

All topics are relative to the device's configured prefix (e.g. `props/study/raven1`).

### Subscribe — Send to Bird

| Leaf topic | Example payload | Effect |
|------------|----------------|--------|
| `/command` | `beak_open`, `head_left`, `neutral`… | Single move |
| `/sequence` | `caw`, `alert`, `wingflap`, `idle`, `sleep` | Built-in sequence |
| `/sequence` | `my_sequence_name` | Saved keyframe sequence |
| `/sound` | `caw` | Play sound + bound sequence |
| `/audio` | `caw` | Audio only, no sequence |
| `/servo` | `0 1800 300` | Raw: channel, µs, time ms |
| `/query` | `all`, `sounds`, `sequences`, `identity` | Request data |

### Publish — Bird Sends

| Leaf topic | Payload | When |
|------------|---------|------|
| `/status` | status string | After every action |
| `/identity` | JSON: name, hostname, ip, prefix | On connect + identity query |
| `/sounds/list` | JSON sound library | On connect + query |
| `/sequences/list` | JSON sequence names | On connect + query |

---

## Web Interface

### Control Tab
Sequences, head D-pad, beak/wings, body bob, raw servo sliders, and custom keyframe sequences. The live indicator pill shows the active sequence.

### Sounds Tab
Upload WAV files via drag-and-drop, assign bound sequences, manage SPIFFS storage.

### Calibrate Tab
Tune every named servo position live — Test button moves the servo immediately. Save All persists to `positions.json`.

### Sequences Tab
Keyframe editor — pose the bird, Add Keyframe, repeat, name, save. Saved sequences appear immediately in Control tab and are playable via MQTT.

### Settings Tab *(new in v4)*
Edit all device configuration post-setup without the portal:
- Device name, hostname
- MQTT location / room / device ID (shows built prefix live)
- MQTT broker and port
- WiFi credentials
- **Reset to Setup AP** — wipes `config.json`, reboots as `BirdSetup`

> WiFi or MQTT changes trigger an automatic reboot to reconnect.

---

## HTTP API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/config` | Current device config JSON (password masked) |
| POST | `/api/config` | Update device config — reboots if WiFi/MQTT changed |
| POST | `/api/reconfigure` | Wipe config and reboot as setup AP |
| GET | `/api/sounds` | Sound library JSON |
| POST | `/api/sounds` | Update a sound entry |
| DELETE | `/api/sounds?file=name` | Remove sound + WAV |
| POST | `/api/upload` | Multipart WAV upload |
| GET | `/api/positions` | Calibrated servo positions |
| POST | `/api/positions` | Save positions |
| GET | `/api/sequences` | List all keyframe sequences |
| GET | `/api/sequences?name=x` | Get one sequence |
| POST | `/api/sequences` | Save or update a sequence |
| DELETE | `/api/sequences?name=x` | Delete a sequence |
| GET | `/api/spiffs` | `{total, used, free}` bytes |

---

## File Structure

```
raven-animatronic-v4/
├── platformio.ini
├── data/
│   ├── index.html          ← Web UI — Control, Sounds, Calibrate, Sequences, Settings tabs
│   ├── sounds.json         ← Sound library
│   ├── positions.json      ← Calibrated servo positions
│   └── sequences/          ← Saved keyframe sequences
├── include/
│   ├── raven_config.h      ← Hardware pins, channel assignments, compile-time defaults only
│   ├── raven_device.h      ← Runtime config — loads config.json, device identity + MQTT prefix
│   ├── raven_captive.h     ← Captive portal — AP mode, DNS server, setup page
│   ├── raven_positions.h   ← Load/save calibrated servo positions
│   ├── raven_servo.h       ← SSC-32U UART and named moves
│   ├── raven_audio.h       ← I2S audio driver and sound library
│   ├── raven_sequences.h   ← Built-in sequence animations
│   ├── raven_keyframes.h   ← Keyframe player and CRUD
│   ├── raven_webui.h       ← ESPAsyncWebServer, WebSocket, HTTP API
│   ├── raven_mqtt.h        ← MQTT client — runtime topic prefix, identity announce
│   └── raven_mdns.h        ← mDNS — runtime hostname
└── src/
    └── main.cpp            ← Boot flow: captive portal check → WiFi → full init
```

> `config.json` is created on SPIFFS by the captive portal at first setup. It is not included in the firmware zip — never hardcode credentials in source files.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `BirdSetup` AP doesn't appear | Firmware not flashed | Check serial output, reflash |
| Setup popup doesn't appear | Captive portal not triggered | Navigate to http://192.168.4.1 manually |
| `raven1.local` not found after setup | mDNS blocked on network | Use IP address shown in serial output |
| Servos not responding | No UART comms | Check GPIO 22/23, SSC-32U baud 9600 |
| Servos jitter / ESP resets | Servo PSU on ESP32 rail | Isolate servo power to SSC-32U VS+ only |
| No audio | I2S wiring | Check GPIO 27/14/13, SD_MODE tied to 3.3V |
| MQTT not connecting | Wrong broker / port | Check Settings tab, verify broker is reachable |
| Multiple birds, same MQTT client ID | Not possible in v4 | Client IDs are now randomised per connection |
| Need to change WiFi after setup | — | Settings tab → update SSID/password → Save (auto-reboots) |
| Completely stuck / wrong config | — | Settings tab → Reset to Setup AP |

---

## Resources

- [Mr. Chicken's Build Resources (Google Drive)](https://drive.google.com/drive/u/0/folders/1Ht4HYf0q8vE1VhdCadyyANCFJWSVyEcL)
- [Raven Builders Facebook Group](https://www.facebook.com/groups/185149192169022/)
- [Lynxmotion SSC-32U Documentation](https://www.lynxmotion.com/c-153-ssc-32u.aspx)
- [Adafruit MAX98357A Guide](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp)
- [Mean Well GST60A07-P1J Datasheet](https://www.meanwell.com/Upload/PDF/GST60A/GST60A-SPEC.PDF)
- [PlatformIO ESP32 Reference](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html)

---

*Controller firmware developed with Claude (Anthropic). Raven mechanic kit by Mr. Chicken's Prop Shop / Jasper J. Anderson FX.*
