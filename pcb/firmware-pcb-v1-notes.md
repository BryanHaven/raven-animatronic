# Raven Animatronic Controller — PCB v1.0 Firmware Notes

## GPIO Pin Changes Required for v5.x Firmware

The carrier PCB v1.0 requires the following changes to GPIO assignments
from the v4.x firmware defaults. Update all pin definitions accordingly.

---

### I2S Audio (MAX98357A)

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| I2S_BCLK | GPIO 27 | GPIO 26 | Swapped to avoid trace crossing |
| I2S_LRC  | GPIO 26 | GPIO 27 | Swapped to avoid trace crossing |
| I2S_DOUT | GPIO 25 | GPIO 25 | Unchanged |

```cpp
// v1.0 PCB pin definitions
#define I2S_BCLK  26
#define I2S_LRC   27
#define I2S_DOUT  25
```

---

### I2C (INA219 Current Monitor)

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| I2C_SDA | GPIO 21 | GPIO 22 | Swapped to avoid trace crossing |
| I2C_SCL | GPIO 22 | GPIO 21 | Swapped to avoid trace crossing |

```cpp
// v1.0 PCB pin definitions
#define I2C_SDA  22
#define I2C_SCL  21

// Initialise I2C with swapped pins
Wire.begin(I2C_SDA, I2C_SCL);
```

---

### UART2 (SSC-32U Serial)

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| UART2_RX | GPIO 16 | GPIO 16 | Unchanged |
| UART2_TX | GPIO 17 | GPIO 17 | Unchanged |

---

### Trigger Input

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| TRIGGER_1_IN | GPIO 34 | GPIO 34 | Unchanged — input only pin |

---

### NeoPixel Status LED

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| NEOPIXEL | GPIO 4 | GPIO 4 | Unchanged |

---

### Audio Volume Control (MAX98357A GAIN pin)

**PCB change required before fabrication:** Route MAX98357A GAIN pin to **GPIO 33 (D33)**.

| Signal | Pin | Notes |
|--------|-----|-------|
| I2S_GAIN | GPIO 33 | MAX98357A GAIN — drive LOW=12dB, INPUT/float=9dB, HIGH=6dB |

**EasyEDA change:** Add trace from MAX98357A U2 GAIN pad to ESP32 header pin D33.

Three volume levels are achievable in firmware by switching the GPIO mode:
```cpp
pinMode(I2S_GAIN, OUTPUT); digitalWrite(I2S_GAIN, LOW);   // 12 dB (loudest)
pinMode(I2S_GAIN, INPUT);                                   //  9 dB (default / float)
pinMode(I2S_GAIN, OUTPUT); digitalWrite(I2S_GAIN, HIGH);   //  6 dB (quietest)
```

> If GAIN is left unconnected (no trace to GPIO 33) the amp defaults to 9 dB — behaviour is identical to v1.0.

---

### Audio Mute / Ducking (MAX98357A SD_MODE pin)

**PCB change required before fabrication:** Route MAX98357A SD_MODE pin to **GPIO 32 (D32)** instead of hard-tying to 3.3V.

| Signal | Pin | Notes |
|--------|-----|-------|
| I2S_SD_MODE | GPIO 32 | MAX98357A SD_MODE — OUTPUT HIGH=amp on, OUTPUT LOW=mute/shutdown |

**EasyEDA change:** Remove 3.3V tie on MAX98357A U2 SD_MODE pad; add trace to ESP32 header pin D32.

D32 and D33 are adjacent on the DevKit — route GAIN (D33) and SD_MODE (D32) together to avoid trace crossing.

```cpp
pinMode(I2S_SD_MODE, OUTPUT);
digitalWrite(I2S_SD_MODE, HIGH);   // amp on (normal operation)
digitalWrite(I2S_SD_MODE, LOW);    // mute / shutdown (audio ducking, power save)
```

> SD_MODE must be driven HIGH in firmware before audio playback. If the pin is left floating or driven LOW the amp stays silent.

---

### Second Trigger Input

