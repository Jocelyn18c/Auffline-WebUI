# TLV320AIC3204IRHBR — Audio Codec Integration

**Rev:** 1.0  
**Interface:** I2S2 (slave) + I2C1  
**Status:** Untested

---

## Table of Contents

1. [[#System Overview]]
2. [[#Hardware Constraints]]
3. [[#Pin Assignments]]
4. [[#Reset & Power-On Sequence]]
5. [[#I2C Register Protocol]]
6. [[#Initialization Register Sequence]]
7. [[#I2S Audio]]
8. [[#Signal Routing]]
9. [[#MEMS Microphone (ICS-40720)]]
10. [[#Error Handling]]

---

## System Overview

The TLV320AIC3204 (IC7) is a stereo audio codec operating as both DAC and ADC simultaneously at 48kHz. It receives PCM audio from the Teensy 4.1 over I2S2 and outputs to headphone (J4) and line out (J5). It captures from the ICS-40720 MEMS microphone and returns ADC data to the Teensy over the same I2S2 bus.

```
DAC Path:
Teensy I2S2 (master TX) --> AIC3204 DAC --> HPL/HPR --> J4 (Headphone, 47nF AC-coupled)
                                          --> LOL/LOR --> J5 (Line Out, 1µF AC-coupled)

ADC Path:
ICS-40720 MEMS mic --> MIC+/MIC-/MICBIAS --> AIC3204 ADC --> Teensy I2S2 (master RX)

Control Path:
Teensy <--> [I2C1, Wire1] <--> AIC3204 (addr 0x18)
```

> **NOTE:** This is a completely separate subsystem from the BM83/I2S1 path. I2S1 and I2S2 are independent Teensy peripherals. The two audio paths never intersect. See [[BM83_Integration]] for Bluetooth documentation.

---

## Hardware Constraints

### I2S Role
- AIC3204 is **I2S slave** — Teensy drives MCLK2, BCLK2, and LRCLK2
- Teensy must use `AudioOutputI2S2` and `AudioInputI2S2` (master variants)
- MCLK2 is required by the AIC3204 PLL and is driven by the Teensy

> **WARNING:** Do NOT use `AudioOutputI2Sslave` for the AIC3204 path. That variant is used on I2S1 for the BM83. Using the wrong I2S object will either silence output or conflict with BM83 clocking.

### I2C Bus
- AIC3204 is on **I2C1** (`Wire1` in Arduino/Teensy) — SCL1/SDA1
- I2C0 (`Wire`) is used by power management ICs (BQ25895, BQ27441, TUSB320) — do not use Wire for codec
- **Address: 0x18** (SPI_SELECT pin 12 tied to GNDD selects I2C mode; address pin not pulled high)

### Reset Circuit
- CODEC_RESET signal drives 2N7002 Q3 with 10kΩ pull-up to SYS_3V3
- Same inversion scheme as BM83 reset:
  - `CODEC_RESET HIGH` → MOSFET on → RST_N LOW (codec in reset)
  - `CODEC_RESET LOW` → MOSFET off → RST_N floats HIGH via pull-up (running)
- CODEC_RESET is driven from a Teensy GPIO — see Pin Assignments

### Power Supply
- AVDD: SYS_3V3 with 10µF + 0.1µF decoupling to GND1 (analog ground)
- IOVDD: SYS_3V3
- AVSS: GND1
- GND1 (analog ground) is single-point tied to GNDD at the codec — do not add additional GND1-to-GNDD connections in firmware

### SPI / MFP Pins
- SPI interface is not used. SCL/SS, SDA/MOSI, MISO/MFP4, HP_SELECT pins are marked NC or configured as inputs in software
- Comment in schematic: *"DISABLE NC OR SET AS INPUTS IN SOFTWARE"* — handled in init sequence

---

## Pin Assignments

| Teensy Pin | Signal | Direction | Notes |
|---|---|---|---|
| I2S2 pins | I2S_OUT2 / BCLK2 / LRCLK2 / MCLK2 | OUT | Master — Teensy drives all clocks |
| I2S2 RX | I2S_IN2 | IN | ADC data from codec |
| Wire1 SCL | I2C1_SCL | OUT | I2C1 clock to AIC3204 |
| Wire1 SDA | I2C1_SDA | BIDIR | I2C1 data to/from AIC3204 |
| 31 | CODEC_RESET | OUT | HIGH = RST_N LOW via 2N7002 Q3 (codec in reset) |

---

## Reset & Power-On Sequence

```
T=0ms    Assert CODEC_RESET HIGH     RST_N LOW — codec in reset
T=10ms   Assert CODEC_RESET LOW      RST_N floats HIGH — codec released
T=10ms+  Wait 1ms                    Internal reset settling (datasheet: >1ms)
T=11ms+  Begin I2C register writes   Page 0 register sequence
```

> **NOTE:** Unlike the BM83, the AIC3204 has no boot-complete event. After the reset hold time, it is ready for I2C writes immediately. Do not interleave BM83 and AIC3204 init — complete BM83 init first, then initialize the codec.

---

## I2C Register Protocol

The AIC3204 uses a paged register map. Before writing any register, the correct page must be selected by writing to **Register 0 (Page Select)** on the current page.

```
Page Select:  Write reg 0x00, value = page number (0 or 1)
Register Write: [START] [0x18 W] [reg_addr] [value] [STOP]
Register Read:  [START] [0x18 W] [reg_addr] [RESTART] [0x18 R] [value] [STOP]
```

All init writes below are on **Page 0** unless explicitly noted.

---

## Initialization Register Sequence

Configured for:
- 48kHz, 16-bit, stereo
- MCLK2 input → PLL → internal clocks
- ADC: IN1_L differential (MIC+/MIC-) with MICBIAS
- DAC: Left → HPL + LOL, Right → HPR + LOR
- SPI/MFP pins disabled (set to input)

```cpp
// Page 0
{ 0x00, 0x00 },  // Select Page 0

// Software reset
{ 0x01, 0x01 },  // P0 R1: Software reset (self-clearing)
// Wait 1ms after reset

// Clock configuration — MCLK2 = 12.288 MHz (256×48kHz, driven by Teensy AudioOutputI2S2)
// NDAC=1, MDAC=2, DOSR=128 → DAC_FS = 12.288M / 1 / 2 / 128 = 48kHz ✓
{ 0x04, 0x00 },  // P0 R4:  PLL clock source = MCLK, PLL disabled
{ 0x05, 0x11 },  // P0 R5:  P=1, R=1 (PLL not active)
{ 0x0B, 0x81 },  // P0 R11: NDAC=1, powered
{ 0x0C, 0x82 },  // P0 R12: MDAC=2, powered
{ 0x0D, 0x00 },  // P0 R13: DOSR MSB
{ 0x0E, 0x80 },  // P0 R14: DOSR=128
{ 0x12, 0x81 },  // P0 R18: NADC=1, powered
{ 0x13, 0x82 },  // P0 R19: MADC=2, powered
{ 0x14, 0x80 },  // P0 R20: AOSR=128

// Interface control
{ 0x1B, 0x00 },  // P0 R27: I2S, 16-bit, slave mode, no offset
{ 0x1C, 0x00 },  // P0 R28: Data offset = 0

// DAC routing and power
{ 0x3F, 0xD6 },  // P0 R63: DAC powered, Left=I2S left ch, Right=I2S right ch
{ 0x40, 0x00 },  // P0 R64: DAC mute off, volume = 0dB
{ 0x41, 0x00 },  // P0 R65: Left DAC digital volume = 0dB
{ 0x42, 0x00 },  // P0 R66: Right DAC digital volume = 0dB

// Page 1 — analog routing
{ 0x00, 0x01 },  // Select Page 1

{ 0x01, 0x08 },  // P1 R1: AVDD disabled (powered from external SYS_3V3)
{ 0x02, 0x01 },  // P1 R2: AVDD LDO powerdown (using external supply)
{ 0x09, 0x3C },  // P1 R9:  HPL/HPR powered
{ 0x0A, 0x0F },  // P1 R10: LOL/LOR powered
{ 0x0C, 0x08 },  // P1 R12: HPL ← DAC_L, not muted
{ 0x0D, 0x08 },  // P1 R13: HPR ← DAC_R, not muted
{ 0x0E, 0x08 },  // P1 R14: LOL ← DAC_L, not muted
{ 0x0F, 0x08 },  // P1 R15: LOR ← DAC_R, not muted
{ 0x10, 0x00 },  // P1 R16: HPL gain = 0dB
{ 0x11, 0x00 },  // P1 R17: HPR gain = 0dB
{ 0x12, 0x00 },  // P1 R18: LOL gain = 0dB
{ 0x13, 0x00 },  // P1 R19: LOR gain = 0dB

// ADC input routing — IN1_L differential (MIC+/MIC- on IN3_L/IN3_R per schematic)
// Schematic shows MIC+/MIC- connected to IN3_L and IN3_R (pins 20/21)
{ 0x34, 0x30 },  // P1 R52: Left MICPGA+ = IN3_L (10kΩ)
{ 0x36, 0x30 },  // P1 R54: Left MICPGA- = IN3_R/CM (10kΩ) — differential
{ 0x37, 0x04 },  // P1 R55: Left MICPGA gain = 6dB (adjust for ICS-40720 sensitivity)
{ 0x38, 0x30 },  // P1 R56: Right MICPGA+ = IN3_R (10kΩ)
{ 0x39, 0x30 },  // P1 R57: Right MICPGA- = IN3_L/CM (10kΩ)
{ 0x3A, 0x04 },  // P1 R58: Right MICPGA gain = 6dB

// MICBIAS = AVDD (2.7V-ish on 3.3V supply — acceptable for ICS-40720 VDD=1.8–3.6V)
{ 0x33, 0x40 },  // P1 R51: MICBIAS = AVDD

// Page 0 — ADC power on
{ 0x00, 0x00 },  // Select Page 0
{ 0x51, 0xC0 },  // P0 R81: ADC powered (left + right), unmuted
{ 0x52, 0x00 },  // P0 R82: ADC fine gain = 0dB
```

> **NOTE:** MIC input routing uses IN3_L/IN3_R as shown in the schematic. Cross-reference AIC3204 datasheet Table 5-1 if ADC is silent after init.

> **NOTE:** All gain values are set to 0dB / minimum as starting points. Tune HPL/HPR output level, MICPGA gain, and DAC digital volume empirically during audio bring-up.

---

## I2S Audio

### Teensy Configuration

```cpp
#include <Audio.h>
#include <Wire.h>

AudioOutputI2S2   i2s2_out;   // I2S2 — Teensy is master, AIC3204 is slave
AudioInputI2S2    i2s2_in;    // I2S2 — simultaneous ADC capture
// Connect audio sources/sinks via AudioConnection
```

### Parameters

| Parameter | Value |
|---|---|
| Sample rate | 48 kHz |
| Bit depth | 16-bit |
| Channels | Stereo |
| AIC3204 role | I2S slave |
| Teensy role | I2S master (drives MCLK2, BCLK2, LRCLK2) |
| MCLK2 | Required — driven by Teensy (256×Fs = 12.288 MHz) |

---

## Signal Routing

### Outputs

| Output | Path | Connector | AC Coupling |
|---|---|---|---|
| HPL / HPR | DAC → headphone amp → J4 | SJ-3523-SMT-TR (3.5mm TRS) | 47nF |
| LOL / LOR | DAC → line driver → J5 | SJ-3523-SMT-TR (3.5mm TRS) | 1µF |

> **NOTE:** The 47nF headphone coupling caps have a low-frequency rolloff around 70Hz at typical headphone impedances (32–300Ω). This is intentional for headphone use but means the headphone output is not suitable for subwoofer or full-range monitoring below ~70Hz. Line out uses 1µF which rolls off below ~8Hz at 20kΩ load — effectively full range.

### Inputs

| Input | Source | Connection | Notes |
|---|---|---|---|
| MIC+ / MIC- | ICS-40720 OUTPUT+ / OUTPUT- | IN3_L / IN3_R | Differential |
| MICBIAS | AIC3204 → ICS-40720 VDD | Pin 19 | AVDD referenced |

---

## MEMS Microphone (ICS-40720)

The ICS-40720 (IC8) is an analog differential output MEMS mic. It is powered from MICBIAS and its differential output feeds directly into the AIC3204 MICPGA.

| Parameter | Value |
|---|---|
| Interface | Analog differential |
| VDD | MICBIAS (AVDD ≈ 3.3V — within ICS-40720 spec of 1.8–3.6V) |
| Output | OUTPUT+ → MIC+ (R47, 100Ω) → IN3_L, OUTPUT- → MIC- (R46, 100Ω) → IN3_R |
| Series resistors | 100Ω (R47, R46) — EMI filtering |
| Sensitivity | −38 dBV/Pa typical |
| Suggested MICPGA gain | 6dB starting point — increase if ADC is underdriven |

> **NOTE:** The ICS-40720 is a bottom-port mic. Verify physical orientation on the PCB matches the intended acoustic port direction before enclosure assembly.

---

## Error Handling

| Condition | Detection | Recovery |
|---|---|---|
| I2C device not found | Wire1.endTransmission() != 0 on addr 0x18 | Log error, halt codec init, report via Serial |
| Register write failure | I2C NACK | Retry once, then log and flag |
| No audio output after init | Scope MCLK2 / BCLK2 — check for clock activity | Verify `AudioOutputI2S2` object is declared and connected |
| ADC always zero | Confirm MICBIAS present, check MICPGA gain register | Increase P1 R55/R58 gain; verify IN3 routing |
| DAC output noise / hum | Check GND1 single-point tie integrity | Ensure no secondary GND1-GNDD connection paths |
| Wrong I2C address | All writes silently fail | Run I2C scan in setup(), log all found addresses |
