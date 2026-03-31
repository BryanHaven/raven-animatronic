# Raven Animatronic Controller — Show File Spec
## Feature: Pre-produced Audio + Motion Sync (v5.x)

**Prepared for:** Claude Code  
**Date:** 2026-03-31  
**Firmware base:** v4.1.3  
**Hardware dependency:** v1.2 carrier PCB (SD card reader)  
**Local repo:** C:\Users\hobbi\OneDrive\E2W\Raven\raven-animatronic-v4.1

---

## Current Hardware Pin State (from raven_config.h)

All pins already defined — no TBD values remain:

```cpp
// SSC-32U UART
SSC_TX_PIN  17
SSC_RX_PIN  16

// INA219 I2C
INA219_SDA  22
INA219_SCL  21

// I2S Audio (MAX98357A)
I2S_BCLK    26
I2S_LRC     27
I2S_DOUT    25
I2S_SD_MODE 32   // GPIO-controlled mute
I2S_GAIN    33   // GPIO-controlled gain

// Trigger inputs (input-only GPIOs)
TRIGGER_1_IN 34
TRIGGER_2_IN 35

// SD card (VSPI)
SD_SCK      19
SD_MISO     23
SD_MOSI     18
SD_CS        5
SD_CD       36   // card detect, active LOW, INPUT_PULLUP

// Power Good (input-only)
PWR_GOOD    39   // Pololu D24V22F5 PG pin
```

**All GPIOs accounted for.** No free general-purpose GPIOs remain on the
30-pin DOIT DevKit V1. The GPIO map is locked for v1.2.

---

## Existing Module Pattern (follow this in raven_show.h)

All modules follow the same header-only pattern seen in `raven_power.h`:

```cpp
#pragma once
#include "raven_config.h"
// ... other includes

// module-level statics
static bool moduleAvail = false;

// init function called from main.cpp setup()
void moduleBegin() { ... }

// called from main.cpp loop() if needed
void moduleLoop() { ... }

// data accessor returning JSON string for WebUI/MQTT
String moduleToJson() { ... }
```

Follow this exact pattern for `raven_show.h`. Keep it header-only,
no .cpp file needed.

---

## Overview

A show file pairs a WAV audio track with a timeline of servo cue events.
Both start at t=0 on the ESP32 and stay in sync locally — no network jitter,
no conductor required. The show controller (M3, QLab, etc.) triggers playback
via a single MQTT message. From that point the bird runs autonomously until
the show completes or is stopped.

This is distinct from:
- **Sequences** (`raven_sequences.h`) — short built-in animations, no audio sync
- **Keyframes** (`raven_keyframes.h`) — user-recorded poses, no audio sync
- **Sound** (`/sound` MQTT topic) — plays audio + fires a bound sequence, no
  precise timing

---

## File Layout on SD Card

```
/shows/
├── tiki_caw.json        ← cue timeline
├── tiki_caw.wav         ← audio track (matched pair)
├── greeting.json
├── greeting.wav
└── ...
```

- Show name = base filename without extension
- JSON and WAV must share the same base name in `/shows/`
- WAV format: 16-bit PCM, 44100Hz or 22050Hz, mono
- JSON loaded into RAM at trigger; WAV streams from SD during playback
- Show name: max 32 chars, lowercase, no spaces (underscores OK)

---

## Show File JSON Schema

```json
{
  "show": "tiki_caw",
  "version": 1,
  "audio": "tiki_caw.wav",
  "duration_ms": 4200,
  "loop": false,
  "cues": [
    {
      "t_ms": 0,
      "ch": 0,
      "pos": 1500,
      "spd": 0,
      "comment": "beak neutral at start"
    },
    {
      "t_ms": 350,
      "ch": 0,
      "pos": 1900,
      "spd": 800
    },
    {
      "t_ms": 600,
      "ch": 0,
      "pos": 1500,
      "spd": 600
    },
    {
      "t_ms": 1200,
      "ch": 1,
      "pos": 1800,
      "spd": 400,
      "comment": "head turn right"
    },
    {
      "t_ms": 1800,
      "ch": 1,
      "pos": 1500,
      "spd": 400
    },
    {
      "t_ms": 2100,
      "ch": 0,
      "pos": 1900,
      "spd": 1000
    },
    {
      "t_ms": 2300,
      "ch": 0,
      "pos": 1500,
      "spd": 700
    },
    {
      "t_ms": 4000,
      "ch": 0,
      "pos": 1500,
      "spd": 0,
      "comment": "return to neutral"
    }
  ]
}
```

### Field Reference

