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
| TRIGGER_IN | GPIO 34 | GPIO 34 | Unchanged — input only pin |

---

### NeoPixel Status LED

| Signal | v4.x Pin | v1.0 PCB Pin | Notes |
|--------|----------|--------------|-------|
| NEOPIXEL | GPIO 4 | GPIO 4 | Unchanged |

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

## Complete v1.0 PCB Pin Definition Block

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

// ── Trigger Input ──────────────────────────
#define TRIGGER_IN  34

// ── NeoPixel ───────────────────────────────
#define NEOPIXEL    4

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
