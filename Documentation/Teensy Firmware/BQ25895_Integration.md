# BQ25895 — LiPo Charger Integration

**Rev:** 1.0  
**Interface:** I2C0 (`Wire`)  
**Status:** In Progress

---

## Table of Contents

1. [[#System Overview]]
2. [[#Hardware Constraints]]
3. [[#Pin Assignments]]
4. [[#Power-On Behavior]]
5. [[#I2C Protocol]]
6. [[#Initialization Sequence]]
7. [[#Runtime Operation]]
8. [[#ADC Readings]]
9. [[#Fault Handling]]
10. [[#Error Handling]]

---

## System Overview

The BQ25895 is a single-cell LiPo charger and NVDC power path manager. It accepts USB or adapter input on VBUS (3.9V–14V), charges the LiPo via BAT, and maintains a regulated SYS output for the system load. The Teensy 4.1 is the I2C host — all charger parameters are configured via I2C. Status and faults are obtained by polling REG0B and REG0C periodically.

```
Power Path:
USB/Adapter --> VBUS --> [BQ25895 Buck Converter] --> SYS (system supply)
                                                 |--> BAT (LiPo charge)

Control Path:
Teensy <--> [I2C0, Wire] <--> BQ25895 (addr 0x6A)

ADC Path:
BQ25895 internal ADC --> REG0D–REG12 --> Teensy reads: VBAT, VSYS, VBUS, ICHG, TS
```

> **NOTE:** I2C0 (`Wire`) is the shared power management bus — BQ25895, BQ27441, and TUSB320 all reside here. `Wire.begin()` and `Wire.setClock()` are called from the shared system init function — **not** from this driver. Do NOT call `Wire.begin()` in `bq25895_init()`. Do NOT use `Wire` for the AIC3204 codec; that device is on `Wire1` (I2C1). See [[AIC3204_Integration]] and [[BM83_Integration]].

---

## Hardware Constraints

### Power Path (NVDC)
- SYS is powered by VBUS first — battery supplements only when input is insufficient
- System operates with no battery present (NVDC instant-on)
- VSYSMIN default: 3.5V (REG03[3:1] = 101) — SYS held at 3.5V minimum when battery is low

### Fuse Limit
- System fuses are rated at **1.8A max**
- Charge current is set to **1472mA (0x17)** — N=23 × 64mA — to remain safely under the fuse limit
- NVDC power path means charge current and system load share the same VBUS input. Under full system load (BM83 + AIC3204 active), effective available charge current will be lower than 1472mA — the BQ25895 handles this automatically via power path management
- **Do not increase ICHG above N=23 (1472mA) without revisiting the fuse ratings**

### Battery
- Single protected LiPo pack: 2× 2200mAh cells in parallel = **4400mAh total**
- Charge voltage: **4.208V** (standard LiPo)
- Charge current: **1472mA ≈ 0.33C** — conservative, appropriate for longevity
- Termination current: **256mA**

### I2C Bus
- BQ25895 is on **I2C0** (`Wire`) — shared with BQ27441 and TUSB320
- **Address: 0x6A** (fixed, not configurable)
- Supports 100 kHz (standard) and 400 kHz (fast) mode
- SDA/SCL pull-ups to SYS_3V3 are on PCB
- `Wire.begin()` / `Wire.setClock(400000)` are owned by the shared system init function

### Watchdog Timer
- Disabled in init (REG07[5:4] = 00)
- The Teensy is always present as host — periodic kicks are unnecessary
- Hardware safety nets remain active regardless: 12h safety timer, thermal shutdown, battery OVP, NTC protection
- If watchdog is ever re-enabled, WD_RST (REG03[6]) must be kicked before the configured period expires or all registers (except IINLIM, VINDPM, VINDPM_OS, BATFET_RST_EN, BATFET_DLY, BATFET_DIS) reset to defaults

### STAT and INT Pins
- **STAT pin:** Disabled in software via STAT_DIS (REG07[6] = 1) — not connected on PCB
- **INT pin:** Not connected on PCB — no Teensy interrupt handler
- Charge status and faults are obtained by **polling REG0B and REG0C** from the main loop

### OTG / Boost Mode
- OTG disabled (REG03[5] = 0) — device is USB peripheral, not USB host

---

## Pin Assignments

| Teensy Pin | Signal | Direction | Notes |
|---|---|---|---|
| Wire SCL | I2C0_SCL | OUT | Shared PM bus — BQ25895, BQ27441, TUSB320 |
| Wire SDA | I2C0_SDA | BIDIR | Shared PM bus |

No additional Teensy GPIO required. STAT and INT are not connected.

---

## Power-On Behavior

After POR, the device enters **default mode** with watchdog expired. All registers are at default values. Autonomous charging begins immediately at defaults (4.208V, 2.048A, 12h safety timer).

Any I2C write transitions to **host mode** and starts the watchdog. The watchdog is disabled in the first init write, preventing register reset during the remainder of init.

```
T=0       VBUS applied / POR
T=0+      Device enters Default Mode — autonomous charging begins at defaults
          WARNING: default ICHG = 2.048A — exceeds fuse limit until init completes
T=init    First I2C write (REG07) → host mode, watchdog disabled immediately
T=init+   Remainder of init sequence sets correct parameters
```

> **WARNING:** There is a brief window between POR and init completion where the charger runs at the default 2.048A charge current. This exceeds the 1.8A fuse rating. Ensure init runs as early as possible in `setup()`, before any other initialization that could delay it.

---

## I2C Protocol

All registers are 8-bit. Address: `0x6A`.

```
Write:
[START] [0x6A W] [reg_addr] [data] [STOP]

Read:
[START] [0x6A W] [reg_addr] [RESTART] [0x6A R] [data] [STOP]

Multi-Read (REG00–REG14, except REG0C):
[START] [0x6A W] [reg_addr] [RESTART] [0x6A R] [d0] ACK [d1] ACK ... [dN] NCK [STOP]
```

> **NOTE:** REG0C (fault register) does **not** support multi-read. Always read it separately. Read it **twice consecutively** — first read returns latched (pre-existing) faults, second read returns live state.

---

## Initialization Sequence

REG07 is written first to disable the watchdog before any other register is touched.

| Register | Value | Description |
|---|---|---|
| REG07 | 0xCC | TERM_EN=1, STAT_DIS=1, WATCHDOG=00 (disabled), EN_TIMER=1, CHG_TIMER=12h |
| REG00 | 0x08 | EN_HIZ=0, EN_ILIM=0, IINLIM=500mA — pre-detection floor, auto-updated by DPDM |
| REG02 | 0x3D | CONV_RATE=0 (one-shot ADC), ICO_EN=1, HVDCP_EN=1, MAXC_EN=1, AUTO_DPDM_EN=1 |
| REG03 | 0x1A | OTG=0, CHG_CONFIG=1 (charge enabled), SYS_MIN=3.5V |
| REG04 | 0x17 | ICHG = N=23 × 64mA = **1472mA** — do not increase without checking fuse ratings |
| REG05 | 0x13 | IPRECHG=128mA, ITERM=256mA |
| REG06 | 0x5E | VREG=4.208V, BATLOWV=3.0V, VRECHG=100mV below VREG |
| REG08 | 0x03 | BAT_COMP=0Ω (IR comp off), VCLAMP=0mV, TREG=120°C |

After init: read REG0C twice to flush any latched POR faults before beginning normal polling.

> **NOTE:** Read REG14[5:3] during bring-up to verify device identity. PN[2:0] = 0b111 confirms BQ25895. Abort init if mismatch.

---

## Runtime Operation

### Polling Schedule
Poll the following registers periodically from the main loop (suggested interval: 1–5s):

- **REG0B** — charge status, VBUS source type, power good flag
- **REG0C** — faults (read twice each poll cycle)
- **REG0D / REG0F / REG11 / REG12** — ADC values (trigger one-shot first via REG02[7])

### Charge Status (REG0B)

```
CHRG_STAT[1:0] (bits 4:3):
  00 — Not charging
  01 — Pre-charge (VBAT < 3.0V)
  10 — Fast charging (CC or CV)
  11 — Charge termination done

VBUS_STAT[2:0] (bits 7:5):
  000 — No input
  001 — USB SDP (500mA)
  010 — USB CDP (1.5A)
  011 — USB DCP (3.25A)
  100 — MaxCharge DCP (1.5A)
  101 — Unknown adapter (500mA)
  110 — Non-standard adapter
  111 — OTG

PG_STAT (bit 2): 1 = power good (valid VBUS present)
VSYS_STAT (bit 0): 1 = in VSYSMIN regulation (battery critically low)
```

---

## ADC Readings

Trigger one-shot conversion by setting CONV_START (REG02[7] = 1). Poll until it self-clears (~1ms). Then read:

| Register | Signal | Formula |
|---|---|---|
| REG0D | VBAT | 2304mV + (raw[6:0] × 20mV) |
| REG0F | VSYS | 2304mV + (raw[6:0] × 20mV) |
| REG11 | VBUS | 2600mV + (raw[6:0] × 100mV), bit[7] = VBUS_GD |
| REG12 | ICHG | raw[6:0] × 50mA (returns 0 when VBAT < VBATSHORT) |
| REG10 | TS%  | 21% + (raw[6:0] × 0.465%) |

Alternatively, set CONV_RATE=1 (REG02[6]) for continuous 1s conversions — no manual trigger needed, but higher quiescent current.

---

## Fault Handling

### REG0C — Fault Register

```
Bit 7     — WATCHDOG_FAULT: watchdog expired (registers may have reset)
Bit 6     — BOOST_FAULT:    OTG overload or OVP (OTG disabled — should never set)
Bits 5:4  — CHRG_FAULT:     00=normal, 01=input fault, 10=thermal shutdown, 11=safety timer
Bit 3     — BAT_FAULT:      battery OVP (VBAT > 104% of VREG)
Bits 2:0  — NTC_FAULT:      000=normal, 001=TS cold, 010=TS hot (buck mode)
```

> **WARNING:** Read REG0C **twice consecutively** every poll cycle. First read returns latched state. Second read returns live state. NTC_FAULT is the only field that always reflects live TS pin condition on first read.

---

## Error Handling

| Condition | Detection | Action |
|---|---|---|
| Device not found | I2C NACK on 0x6A | Log error, retry init ×3, flag system fault |
| Wrong device ID | REG14[5:3] ≠ 0b111 | Abort init — wrong device or bus corruption |
| Watchdog expired | REG0C[7] set | Re-run full init to restore charge parameters |
| Battery OVP | REG0C[3] set | Log — do not retry charge, cell may be damaged |
| Thermal shutdown | CHRG_FAULT = 10 | Log — charger resumes autonomously after cooling |
| Safety timer expired | CHRG_FAULT = 11 | Log — 12h elapsed without termination, possible cell or NTC fault |
| NTC cold/hot | NTC_FAULT ≠ 000 | Log — charger suspends autonomously, resumes when TS returns to normal range |
| No power good | REG0B[2] = 0 | VBUS not present — system running on battery only |