**Top level:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `show` | string | yes | Must match filename base |
| `version` | int | yes | Schema version — always 1 |
| `audio` | string | yes | WAV filename in `/shows/` |
| `duration_ms` | int | yes | Total show length in ms |
| `loop` | bool | no | Restart when complete. Default false |
| `cues` | array | yes | Servo events, sorted by t_ms |

**Per cue:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `t_ms` | int | yes | Milliseconds from show start (t=0) |
| `ch` | int | yes | SSC-32U channel — use CH_* defines from raven_config.h |
| `pos` | int | yes | Target position µs (500–2500) |
| `spd` | int | yes | Move speed µs/sec. 0 = max speed |
| `comment` | string | no | Ignored at runtime, for authoring only |

**Constraints:**
- Cues MUST be sorted by `t_ms` ascending — reject unsorted files with error
- Multiple cues at same `t_ms` are valid (fire multiple channels together)
- Valid `ch` values: 0 (CH_BEAK), 1 (CH_HEAD_PAN), 2 (CH_HEAD_TILT),
  3 (CH_WING), 5 (CH_BODY_BOB)
- `pos` clamped to 500–2500µs
- `spd` 0 = SSC-32U default (omit S field in command string)

---

## MQTT Interface

### Trigger a show
```
Topic:   {prefix}/show
Payload: "tiki_caw"
```
- Loads `/shows/tiki_caw.json` from SD
- Starts audio + cue playback immediately
- If a show is already playing, stop it first then start new one

### Stop a running show
```
Topic:   {prefix}/show
Payload: "stop"
```
- Stops audio immediately
- Moves all active servos to neutral positions at MOVE_SLOW speed

### Query available shows
```
Topic:   {prefix}/query
Payload: "shows"
```
Reply:
```
Topic:   {prefix}/shows/list
Payload: ["tiki_caw","greeting","farewell"]
```

### Status during playback
```
Topic:   {prefix}/status
Payload: "show:tiki_caw:playing:2450"    ← name:state:elapsed_ms
         "show:tiki_caw:complete"
         "show:tiki_caw:stopped"
         "show:tiki_caw:error:file_not_found"
         "show:tiki_caw:error:parse_fail"
         "show:tiki_caw:error:invalid_cues"
         "show:tiki_caw:error:audio_fail"
         "show:error:no_sd_card"
```

---

## New File: `include/raven_show.h`

Create this file. Follow the header-only module pattern from `raven_power.h`.
Do NOT modify `raven_audio.h`, `raven_sequences.h`, or `raven_keyframes.h`.

### Data Structures

```cpp
#pragma once
#include <SD.h>
#include <ArduinoJson.h>
#include "raven_config.h"
#include "raven_audio.h"
#include "raven_servo.h"
#include "raven_mqtt.h"

#define SHOW_MAX_CUES    128
#define SHOW_NAME_LEN     33
#define SHOW_AUDIO_LEN    64
#define SHOW_DIR        "/shows"

struct ShowCue {
    uint32_t t_ms;
    uint8_t  channel;
    uint16_t position;
    uint16_t speed;
};

struct ShowFile {
    char     name[SHOW_NAME_LEN];
    char     audio_file[SHOW_AUDIO_LEN];
    uint32_t duration_ms;
    bool     loop;
    ShowCue  cues[SHOW_MAX_CUES];
    uint8_t  cue_count;
};

enum ShowState {
    SHOW_IDLE,
    SHOW_LOADING,
    SHOW_PLAYING,
    SHOW_COMPLETE,
    SHOW_ERROR
};

static ShowFile   _show;
static ShowState  _showState    = SHOW_IDLE;
static uint32_t   _showStartMs  = 0;
static uint8_t    _nextCueIdx   = 0;
static bool       _sdAvailable  = false;
```

### Public API Functions

```cpp
// Called from main.cpp setup() after SD.begin()
void showBegin();

// Called from raven_mqtt.h when /show topic received
// name = show name to start, or "stop" to stop
bool showTrigger(const char* name);

// Called from main.cpp loop() — drives cue dispatch
void showLoop();

// Returns JSON string for WebUI/MQTT
String showToJson();

// Accessors
ShowState   showGetState();
uint32_t    showGetElapsed();
const char* showGetName();
```

### showBegin() Implementation

```cpp
void showBegin() {
    // SD.begin() must already be called in main.cpp setup()
    // before showBegin() is called
    _sdAvailable = SD.exists(SHOW_DIR);
    if (!_sdAvailable) {
        SD.mkdir(SHOW_DIR);
        _sdAvailable = true;
    }
    Serial.printf("[show] ready — shows dir: %s\n",
                  _sdAvailable ? "OK" : "MISSING");
}
```