**PCB change required before fabrication:** Add second 3.5mm trigger jack (J9) wired to **GPIO 35 (D35)**.

| Signal | Pin | Notes |
|--------|-----|-------|
| TRIGGER_2_IN | GPIO 35 | Secondary contact closure trigger — input-only pin |

**EasyEDA change:** Add J9 (XHB-3A, matching J7b) with pin 2 (SIG) routed to ESP32 GPIO 35. GPIO 35 is input-only — no pull-up/down resistor needed on pad; use internal INPUT_PULLUP in firmware.

J9 pinout matches J7b: pin 1=+3.3V, pin 2=SIG, pin 3=GND.

---

### I2C Expansion Header

**PCB change required before fabrication:** Expose I2C bus on a dedicated header (J10, XHB-4A or JST-PH 4-pin).

| Pin | Signal | Notes |
|-----|--------|-------|
| 1 | GND | Common ground |
| 2 | +3.3V | Breakout power |
| 3 | SDA (GPIO 22) | Shared with INA219 |
| 4 | SCL (GPIO 21) | Shared with INA219 |

**EasyEDA change:** Add J10 header pads tapped from the existing INA219 SDA/SCL traces. No additional routing required beyond the header footprint.

Allows future I2C peripherals (OLED display, additional sensors) without a PCB revision.

---

### SD Card (Audio Storage Expansion)

**PCB change required before fabrication:** Add micro-SD card socket using VSPI bus.

| Signal | Pin | Notes |
|--------|-----|-------|
| SD_SCK  | GPIO 19 | VSPI clock |
| SD_MISO | GPIO 23 | VSPI MISO |
| SD_MOSI | GPIO 18 | VSPI MOSI |
| SD_CS   | GPIO 5  | VSPI chip select — held HIGH at boot (safe for SD CS) |
| SD_CD   | GPIO 36 | Card detect — active LOW, INPUT_PULLUP; input-only, no boot concerns |

**EasyEDA change:** Add micro-SD socket footprint (e.g. Molex 104031-0811 or similar). Route VSPI signals to socket. Add 10kΩ pull-up on SD_CS to ensure it's HIGH during ESP32 boot strapping.

Provides gigabytes of WAV storage vs ~1.5 MB on SPIFFS. Uses the Arduino `SD.h` library.

---

### 5V Power Regulator — Pololu D24V22F5 (replaces AMS1117-5.0)

**PCB change required before fabrication:** Remove U4 (AMS1117-5.0) and replace with a socketed Pololu D24V22F5 5V/2.5A buck module. The AMS1117-5.0 1A limit is exceeded by the v1.1 load (ESP32 + audio amp + NeoPixels + SD card peak ~1.46A).

**EasyEDA change:** Delete U4 AMS1117 symbol and its tantalum input/output caps. Place a 1×5 female header symbol (label: "Pololu D24V22F5 socket"). Add bypass caps at socket.

| Socket pin | Signal | Connect to |
|------------|--------|------------|
| 1 | PG (Power Good) | GPIO 39 (input-only) |
| 2 | EN (Enable) | +6V rail (always on) |
| 3 | VIN | +6V rail (from J3) |
| 4 | GND | GND pour |
| 5 | VOUT | +5V net |

Bypass caps to add near socket:
- C_IN: 10µF X7R 0805 on VIN to GND (LCSC C15850)
- C_OUT: 22µF X7R 1210 on VOUT to GND (LCSC C13585)
- C_OUT2: 100nF X7R 0402 on VOUT to GND (LCSC C14663)

