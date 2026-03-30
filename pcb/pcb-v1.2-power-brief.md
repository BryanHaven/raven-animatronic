# Raven Animatronic Controller PCB v1.2
## Power Subsystem Design Brief — Pololu D24V22F5 5V Buck Module

**Purpose:** Replace the AMS1117-5.0 LDO (U4) with a socketed Pololu D24V22F5
fixed 5V buck regulator module to handle the expanded 5V logic load of v1.2
(ESP32 + MAX98357A + SD card reader + WS2812B/WS2813 NeoPixels + GPIO audio
gain/SD mode control).

---

## Recommended Approach — COTS Module Socket

Rather than a discrete TPS54331 buck converter design (complex layout, many
external components, switching noise risk), use a **Pololu D24V22F5** fixed
5V module socketed on a 1×5 0.1" female header. Benefits:

- **No external components** — complete switching regulator on a 17.8×17.8mm PCB
- **Fixed 5V output** — no trimpot to drift or miscalibrate in the field
- **Field replaceable** — drops in/out of socket, no soldering to replace
- **Production grade** — Pololu modules are well-characterized and reliable
- **Power Good signal** — D24V22F5 has PG pin for firmware power rail monitoring
- **~$15** from Pololu, Amazon, or Digi-Key

---

## Why the Change (AMS1117 Retirement)

The AMS1117-5.0 LDO is rated 1A continuous with a 1.3A absolute maximum.
The v1.2 load budget peaks at ~1.46A with all peripherals active simultaneously:

| Component            | Idle   | Peak    |
|----------------------|--------|---------|
| ESP32 dev board      | 150mA  | 500mA   |
| MAX98357A audio amp  | 10mA   | 800mA   |
| WS2812B/WS2813 ×3   | 1mA    | 60mA    |
| SD card module       | 0.5mA  | 100mA   |
| **Total**            | ~162mA | ~1.46A  |

The AMS1117 also dissipates (6V − 5V) × 1A = 1W as heat at full load.
The Pololu module runs at ~90% efficiency, dissipating only ~0.08W at typical load.

---

## Selected Module

**Pololu D24V22F5 — 5V, 2.5A Step-Down Voltage Regulator**

- Pololu item: #2858 (https://www.pololu.com/product/2858)
- Also available: Amazon, Digi-Key
- Input voltage: 4V–36V (effective minimum ~5.7V at full load due to dropout)
- Output voltage: Fixed 5V (±4% accuracy)
- Output current: 2.5A continuous typical
- Switching frequency: Internal (variable, optimized for efficiency)
- Efficiency: 85–95% at 6V in / 5V out
- Board dimensions: 0.7" × 0.7" (17.8mm × 17.8mm)
- Connections: 1×5 row of 0.1" (2.54mm) through-hole pads
- Protection: Reverse voltage, overcurrent, short circuit, thermal shutdown
- Power Good output: Yes (PG pin goes high when output is in regulation)

**Note on D24V25F5 vs D24V22F5:** The D24V22F5 is Pololu's current recommended
version — lower cost, same size, same current rating, adds PG pin. Use D24V22F5.
The D24V25F5 is the older version — still works but being phased out.

---

## Module Pinout

Pins are in a single 1×5 row on 0.1" spacing:

| Pin | Name | Function | Connect to |
|-----|------|----------|------------|
| 1 | PG | Power Good — high when 5V rail OK | Optional GPIO (e.g. GPIO 35) or NC |
| 2 | EN | Enable — active high | +6V (always on) or GPIO for shutdown |
| 3 | VIN | Input voltage | +6V rail from J3 |
| 4 | GND | Ground | GND |
| 5 | VOUT | 5V output | +5V net |

**Module orientation note:** Pin 1 (PG) is marked with a white dot on the Pololu
PCB. Verify orientation before inserting — reversing VIN/VOUT will destroy the
module despite the reverse-voltage protection on VIN.

---

## PCB Socket Design for v1.2

### Footprint
Place a **1×5 female header, 0.1" pitch, through-hole** on the carrier PCB
in the space previously occupied by U4 (AMS1117) and its input/output caps.

Suggested LCSC part for socket: **C124418** (1×5P female header 2.54mm) or
any standard through-hole 1×5 2.54mm female header in JLCPCB basic library.

### Placement
- The module sits perpendicular to the board surface on the socket
- Board height above carrier PCB: ~8mm (module + socket)
- Footprint required on carrier PCB: ~20×20mm clear area
- The 16.5mm gap between J5 and J3 on the left edge is the target location —
  place the socket centered in that gap