### showTrigger() Implementation Notes

```cpp
bool showTrigger(const char* name) {
    // Handle "stop" payload
    if (strcmp(name, "stop") == 0) {
        // stop audio, return servos to neutral, publish status
        _showState = SHOW_IDLE;
        return true;
    }

    // Build path: /shows/tiki_caw.json
    char path[80];
    snprintf(path, sizeof(path), "%s/%s.json", SHOW_DIR, name);

    // Load and parse JSON
    // Use StaticJsonDocument sized for SHOW_MAX_CUES cues
    // Reject if cue_count > SHOW_MAX_CUES
    // Validate cues are sorted by t_ms

    // CRITICAL timing — set timestamp BEFORE starting audio
    _showStartMs = millis();
    _nextCueIdx  = 0;
    _showState   = SHOW_PLAYING;

    // Start audio — audioPlay() must accept an SD path string
    audioPlay(show.audio_file_with_path);

    return true;
}
```

### showLoop() Implementation Notes

```cpp
void showLoop() {
    if (_showState != SHOW_PLAYING) return;

    uint32_t elapsed = millis() - _showStartMs;

    // Dispatch all cues whose time has passed
    while (_nextCueIdx < _show.cue_count &&
           elapsed >= _show.cues[_nextCueIdx].t_ms) {
        ShowCue& c = _show.cues[_nextCueIdx];
        // Send SSC-32U command — use servoMove() from raven_servo.h
        // or format raw #ch Ppos Sspd\r string directly
        servoMove(c.channel, c.position, c.speed);
        _nextCueIdx++;
    }

    // Check completion — whichever comes first:
    // 1. All cues dispatched AND duration elapsed
    // 2. Audio reports complete (poll audioIsPlaying())
    if (elapsed >= _show.duration_ms || !audioIsPlaying()) {
        if (_show.loop) {
            // Restart
            _showStartMs = millis();
            _nextCueIdx  = 0;
            audioPlay(_show.audio_file_path);
        } else {
            _showState = SHOW_COMPLETE;
            // publish status complete
        }
    }
}
```

**Important:** Use `millis()` polling in `showLoop()` — do NOT use
FreeRTOS timers. The loop() approach is simpler and gives ±10ms precision
which is more than adequate for servo cues.

### SSC-32U Command Format

Matches the pattern already used in `raven_servo.h`:
```
#Ppos Sspd\r         ← single channel with speed
#Ppos\r              ← single channel, max speed (spd == 0)
```

Check `raven_servo.h` for the exact `servoMove()` signature and reuse it
rather than formatting raw strings.

### ArduinoJson Document Size

```cpp
// Size for SHOW_MAX_CUES=128 cues
// Each cue: ~5 ints = ~40 bytes
// Plus header fields, overhead
StaticJsonDocument<16384> doc;
```

If stack is tight use `DynamicJsonDocument<16384>` instead.

---

## Changes to Existing Files

### `include/raven_mqtt.h`
Add handler for `/show` topic in the existing topic routing:
```cpp
else if (strcmp(leaf, "show") == 0) {
    showTrigger(payload);
}
```

Add `shows` case to the existing `/query` handler:
```cpp
else if (strcmp(payload, "shows") == 0) {
    publishShowList();
}
```

Add `publishShowList()` — scan `/shows/` for `.json` files, publish base
names as JSON array to `{prefix}/shows/list`.

### `src/main.cpp`
Add in `setup()` after `SD.begin()`:
```cpp
showBegin();
```

Add in `loop()`:
```cpp
showLoop();
```

Include at top:
```cpp
#include "raven_show.h"
```

### `platformio.ini`
Verify `ArduinoJson` is in `lib_deps`. If not, add:
```ini
bblanchon/ArduinoJson @ ^6.21.0
```

Also verify `SD` library is included — it's part of the ESP32 Arduino core
so usually doesn't need a separate entry.

---

## Web UI Changes (`data/index.html`)

Add a **Shows** tab alongside the existing tabs (Control, Sounds, Calibrate,
Sequences, Settings).

**Shows tab contents:**
- List of shows fetched from `GET /api/shows`
- Play button per show — POSTs to MQTT `/show` topic via WebSocket
- Stop button — sends "stop" to `/show`
- Progress bar — updates via WebSocket driven by `showGetElapsed()`
- Current show name + elapsed time display

**New HTTP API endpoints in `raven_webui.h`:**

