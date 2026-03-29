# Animatronic Bird Controller — v4.1

ESP32-based WiFi controller for animatronic props built around the [Mr. Chicken's Prop Shop Animatronic Raven Kit](https://chickenprops.com/products/animatronic-raven-kit) and compatible builds. v4 adds a **captive portal for first-time WiFi/MQTT setup**, **full runtime device configuration** (no hardcoded credentials), and a **three-level MQTT topic hierarchy** designed for multi-prop show environments. v4.1 hardens the firmware for show reliability with OTA updates, a hardware watchdog, MQTT last will, and WiFi auto-reconnect.

> **Upgrading from v3?** The only breaking change is that WiFi and MQTT credentials are no longer in `raven_config.h`. On first boot v4 will launch the setup AP — configure once, then it behaves exactly like v3.

---

## Version History

| Version | Summary |
| --- | --- |
| **v4.1.3** | INA219 current monitor: real-time voltage, current (mA), and power (mW) on the +7.5V servo rail — published to `{prefix}/power` via MQTT, WebSocket `power:` push every 5 s, and `GET /api/power`; Bug fix: `soundPlay` was racing two audio tasks when a sound had a bound sequence, causing silent playback — sequence now owns its own audio |
| **v4.1.2** | Audio fix: `i2s_set_clk` now always uses `I2S_BITS_PER_SAMPLE_16BIT` regardless of source file — 8-bit WAVs are upsampled to 16-bit before write, passing 8-bit to the I2S peripheral caused a malformed stream and MAX98357A fault; I2S clock stopped after playback to prevent amp auto-shutdown on sustained silence; GPIO pins corrected to match PCB v1.0 schematic (UART TX/RX: GPIO 17/16; I2S BCLK/LRC/DOUT: GPIO 26/27/25) |
| **v4.1.1** | Bug fixes: watchdog API corrected for installed toolchain (`esp_task_wdt_init` replaces IDF 5.x struct API); captive portal now validates Device Name and Hostname as required fields; firmware fallback populates blank name/hostname from Device ID so mDNS always starts |
| **v4.1** | Show hardening: OTA firmware updates over WiFi, hardware watchdog (30s), MQTT last will (`offline`/`online` on `{prefix}/status`), WiFi auto-reconnect with exponential backoff |
| **v4.0** | Captive portal first-time setup (BirdSetup AP), runtime config.json, three-level MQTT topic hierarchy (location/room/device), Settings tab in web UI, Reset to AP mode button, identity announcement on MQTT connect |
| **v3.0** | mDNS (`http://bird.local`), live sequence indicator, SPIFFS-persisted servo calibration, keyframe sequence editor, MQTT Home Assistant discovery |
| **v2.0** | I2S audio (MAX98357A), dynamic sound library, WAV upload via web UI |
| **v1.0** | Initial release: DAC audio, static soundboard, ESPAsyncWebServer, WebSocket control |

---

## Planned Features (v5+)

| Feature | Notes |
| --- | --- |
| Kit-agnostic architecture | `kit.json` defines servo mapping, NeoPixel config, and personality per prop type |
| Glowing eyes | WS2812B/WS2813 LEDs behind glass lenses — daisy-chained on NeoPixel line as pixels 1 & 2, status LED becomes pixel 0. Eye color, effects, and blink timing defined per-kit in kit.json |
| Random idle behavior | Background task — random head/beak movements between cues, configurable interval |
| Conductor mode | One MQTT message fans out a timed sequence to multiple birds in a room |
| Startup sync | All birds in a room wait for a sync message before moving to neutral |
| Export/import config | Download full config as zip, clone to new bird by uploading |
| OTA SPIFFS update | Currently OTA covers firmware only — add filesystem update support |
| Physical trigger input | 3.5mm jack → GPIO 34, fires configurable sequence on contact closure |
| ~~Current sensing~~ | ✅ Done in v4.1.3 — INA219 on VS+ rail, live power readings via MQTT/WebSocket/HTTP |
| Audio volume control | Configurable gain via MAX98357A GAIN pin |
| Audio ducking | Lower ambient audio level during cue sequences |
| Looping ambient audio | `/audio/loop` command for continuous background sound |
| Cue numbers | Numeric `/cue` topic mapping integers to sequence names for QLab/Isadora |
| Status LED | NeoPixel showing WiFi/MQTT/sequence state |

---

## What's New in v4.1 — Show Hardening

* **OTA firmware updates** — flash new firmware over WiFi without USB. Password defaults to the device's Device ID (e.g. `raven1`). To use, uncomment three lines in `platformio.ini` and run upload normally — PlatformIO finds the bird on the network automatically.
* **Hardware watchdog** — 30-second timeout. If the firmware hangs for any reason the ESP32 automatically reboots. Armed after all init completes to avoid false trips during startup.
* **MQTT last will** — the broker automatically publishes `offline` to `{prefix}/status` the moment a bird drops off unexpectedly. When the bird reconnects it publishes `online`, clearing the retained message. Your show controller always knows the true state of every prop.
* **WiFi auto-reconnect** — if WiFi drops mid-show, the firmware retries automatically with exponential backoff: 5s → 10s → 20s → 40s → 60s (cap), resetting to 5s on successful reconnect. MQTT reconnects automatically once WiFi is restored.

---

## What's New in v4.0

* **Captive portal setup** — on first boot (or after a reset) the ESP32 becomes a WiFi access point called `BirdSetup`. Connect any phone or laptop, get an automatic popup, fill in WiFi and MQTT details, save. Done.
* **Runtime device configuration** — all identity, WiFi, and MQTT settings live in `config.json` on SPIFFS. Edit from the new Settings tab in the web UI without reflashing.
* **Three-level MQTT topic hierarchy** — `location/room/device` (e.g. `props/tiki_bar/parrot1`). Supports MQTT wildcard broadcasting to whole rooms.
* **Settings tab** — edit device name, hostname, MQTT prefix, broker, and WiFi from the browser. Auto-reboots if WiFi or MQTT settings change.
* **Reset to AP mode** — one button in Settings wipes `config.json` and restarts as `BirdSetup` for reconfiguration.
* **Identity announcement** — on MQTT connect the bird publishes its name, hostname, IP, and full topic prefix to `{prefix}/identity` so show controllers always know what's on the network.
* **Multi-prop ready** — flash the same firmware to every bird. Configure each individually through the portal. Use MQTT wildcards to control whole rooms at once.

---

## Hardware

### Raven Mechanic Kit

**Mr. Chicken's Prop Shop — Animatronic Raven Kit**

* <https://chickenprops.com/products/animatronic-raven-kit>
* Available in **Basic** (mechanical parts only), **Deluxe** (Basic + servos), and **Bare Bones** (custom parts only) configurations

### Servos (per kit documentation)

| Qty | Part | Role |
| --- | --- | --- |
| 1× | Hitec HS-53 | Beak open/close |
| 3× | Hitec HS-425BB | Head pan, head tilt, body bob |
| 1× | Hitec HS-645MG | Wings (single servo, both wings mechanically linked) |

> The Deluxe Kit includes all servos. Check specs before substituting — voltage and torque ranges matter.

### Electronics

| Component | Exact Part | Notes |
| --- | --- | --- |
| Microcontroller | DOIT ESP32 DevKit V1 (30-pin WROOM) | Any `esp32dev` board works |
| Carrier PCB | Raven Animatronic Controller PCB v1.0 | 100×60mm, JLCPCB fabricated — see `/pcb` directory |
| Servo Controller | Lynxmotion SSC-32U | USB + full-duplex UART |
| Audio Amplifier | Adafruit MAX98357A I2S Breakout — PID: 3006 | 3W Class D, I2S input |
| Speaker | Adafruit Mono Enclosed Speaker 3W 4Ω — PID: 4445 | 2.8" × 1.2", bare wire leads |
| Current Monitor | INA219BIDR (SOIC-8) | Monitors servo current on +7.5V rail, I2C address 0x40 |
| Servo PSU | Mean Well GST60A07-P1J (7.5V 5A, 37.5W) | See Power Supplies section |
| Logic PSU | Mean Well GST25A05-P1J (5V 4A, 20W) | Powers ESP32 + MAX98357A |

---

## Power Supplies

Three separate power domains — **keep them isolated**.

### Domain 1 — Servos (6–7.5V, high current)

Mean Well's GST desktop adapter series doesn't offer 6V — it jumps from 5V to 7V to 7.5V. The recommended approach is:

**Mean Well GST60A07-P1J** — 7.5V, 5A, 37.5W desktop adapter.
The SSC-32U has an adjustable trimmer on the VS+ rail — trim down to 6V if your servos prefer it.

* IEC C14 inlet — requires IEC C13 power cord (not included)
* 2.1×5.5mm barrel output, centre positive
* Connects to SSC-32U **VS+** (red) and **GND** (black) terminals

### Domain 2 — ESP32 + Audio (5V, low current)

**Mean Well GST25A05-P1J** — 5V, 4A, 20W desktop adapter.

* IEC C14 inlet — requires IEC C13 power cord (not included)
* Connects to ESP32 **VIN**; MAX98357A VIN connects to ESP32 5V pin

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

> **Carrier PCB v1.0:** connect via J5 (XHB-4A). +7.5V servo power is routed through R_SHUNT on the carrier board.

| ESP32 Pin | J5 Pin | SSC-32U Pin | Notes |
| --- | --- | --- | --- |
| GPIO 17 (TX2) | 3 | RX | Commands to servo controller |
| GPIO 16 (RX2) | 2 | TX | Done signal back to ESP32 |
| GND | 1 | GND | Common ground — required |
| +7.5V | 4 | VS+ | Servo power via R_SHUNT |

### ESP32 → MAX98357A (I2S Audio)

| ESP32 Pin | MAX98357A Pin | Notes |
| --- | --- | --- |
| GPIO 26 | BCLK | Bit clock |
| GPIO 27 | LRC | Left/Right clock |
| GPIO 25 | DIN | Serial audio data |
| 3.3V | SD_MODE | Tie high — always on |
| 5V | VIN | Power |
| GND | GND | Common ground |

**GAIN pin:** Float = 9dB (default) · GND = 12dB · 3.3V = 6dB

**Speaker (PID:4445):** connect bare wires directly to OUT+ and OUT−. No capacitor needed.

### ESP32 → INA219 (I2C Current Monitor)

| ESP32 Pin | INA219 Pin | Notes |
| --- | --- | --- |
| GPIO 22 | SDA | I2C data |
| GPIO 21 | SCL | I2C clock |
| 3.3V | VS | Power |
| GND | GND | Common ground |

I2C address: **0x40** (A0 and A1 tied to GND)

Shunt resistor R_SHUNT: 0.1Ω 2W 2512 on +7.5V rail between J3 and J5.

### NeoPixel Chain

The NeoPixel connector J8 (XHB-3A) supports a daisy-chained WS2812B/WS2813 LED string:

| Pixel index | Function | Notes |
| --- | --- | --- |
| 0 | Status LED | Shows WiFi/MQTT/sequence state |
| 1 | Eye left | Optional — enabled in kit.json |
| 2 | Eye right | Optional — enabled in kit.json |

For builds without eyes, set `neopixel.count = 1` in kit.json.

**WS2813 recommended** for production props — dual data line prevents chain failure if one LED dies.

Add a **300–500Ω resistor** in series on the DAT line at J8 for noise immunity on long runs alongside servo PWM wiring.

### SSC-32U Servo Channel Map

| Channel | Servo | Movement |
| --- | --- | --- |
| CH 0 | Hitec HS-53 | Beak open / close |
| CH 1 | Hitec HS-425BB | Head pan — left / right |
| CH 2 | Hitec HS-425BB | Head tilt — up / down |
| CH 3 | Hitec HS-645MG | Wings (single servo, both wings mechanically linked) |
| CH 4 | — | Unused |
| CH 5 | Hitec HS-425BB | Body bob |

---

## Carrier PCB

PCB design files are in the `/pcb` directory:

| File | Description |
| --- | --- |
| `Gerber_PCB1_2026-03-14.zip` | Gerber files for JLCPCB fabrication |
| `BOM_Board1_PCB1_2026-03-14.xlsx` | Bill of Materials |
| `CPL_JLCPCB.xlsx` | Pick and Place file for JLCPCB assembly |
| `firmware-pcb-v1-notes.md` | Full pin change notes for v5.x firmware |

**Board specs:** 100×60mm · 2-layer · 3mm corner radius · M3 corner mounting holes

**Connectors:**

| Ref | Type | Function |
| --- | --- | --- |
| J3 | XHB-3A | +7.5V power in (pin 1=+7.5V, pin 2=GND, pin 3=NC) |
| J4 | XHB-2A | +5V power in (pin 1=+5V, pin 2=GND) |
| J5 | XHB-4A | SSC-32U (pin 1=GND, pin 2=RX, pin 3=TX, pin 4=+7.5V) |
| J7b | XHB-3A | Trigger jack landing (pin 1=+3.3V, pin 2=SIG, pin 3=GND) |
| J8 | XHB-3A | NeoPixel (pin 1=+5V, pin 2=DAT, pin 3=GND) |

**Hand-install after JLCPCB assembly:**
- DOIT ESP32 DevKit V1 30-pin (into H1/H2 female headers — LCSC C7499333)
- Adafruit MAX98357A breakout PID:3006 (into U2 header, secured with M2.5 screws at U8/U9)

---

## kit.json — NeoPixel / Eyes Configuration

The v5.x kit.json spec includes a `neopixel` block for per-kit LED personality:

**Parrot (with glowing eyes):**
```json
{
  "kit": "parrot",
  "neopixel": {
    "count": 3,
    "pixels": {
      "status": 0,
      "eye_left": 1,
      "eye_right": 2
    },
    "eyes": {
      "enabled": true,
      "idle_color": [255, 60, 0],
      "idle_effect": "breathe",
      "trigger_color": [255, 0, 0],
      "trigger_effect": "snap",
      "blink_enabled": true,
      "blink_interval_min_ms": 3000,
      "blink_interval_max_ms": 8000
    }
  }
}
```

**Raven (status LED only):**
```json
{
  "kit": "raven",
  "neopixel": {
    "count": 1,
    "pixels": {
      "status": 0
    },
    "eyes": {
      "enabled": false
    }
  }
}
```

---

## Software Setup

### Prerequisites

* [PlatformIO](https://platformio.org/) (VS Code extension recommended)

### 1. Upload filesystem

```
pio run --target uploadfs --environment esp32dev
```

Uploads `index.html`, `sounds.json`, and `positions.json`. Note: `config.json` is **not** shipped — it is created by the captive portal on first setup.

### 2. Upload firmware

```
pio run --target upload --environment esp32dev
```

### 3. First-time setup via captive portal

1. Power on the ESP32 — no `config.json` exists yet
2. On your phone or laptop, connect to the **`BirdSetup`** WiFi network
3. A setup page should pop up automatically (captive portal). If it doesn't, navigate to **<http://192.168.4.1>**
4. Fill in all fields and click **Save & Connect**
5. The device reboots and joins your WiFi network
6. Navigate to **http://`[hostname]`.local** (e.g. `http://raven1.local`)

### 4. Monitor serial output

```
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
| --- | --- | --- |
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
| --- | --- | --- | --- | --- |
| Raven 1 | props | study | raven1 | `props/study/raven1` |
| Raven 2 | props | study | raven2 | `props/study/raven2` |
| Parrot 1 | props | tiki\_bar | parrot1 | `props/tiki_bar/parrot1` |
| Parrot 2 | props | tiki\_bar | parrot2 | `props/tiki_bar/parrot2` |

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

### Discovery — All Birds at Once

On show startup, publish `all` to `props/+/+/query`. Every bird will respond on its own `{prefix}/identity`, `{prefix}/sounds/list`, and `{prefix}/sequences/list` topics.

---

## MQTT Topics

All topics are relative to the device's configured prefix (e.g. `props/study/raven1`).

### Subscribe — Send to Bird

| Leaf topic | Example payload | Effect |
| --- | --- | --- |
| `/command` | `beak_open`, `head_left`, `neutral`… | Single move |
| `/sequence` | `caw`, `alert`, `wingflap`, `idle`, `sleep` | Built-in sequence |
| `/sequence` | `my_sequence_name` | Saved keyframe sequence |
| `/sound` | `caw` | Play sound + bound sequence |
| `/audio` | `caw` | Audio only, no sequence |
| `/servo` | `0 1800 300` | Raw: channel, µs, time ms |
| `/query` | `all`, `sounds`, `sequences`, `identity`, `power` | Request data |

### Publish — Bird Sends

| Leaf topic | Payload | When |
| --- | --- | --- |
| `/status` | status string | After every action |
| `/identity` | JSON: name, hostname, ip, prefix | On connect + identity query |
| `/sounds/list` | JSON sound library | On connect + query |
| `/sequences/list` | JSON sequence names | On connect + query |
| `/power` | `{"available":true,"bus_v":7.42,"current_mA":123.4,"power_mW":915.2}` | On power query; WebSocket push every 5 s |

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

* Device name, hostname
* MQTT location / room / device ID (shows built prefix live)
* MQTT broker and port
* WiFi credentials
* **Reset to Setup AP** — wipes `config.json`, reboots as `BirdSetup`

> WiFi or MQTT changes trigger an automatic reboot to reconnect.

---

## HTTP API Reference

| Method | Endpoint | Description |
| --- | --- | --- |
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
| GET | `/api/power` | INA219 reading: `{available, bus_v, current_mA, power_mW}` |

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
│   ├── raven_mdns.h        ← mDNS — runtime hostname
│   └── raven_power.h       ← INA219 current monitor — I2C, voltage/current/power readings
├── pcb/
│   ├── Gerber_PCB1_2026-03-14.zip
│   ├── BOM_Board1_PCB1_2026-03-14.xlsx
│   ├── CPL_JLCPCB.xlsx
│   └── firmware-pcb-v1-notes.md
└── src/
    └── main.cpp            ← Boot flow: captive portal check → WiFi → full init
```

> `config.json` is created on SPIFFS by the captive portal at first setup. It is not included in the firmware zip — never hardcode credentials in source files.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `BirdSetup` AP doesn't appear | Firmware not flashed | Check serial output, reflash |
| Setup popup doesn't appear | Captive portal not triggered | Navigate to <http://192.168.4.1> manually |
| `raven1.local` not found after setup | mDNS blocked on network | Use IP address shown in serial output |
| Servos not responding | Wrong UART pins | Check GPIO 16/17 on carrier PCB v1.0 |
| Servos jitter / ESP resets | Servo PSU on ESP32 rail | Isolate servo power to SSC-32U VS+ only |
| No audio | I2S wiring or WAV format | Check GPIO 26/27/25; ensure WAVs are 8-bit or 16-bit mono — firmware upsamples 8-bit to 16-bit internally |
| I2C not working | SDA/SCL swapped | GPIO 22=SDA, GPIO 21=SCL on carrier PCB v1.0 |
| MQTT not connecting | Wrong broker / port | Check Settings tab, verify broker is reachable |
| Multiple birds, same MQTT client ID | Not possible in v4 | Client IDs are now randomised per connection |
| Need to change WiFi after setup | — | Settings tab → update SSID/password → Save (auto-reboots) |
| Completely stuck / wrong config | — | Settings tab → Reset to Setup AP |

---

## Resources

* [Mr. Chicken's Build Resources (Google Drive)](https://drive.google.com/drive/u/0/folders/1Ht4HYf0q8vE1VhdCadyyANCFJWSVyEcL)
* [Raven Builders Facebook Group](https://www.facebook.com/groups/185149192169022/)
* [Lynxmotion SSC-32U Documentation](https://www.lynxmotion.com/c-153-ssc-32u.aspx)
* [Adafruit MAX98357A Guide](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp)
* [Mean Well GST60A07-P1J Datasheet](https://www.meanwell.com/Upload/PDF/GST60A/GST60A-SPEC.PDF)
* [PlatformIO ESP32 Reference](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html)

---

*Controller firmware developed with Claude (Anthropic). Raven mechanic kit by Mr. Chicken's Prop Shop / Jasper J. Anderson FX.*