### Connections on carrier PCB (at socket)
```
Socket pin 1 (PG)   → GPIO 35 trace (input-only, 3.3V tolerant) OR leave NC
Socket pin 2 (EN)   → +6V rail (ties through to pull-up on module)
Socket pin 3 (VIN)  → +6V rail (from J3, before R_SHUNT)
Socket pin 4 (GND)  → GND pour
Socket pin 5 (VOUT) → +5V net (to ESP32 VIN, MAX98357A, J8 NeoPixel)
```

### Bypass capacitors
Add near the socket on the carrier PCB:
- **C_IN**: 10µF ceramic X7R 0805 on VIN to GND (input decoupling)
- **C_OUT**: 22µF ceramic X7R 1210 on VOUT to GND (output bulk)
- **C_OUT2**: 100nF ceramic 0402 on VOUT to GND (high-freq bypass)

These supplement the module's onboard capacitors and improve transient response
during ESP32 WiFi spikes.

---

## Power Good (PG) Pin — Optional Firmware Use

If PG is connected to GPIO 35 (or any input-only GPIO):

```cpp
#define PIN_5V_PG  35   // Power Good from Pololu D24V22F5

void setup() {
    pinMode(PIN_5V_PG, INPUT);
}

// Check 5V rail health
bool is5VRailOK() {
    return digitalRead(PIN_5V_PG) == HIGH;
}
```

PG goes LOW if the output falls out of regulation (overload, thermal shutdown,
input undervoltage). Firmware can use this to gracefully stop servo commands
and publish an MQTT alert before the ESP32 itself browns out.

If PG monitoring is not needed for v1.2, tie PG to NC and leave the GPIO
unassigned — the module operates identically with PG unconnected.

---

## Schematic Changes for v1.2

### Remove from schematic
- U4 (AMS1117-5.0)
- C_IN (10µF tantalum)
- C_OUT (22µF tantalum)

### Add to schematic
- U4 (symbol: 1×5 header, label "Pololu D24V22F5 socket")
  - Pin 1: PG → optional GPIO or NC
  - Pin 2: EN → +6V
  - Pin 3: VIN → +6V
  - Pin 4: GND → GND
  - Pin 5: VOUT → +5V
- C_IN (10µF X7R ceramic, VIN to GND)
- C_OUT (22µF X7R ceramic, VOUT to GND)
- C_OUT2 (100nF, VOUT to GND)

---

## v1.2 New Features (placeholders for GPIO assignment during layout)

| Feature | Interface | GPIOs needed | Notes |
|---------|-----------|--------------|-------|
| Second trigger input | Digital in | 1 | Same D1+R1 circuit as existing trigger |
| SD card reader | SPI | 4 (CS, MOSI, MISO, SCK) | Avoid GPIO 6–11 (flash) |
| Audio GAIN control | Digital out | 1 | MAX98357A GAIN pin via 2-resistor divider |
| SD_MODE control | Digital out | 1 | MAX98357A SD pin — enables/mutes amp |
| 5V Power Good | Digital in | 1 (optional) | Pololu PG pin → input-only GPIO |

**GPIO budget check (30-pin DOIT DevKit V1):**

Already used: GPIO 4, 16, 17, 21, 22, 25, 26, 27, 34
Available input-only: GPIO 35, 36, 39 (no internal pull-up)
Available general: GPIO 0(boot), 2, 5, 12(boot), 13, 14, 15, 18, 19, 23, 32, 33

v1.2 needs 7 new GPIOs minimum. Feasible — assign during schematic entry.

---

## BOM Additions for v1.2 Power Section

| Ref | Part | Value | Package | Source | Notes |
|-----|------|-------|---------|--------|-------|
| U4_SOCKET | 1×5 Female Header 2.54mm | — | Through-hole | LCSC C124418 | Socket for Pololu module |
| U4_MODULE | Pololu D24V22F5 | 5V 2.5A | — | Pololu #2858 | Hand-install — not JLCPCB |
| C_IN | Ceramic cap X7R | 10µF 10V | 0805 | LCSC C15850 | Near VIN of socket |
| C_OUT | Ceramic cap X7R | 22µF 10V | 1210 | LCSC C13585 | Near VOUT of socket |
| C_OUT2 | Ceramic cap X7R | 100nF 10V | 0402 | LCSC C14663 | Near VOUT of socket |

**Note:** The Pololu module itself is hand-installed after JLCPCB assembly,
same as the ESP32 dev board and MAX98357A breakout. Mark it DNP in the BOM
or add as a line item with no LCSC part number.

---

## References

- Pololu D24V22F5 product page: https://www.pololu.com/product/2858
- Pololu D24V22F5 dimensional drawing: https://www.pololu.com/product/2858/resources
- Pololu D24V25F5 (older version, same size): https://www.pololu.com/product/2850

---

*Generated: 2026-03-24*
*Project: Raven Animatronic Controller v1.2 PCB*
*Author: Bryan Haven*