Socket LCSC part: **C124418** (1×5P female header 2.54mm through-hole)
Module: **Pololu D24V22F5** (#2858) — hand-install after JLCPCB assembly, same as ESP32 and MAX98357A breakout.

> Pin 1 (PG) is marked with a white dot on the Pololu module. Verify orientation before inserting.

```cpp
#define PWR_GOOD  39   // HIGH = 5V rail OK; LOW = overload/thermal shutdown

bool is5VRailOK() { return digitalRead(PWR_GOOD) == HIGH; }
```

---

## Connector Pinouts (J5 SSC-32U Cable Change)

### J5 — SSC-32U Connector (XHB-4A)

The +7.5V power was moved to **pin 4** (was pin 1) to shorten
the PCB trace path through R_SHUNT.

| Pin | Signal | Notes |
|-----|--------|-------|
| 1 | GND | |
| 2 | UART2_RX | ESP32 GPIO 16 |
| 3 | UART2_TX | ESP32 GPIO 17 |
| 4 | +7.5V | Power to SSC-32U |

**Cable wiring note:** The SSC-32U end uses Dupont female connectors.
Wire the XHB-4A plug to match the above pinout. The SSC-32U board
uses standard 0.1" headers — connect to the SSC-32U power and
serial header pins accordingly.

---

## New Hardware — INA219 Current Monitor

The v1.0 PCB adds an INA219BIDR current monitor on the +7.5V
servo power rail. I2C address is **0x40** (A0 and A1 both tied to GND).

### Shunt Resistor
- R_SHUNT = 0.1Ω (100mΩ), 2512 package, 2W
- Maximum measurable current: ~3.2A at default ±320mV range
- LCSC part: C37635799

### INA219 Configuration
```cpp
// INA219 I2C address
#define INA219_ADDRESS  0x40

// Shunt resistor value
#define SHUNT_OHMS      0.1f

// Initialise
Adafruit_INA219 ina219(INA219_ADDRESS);
ina219.begin();
```

---

## Complete v1.1 PCB Pin Definition Block

```cpp
// ── I2S Audio ──────────────────────────────
#define I2S_BCLK    26
#define I2S_LRC     27
#define I2S_DOUT    25

// ── I2C ────────────────────────────────────
#define I2C_SDA     22
#define I2C_SCL     21

// ── UART2 (SSC-32U) ────────────────────────
#define UART2_RX    16
#define UART2_TX    17

// ── Trigger Inputs ─────────────────────────
#define TRIGGER_1_IN  34   // primary — J7b
#define TRIGGER_2_IN  35   // secondary — J9

// ── NeoPixel ───────────────────────────────
#define NEOPIXEL     4

// ── Audio (MAX98357A) ───────────────────────
#define I2S_SD_MODE  32   // OUTPUT HIGH=on, OUTPUT LOW=mute
#define I2S_GAIN     33   // LOW=12dB, INPUT=9dB, HIGH=6dB

// ── SD Card (VSPI) ─────────────────────────
#define SD_SCK       19
#define SD_MISO      23
#define SD_MOSI      18
#define SD_CS         5
#define SD_CD        36   // active LOW, INPUT_PULLUP

// ── Power Good (Pololu D24V22F5) ───────────
#define PWR_GOOD     39   // HIGH = 5V rail OK; input-only

// ── I2C Expansion (shared with INA219) ─────
// Exposed on J10 header — tap from INA219 traces

// ── INA219 ─────────────────────────────────
#define INA219_ADDRESS  0x40
#define SHUNT_OHMS      0.1f
```

---

## PCB v1.0 Files

All PCB design files are in the `/pcb` directory of the repository:

- `Gerber_PCB1_2026-03-14.zip` — Gerber files for fabrication
- `BOM_Board1_PCB1_2026-03-14.xlsx` — Bill of Materials
- `CPL_JLCPCB.xlsx` — Pick and Place file for JLCPCB assembly
- EasyEDA project files

### Fabrication Notes
- **Board:** 100mm x 60mm, 2-layer, 3mm corner radius
- **Assembly:** JLCPCB SMT assembly service
- **ESP32:** DOIT DevKit V1 30-pin — hand install after assembly
- **MAX98357A:** Adafruit breakout PID 3006 — hand install after assembly
- **Mounting holes:** M3 at corners (U4-U7), M2.5 for MAX98357A breakout (U8-U9)

---

*Generated: 2026-03-15*
*PCB Design: Bryan Haven*
*Project: Raven Animatronic Controller v5.x*
