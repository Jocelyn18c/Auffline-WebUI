# TUSB320 — USB Type-C CC Controller Integration

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
8. [[#Register Reference]]
9. [[#USB MSD Gating]]
10. [[#Error Handling]]

---

## System Overview

The TUSB320 (IC6) is a USB Type-C Configuration Channel (CC) logic controller. Its sole function in this system is to present the correct Rd pull-down resistors (5.1kΩ) on CC1 and CC2 of the USB-C connector (J3), enabling a host computer to detect the device as a UFP (USB device) and assert VBUS. All USB data transfer is handled by the Teensy 4.1's native USB peripheral over D+/D−. The TUSB320 does not participate in data transfer.

```
CC Role Signaling:
Host Computer --> [USB-C Cable] --> CC1/CC2 (Rp on host)
                                --> TUSB320 Rd on CC1+CC2 --> Host detects UFP, asserts VBUS

Data Path (independent):
Host Computer --> [USB-C Cable] --> D+/D− --> TPD4S014 (U2) --> Teensy D+/D− pads
                                                             --> Teensy USB MSD stack

VBUS Path:
Host VBUS --> VUSB_RAW --> TPD4S014 (U2) --> VUSB_PROT --> BQ25895 VBUS input
                                                        --> TUSB320 VBUS_DET (via 900kΩ R6)

Control Path:
Teensy <--> [I2C0, Wire] <--> TUSB320 (addr 0x60)
```

> **NOTE:** I2C0 (`Wire`) is the shared power management bus — TUSB320, BQ25895, and BQ27441 all reside here. `Wire.begin()` and `Wire.setClock()` are called from the shared system init function — **not** from this driver. Do NOT call `Wire.begin()` in `tusb320_init()`. The AIC3204 codec is on `Wire1` (I2C1) — see [[AIC3204_Integration]].

---

## Hardware Constraints

### Port Mode
- PORT pin is pulled **LOW** → hardwired **UFP (Upstream Facing Port / Sink)** mode
- The device permanently presents Rd (5.1kΩ) on both CC pins
- Mode cannot be changed in software; I2C MODE_SELECT writes have no effect when PORT is hardwired
- **Do not issue I2C_SOFT_RESET** — it re-samples PORT LOW and resets status registers, providing no benefit and potentially causing a momentary detach event

### I2C Bus
- TUSB320 is on **I2C0** (`Wire`) — shared with BQ25895 and BQ27441
- **Address: 0x60** (ADDR pin tied to GND)
- SDA/SCL pulled up to SYS_3V3 on PCB
- `Wire.begin()` / `Wire.setClock(400000)` owned by shared system init function

### Enable
- EN_N pin tied to GND — device is **always enabled**; no software enable/disable

### Interrupt
- INT_N/OUT3 pulled up to SYS_3V3 via 200kΩ (R41) — **not connected to any Teensy GPIO**
- Interrupt-driven detection is not available
- Attach/detach status must be obtained by **polling REG09** from the main loop

### VBUS Detection
- VBUS_DET connected to VUSB_PROT through 900kΩ (R6) — required for UFP attach detection
- TUSB320 uses VBUS presence to confirm successful attachment in UFP mode
- VUSB_PROT is the post-protection VBUS net (output of TPD4S014 U2)

### ID Pin
- ID pin pulled up to SYS_3V3 via 200kΩ (R42) — open drain, asserts low on DFP attach
- Permanently high in UFP mode — informational only, not connected to Teensy GPIO

### VDD Supply
- VDD = SYS_3V3 (3.3V)
- 0.1µF decoupling capacitor C7 to GNDD
- SDA/SCL I2C pull-ups are also at 3.3V — VDD ≥ 3.0V required to prevent I2C back-powering. SYS_3V3 satisfies this.

### CC ESD Protection
- CC1 and CC2 pass through TPD1E10B06DYAR TVS diodes (D1, D2) before reaching the connector
- No firmware implication; hardware-only protection

---

## Pin Assignments

| Teensy Pin | Signal     | Direction | Notes                                      |
|------------|------------|-----------|--------------------------------------------|
| Wire SCL   | I2C0_SCL   | OUT       | Shared PM bus — TUSB320, BQ25895, BQ27441  |
| Wire SDA   | I2C0_SDA   | BIDIR     | Shared PM bus                              |

No additional Teensy GPIO required. INT_N and ID are not connected to Teensy.

---

## Power-On Behavior

```
T=0       VDD (SYS_3V3) ramps — must reach valid level within 25ms
T=0+      TUSB320 enters Unattached.SNK — Rd presented on CC1 and CC2
          I2C available after TI2C_EN (internal, ~ms after VDD valid)
T=init    tusb320_init() verifies device ID, reads and clears INTERRUPT_STATUS
T=init+   Normal polling begins — ATTACHED_STATE and CURRENT_MODE_DETECT monitored
```

> **NOTE:** IO pull-up power rail (SYS_3V3) must ramp with or after VDD. This is satisfied by the system power sequencing since VDD IS SYS_3V3.

---

## I2C Protocol

All registers are 8-bit. Address: `0x60`.

```
Write:
[START] [0x60 W] [reg_addr] [data] [STOP]

Read (set sub-address then read):
[START] [0x60 W] [reg_addr] [STOP]
[START] [0x60 R] [data] [STOP]

Multi-Read (sequential from sub-address):
[START] [0x60 W] [reg_addr] [STOP]
[START] [0x60 R] [d0] ACK [d1] ACK ... [dN] NACK [STOP]
```

> **NOTE:** If no sub-address write precedes a read, reads start at 0x00 and auto-increment. Always set sub-address explicitly for deterministic reads.

> **NOTE:** DEVICE_ID occupies registers 0x00–0x07 as ASCII bytes. Do not issue a bare multi-read from 0x00 expecting status registers — skip to 0x08 for status.

---

## Initialization Sequence

```
1. Verify device present: I2C read at 0x60 — NACK means device missing, abort
2. Read DEVICE_ID (0x00–0x07) — confirm ASCII string "TUSB320\0"
3. Read REG09 (0x09) — clear INTERRUPT_STATUS bit[4] by writing 1 to it
4. Read REG08 (0x08) — log initial CURRENT_MODE_DETECT and ATTACHED_STATE on entry
```

No register writes are required during init. PORT pin determines mode at power-on; MODE_SELECT in REG0A defaults to 0x00 (follow PORT pin) and must not be changed.

---

## Runtime Operation

### Polling Schedule

Poll REG09 every **500ms** from the main loop. This is sufficient latency for USB attach/detach events — the host debounce is ~100–150ms and the USB enumeration process takes hundreds of ms regardless.

If CURRENT_MODE_DETECT changes (i.e. a charger is connected advertising higher current), also poll REG08. Per the datasheet, CURRENT_MODE_DETECT only updates after an I2C_SOFT_RESET — **do not issue soft reset**. Instead, read REG08 once at attach time and treat the value as valid for that connection session. If the BQ25895 DPDM auto-detection is enabled (it is — see [[BQ25895_Integration]]), the BQ25895 independently handles charge current negotiation. TUSB320 current mode detection is therefore **informational only** in this system.

### Attach/Detach State (REG09[7:6] — ATTACHED_STATE)

```
00 — Not attached (Unattached.SNK)
01 — Attached.SRC detected (DFP connected) — normal USB host connection
10 — Attached.SNK (UFP) — should not occur; device is hardwired UFP
11 — Accessory attached
```

In normal operation, the device will toggle between `00` (no cable) and `01` (host connected). State `01` is the condition under which the Teensy USB MSD stack should be active.

### INTERRUPT_STATUS (REG09[4])

This bit sets whenever any CSR changes. Since INT_N is not wired to the Teensy, this bit must be cleared manually after each poll that detects a change:

```
Read REG09
If INTERRUPT_STATUS bit is set:
    Write REG09 with bit[4] = 1 to clear it
    (Write of 1 clears — see access type C)
```

Failure to clear INTERRUPT_STATUS will prevent INT_N from correctly asserting on future events. Although INT_N is unused here, the internal state must still be maintained correctly per the datasheet note.

### Cable Orientation (REG09[5] — CABLE_DIR)

```
0 — CC1 is active
1 — CC2 is active (default at power-on)
```

Informational only. USB 2.0 D+/D− are routed symmetrically through the TPD4S014; no mux switching is required. Log this value at attach time for diagnostics.

---

## Register Reference

### 0x00–0x07 — DEVICE_ID (R)
ASCII bytes of device identifier.  
Expected: `{0x00, 0x54, 0x55, 0x53, 0x42, 0x33, 0x32, 0x30}` = `"\0TUSB320"`  
Read from 0x07 down to 0x00 or use multi-read from 0x00.

### 0x08 — Status Register 1 (RU/RW)

| Bits | Name                   | Description                                                          |
|------|------------------------|----------------------------------------------------------------------|
| 7:6  | CURRENT_MODE_ADVERTISE | DFP advertise level (N/A in UFP mode — ignore)                       |
| 5:4  | CURRENT_MODE_DETECT    | 00=Default(500/900mA), 01=Medium(1.5A), 10=500mA accessory, 11=High(3A) |
| 3:1  | ACCESSORY_CONNECTED    | 000=none, 100=audio, 101=audio charge-through, 110=debug             |
| 0    | ACTIVE_CABLE_DETECTION | 1 = active cable detected                                            |

### 0x09 — Status Register 2 (RU/RC)

| Bits | Name             | Description                                                               |
|------|------------------|---------------------------------------------------------------------------|
| 7:6  | ATTACHED_STATE   | 00=not attached, 01=Attached.SRC (DFP), 10=Attached.SNK, 11=accessory    |
| 5    | CABLE_DIR        | 0=CC1 active, 1=CC2 active                                                |
| 4    | INTERRUPT_STATUS | 1=CSR changed (INT_N asserted). Write 1 to clear.                         |
| 2:1  | DRP_DUTY_CYCLE   | N/A in UFP mode                                                           |

### 0x0A — Control Register (RW)

| Bits | Name            | Description                                                              |
|------|-----------------|--------------------------------------------------------------------------|
| 7:6  | DEBOUNCE        | CC debounce time: 00=133ms, 01=116ms, 10=151ms, 11=168ms (default 00)   |
| 5:4  | MODE_SELECT     | 00=follow PORT pin (default — do not change)                             |
| 3    | I2C_SOFT_RESET  | Write 1 to reset digital logic. Self-clearing. **Do not use.**           |

### 0x45 — Misc Control (RW)

| Bits | Name          | Description                                         |
|------|---------------|-----------------------------------------------------|
| 2    | DISABLE_RD_RP | 1 = disable Rd/Rp. **Do not set** — disables CC signaling entirely. |

---

## USB MSD Gating

The TUSB320 `tusb320_is_attached()` function is the gate condition for USB Mass Storage Device activity. The Teensy USB stack should only be considered active when ATTACHED_STATE == 0b01.

Future MSD / file transfer code **must** check `tusb320_is_attached()` before initiating any USB transfers. This prevents the Teensy from attempting USB enumeration when no host is connected.

Design contract:
```
tusb320_is_attached()  →  true   : USB-C host connected, VBUS present, D+/D- active
                       →  false  : no cable, or accessory (not a host), or VBUS absent
```

The MSD subsystem (to be written) will use this as its enable gate. No other subsystem depends on TUSB320 state.

> **NOTE:** Cable orientation (CABLE_DIR) does not affect MSD functionality. D+/D− are wired symmetrically. Do not gate any functionality on CABLE_DIR.

---

## Error Handling

| Condition                        | Detection                              | Action                                                      |
|----------------------------------|----------------------------------------|-------------------------------------------------------------|
| Device not found                 | I2C NACK on 0x60                       | Log error, retry init ×3, flag system fault                 |
| Wrong device ID                  | 0x00–0x07 ≠ "TUSB320\0"               | Abort init — wrong device or bus corruption                 |
| Unexpected ATTACHED_STATE        | State == 0b10 or 0b11                  | Log — UFP/accessory states should not occur; continue polling |
| INTERRUPT_STATUS stuck high      | Bit[4] set after repeated clear writes | Log — possible I2C issue; re-init driver                   |
| Poll I2C error (Wire timeout)    | Wire.endTransmission() ≠ 0            | Log, mark last known state stale, retry next poll cycle     |