```
GET  /api/shows          → JSON array: ["tiki_caw","greeting"]
GET  /api/shows?name=x   → full JSON content of named show file
DELETE /api/shows?name=x → delete .json + .wav pair from SD
```

**File upload** (WAV + JSON pair) — use the existing multipart upload
infrastructure from the Sounds tab as a pattern. Upload endpoint:
```
POST /api/shows/upload   → multipart: field "json" + field "wav"
```

Both files must be present in the same request. Validate that base names
match before writing to SD.

---

## SD Card Initialization in main.cpp

The SD card init must happen in `setup()` before `showBegin()`.
Use the VSPI bus with the pins from `raven_config.h`:

```cpp
#include <SPI.h>
#include <SD.h>

// In setup():
SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
if (!SD.begin(SD_CS)) {
    Serial.println("[sd] card mount failed");
    // showBegin() will handle graceful degradation
} else {
    Serial.println("[sd] card mounted OK");
}
showBegin();
```

**Card detect (SD_CD = GPIO 36):** GPIO 36 is input-only with no internal
pull-up. Requires external pull-up on PCB (check v1.2 schematic). Read
with `digitalRead(SD_CD)` — LOW = card present.

---

## Show Authoring Workflow

1. **Record/produce audio** in Audacity — export as 16-bit PCM WAV, mono,
   44100Hz or 22050Hz. Name it `myshow.wav`.

2. **Find timestamps** — use Audacity's timeline to identify key moments
   (beak open on syllable, head turn on beat, wings at climax, etc.).
   Note timestamps in milliseconds.

3. **Write cue JSON** — create `myshow.json` matching the schema above.
   Set `duration_ms` to the WAV length in ms (shown in Audacity title bar).
   Sort cues by `t_ms`. Use channel numbers from raven_config.h:
   - 0 = beak, 1 = head pan, 2 = head tilt, 3 = wing, 5 = body bob

4. **Validate** — confirm cues are sorted, pos values are 500–2500, ch
   values are valid, `duration_ms` ≥ WAV length.

5. **Deploy** — copy both files to SD card `/shows/` directory, or use
   the Shows tab upload in the web UI.

6. **Trigger:**
   ```
   MQTT topic:   props/tiki_bar/raven1/show
   MQTT payload: myshow
   ```
   Or click Play in the Shows tab.

---

## Servo Channel Reference (from raven_config.h)

| Define | Channel | Servo | Use in cues |
|--------|---------|-------|-------------|
| CH_BEAK | 0 | Hitec HS-53 | Beak open/close |
| CH_HEAD_PAN | 1 | Hitec HS-425BB | Head left/right |
| CH_HEAD_TILT | 2 | Hitec HS-425BB | Head up/down |
| CH_WING | 3 | Hitec HS-645MG | Wings both |
| CH_BODY_BOB | 5 | Hitec HS-425BB | Body bob |

Default positions for reference when authoring neutral cues:
- BEAK_CLOSED=1500, BEAK_OPEN=1900
- HEAD_CENTRE=1500, HEAD_LEFT=1200, HEAD_RIGHT=1800
- HEAD_UP=1600, HEAD_DOWN=1400
- WING_FOLD=1000, WING_HALF=1500, WING_FULL=2000
- BOB_MID=1500, BOB_UP=1700, BOB_DOWN=1300

---

## Dependencies

| Requirement | Status | Notes |
|-------------|--------|-------|
| SD card pins | ✅ Mapped | SCK=19, MISO=23, MOSI=18, CS=5, CD=36 |
| ArduinoJson | Verify in platformio.ini | `bblanchon/ArduinoJson @ ^6.21.0` |
| SD library | ESP32 core | No separate install needed |
| audioIsPlaying() | Check raven_audio.h | May need to add if not present |
| servoMove() | Check raven_servo.h | Use existing function signature |
| v1.2 PCB | Hardware | SD card reader physically present |

---

## Future Extensions (not in initial implementation)

- **Web-based cue editor** — waveform display + drag-and-drop cue placement
  in the Shows tab browser UI
- **Multi-bird sync** — `sync_delay_ms` field in show JSON for coordinated
  playback across multiple props via `props/tiki_bar/+/show`
- **NeoPixel cues** — `"type": "neopixel"` cue events for eye flashes
- **Audio SFX overlays** — secondary audio events overlaid during a show

---

*Project: Raven Animatronic Controller v4.1.3 → v5.x*  
*Feature: Show File Audio+Motion Sync*  
*Spec version: 1.1 — updated with real pin assignments and module patterns*  
*Author: Bryan Haven*
