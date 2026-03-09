# BM83SM1-00TA — Bluetooth A2DP Source Integration

**Rev:** 1.1  
**Profile:** A2DP Source  
**UART Ref:** AudioUARTCommandSet_v2_02  
**Status:** Untested

---

## Table of Contents

1. [[#System Overview]]
2. [[#Hardware Constraints]]
3. [[#Pin Assignments]]
4. [[#Boot & Initialization Sequence]]
5. [[#UART Protocol]]
6. [[#I2S Audio]]
7. [[#Device Discovery & Connection]]
8. [[#AVRCP Playback State]]
9. [[#Device Persistence (SD Card)]]
10. [[#Error Handling]]

---

## System Overview

The BM83SM1-00TA acts as a Bluetooth A2DP audio source. It receives raw PCM audio from the Teensy 4.1 over I2S1, encodes it to SBC at 48kHz, and streams to paired Bluetooth speakers or headphones. The Teensy is the system host — all BM83 behavior is controlled via UART.

```
Audio Path:
Teensy I2S1 (slave TX) --> BM83 --> A2DP/SBC --> BT Speaker / Headphones

Control Path:
Teensy <--> [UART 115200 8N1] <--> BM83

AVRCP Path (polled, not pushed):
BT Device --> [AVRCP] --> BM83 internal state --> Teensy polls Read_Link_Status
```

> **NOTE:** The AIC3204 codec (I2S2) is a fully separate audio path. The two paths never intersect. See [[AIC3204_Integration]] for codec documentation.

---

## Hardware Constraints

### Operating Mode
- BM83SM1-**00TA** SKU ships pre-programmed with AT firmware
- Defaults to **Host/MCU mode** — no firmware flashing or IS2083 Config Tool required
- UART command control active by default

### I2S
- BM83 is **I2S master** — drives BCLK and LRCLK
- Teensy must be configured as **I2S slave** (`AudioOutputI2Sslave`)
- `MCLK1` is routed on PCB but does not need to be driven — BM83 self-clocked in master mode

> **WARNING:** Do not use `AudioOutputI2S`. Use `AudioOutputI2Sslave` only. Clock conflict will silently corrupt audio.

### UART
- **115200 baud, 8N1, no flow control**
- RTS/CTS pins unconnected on PCB — correct, AT firmware has flow control disabled

### Reset Circuit
- RST_N (pin 43) driven via 2N7002 MOSFET with 10kΩ pull-up to SYS_3V3
- Signal polarity is **inverted**: `BT_RESET HIGH` → MOSFET on → RST_N LOW (in reset)
- `BT_RESET LOW` → MOSFET off → RST_N HIGH via pull-up (running)

### BAT_IN Power Supply
- BAT_IN (BM83 pin 24) tied to SYS_3V3 with 10µF + 0.1µF decoupling to ground
- No battery or charging circuit present — BM83 runs from regulated 3.3V only

> **WARNING:** Do not query battery status or act on battery state UART events. Values are meaningless in this supply configuration.

---

## Pin Assignments

| Teensy Pin | Signal                     | Direction | Notes                                  |
| ---------- | -------------------------- | --------- | -------------------------------------- |
| 36         | PWR(MFB)                   | OUT       | HIGH to power on BM83                  |
| 32         | BT_RESET                   | OUT       | HIGH = RST_N LOW (in reset) via 2N7002 |
| Serial8 TX | BM83RX                     | OUT       | UART commands to BM83                  |
| Serial8 RX | BM83TX                     | IN        | UART events from BM83                  |
| I2S1 pins  | I2S_OUT1A / BCLK1 / LRCLK1 | OUT/IN    | Slave — BM83 drives clocks             |

---

## Boot & Initialization Sequence

```
T=0ms    Assert BT_RESET HIGH     RST_N LOW — chip in reset
T=0ms    Assert MFB LOW
T=10ms   Assert MFB HIGH          MFB must go HIGH before RST_N is released
T=30ms   Release BT_RESET LOW     RST_N floats HIGH — chip released from reset
T=30ms+  Poll UART (discard)      Events may arrive during boot window
T=530ms  Wait for 0x30 event      EVT_BTM_INIT_COMPLETE, status byte 0x00
T=530ms+ Send MMI_Action 0xE7     Set A2DP 48kHz PCM encoder mode
```

> **WARNING:** Do not send any UART commands before receiving `EVT_BTM_INIT_COMPLETE (0x30)` with status `0x00`. Commands sent before this event are silently discarded.

> **WARNING:** MFB must be asserted HIGH before RST_N is released. Simultaneous assertion is incorrect and causes unreliable boot behavior.

---

## UART Protocol

### Frame Format

```
| 0xAA (start) | LEN_MSB | LEN_LSB | CMD/EVENT_ID | PARAMS... | CHECKSUM |
```

- `LEN` = number of bytes from CMD_ID through end of PARAMS (excludes start byte, length bytes, and checksum)
- `CHECKSUM` = `0x100 - ((LEN_MSB + LEN_LSB + CMD_ID + all PARAMS) & 0xFF)`

### Commands (Teensy → BM83)

| Command | ID | Parameters |
|---|---|---|
| MMI_Action | `0x02` | `db_index, action` |
| Music_Control | `0x04` | `0x00, action` |
| Read_Link_Status | `0x0D` | `0x00` |
| Read_Paired_Device_Record | `0x0E` | `0x00` |
| Profile_Link_Back | `0x17` | `type, [device_index, profile, bdaddr]` |
| Disconnect | `0x18` | `db_index, flag` |
| Set_Overall_Gain | `0x23` | `db_index, type, gain` |

### MMI_Action Sub-Commands (action byte)

| Action | Byte | Description |
|---|---|---|
| Enter pairing mode | `0x51` | BM83 enters discoverable mode |
| A2DP 48kHz PCM encoder | `0xE7` | **Sent at init** — sets A2DP source at 48kHz |
| Exit PCM encoder | `0xE8` | Shutdown A2DP source stream |

### Music_Control Action Bytes

| Action | Byte |
|---|---|
| Play | `0x05` |
| Pause | `0x06` |
| Stop | `0x08` |
| Next | `0x09` |
| Previous | `0x0A` |

### Events (BM83 → Teensy)

| Event | ID | Description |
|---|---|---|
| BTM_Status | `0x01` | All link and state changes |
| Read_Link_Status_Reply | `0x1E` | Poll response — contains play status |
| Read_Paired_Device_Record_Reply | `0x1F` | Paired device BD_ADDR + priority |
| Report_BTM_Initial_Status | `0x30` | Boot complete |

### BTM_Status State Bytes (payload[0])

| State | Byte | Action |
|---|---|---|
| Power ON | `0x02` | Nominal |
| Pairing / discoverable | `0x01` | Scan active |
| Pairing successful | `0x03` | New device paired |
| Pairing failed | `0x04` | Retry |
| A2DP link established | `0x06` | Trigger SD save, begin stream |
| A2DP link disconnected | `0x08` | Cleanup, reconnect |
| AVRCP link established | `0x0B` | Streaming fully active |
| AVRCP link disconnected | `0x0C` | AVRCP commands suspended |
| Standby | `0x0F` | Idle |

---

## I2S Audio

### Teensy Configuration

```cpp
#include <Audio.h>

AudioOutputI2Sslave   i2s1_out;   // I2S1 — BM83 is master, Teensy is slave
// Connect audio source via AudioConnection
```

### Parameters

| Parameter | Value |
|---|---|
| Sample rate | 48 kHz |
| Bit depth | 16-bit |
| Channels | Stereo |
| BM83 role | I2S master (drives BCLK, LRCLK) |
| Teensy role | I2S slave (data only) |
| MCLK | Not required from Teensy |

---

## Device Discovery & Connection

### Pairing New Device

```
1. Send MMI_Action (0x02), action=0x51 → BM83 enters discoverable mode
2. BT device initiates connection to BM83
3. Receive BTM_Status state=0x03 (pairing successful)
4. Receive BTM_Status state=0x06 (A2DP established) → trigger SD save
5. Receive BTM_Status state=0x0B (AVRCP established) → streaming active
```

### Connect to Specific Device (by BD_ADDR)

```
1. Send Profile_Link_Back (0x17)
     params: type=0x05, device_index=0x00, profile=0x02 (A2DP), bdaddr[6]
2. Wait for BTM_Status state=0x06 then 0x0B
```

### Connect to Last Paired Device

```
1. Send Profile_Link_Back (0x17), params: type=0x02
   OR allow BM83 AT firmware auto-reconnect on boot (default behavior — confirm)
2. Wait for BTM_Status state=0x06 then 0x0B
```

---

## AVRCP Playback State

AVRCP passthrough commands from the BT device **are not pushed to the Teensy as unsolicited events**. The BM83 handles them internally. The Teensy must poll.

### Polling Method

```
1. Every 300ms (when BM83_STATE_STREAMING), send Read_Link_Status (0x0D), param=0x00
2. Receive Read_Link_Status_Reply (0x1E)
     payload[0] = Device_State
     payload[1] = DB0_Connect_Status (bitmask: bit0=A2DP sig, bit1=A2DP stream, bit2=AVRCP)
     payload[2] = DB1_Connect_Status
     payload[3] = DB0_Play_Status  ← compare against last known state
     payload[4] = DB1_Play_Status
     payload[5] = DB0_Stream_Status
     payload[6] = DB1_Stream_Status
3. On state change, fire audio engine callback
```

### DB0_Play_Status Values

| Value | State | Callback |
|---|---|---|
| `0x00` | STOP | `audio_engine_stop()` |
| `0x01` | PLAYING | `audio_engine_play()` |
| `0x02` | PAUSED | `audio_engine_pause()` |
| `0x03` | FWD_SEEK | `audio_engine_next()` |
| `0x04` | REV_SEEK | `audio_engine_previous()` |
| `0x07` | WAIT_TO_PLAY | — |
| `0x08` | WAIT_TO_PAUSE | `audio_engine_pause()` |

### Sending AVRCP Commands to BT Device

Use `Music_Control (0x04)` to send playback commands outbound:

```cpp
uint8_t params[] = { 0x00, MUSIC_PLAY };
bm83_send(CMD_MUSIC_CONTROL, params, 2);
```

---

## Device Persistence (SD Card)

On `BTM_Status state=0x06` (A2DP connected), the `link_info` byte (payload[1]) contains the database index of the connected device. Use this to retrieve the full device record:

```
1. Send Read_Paired_Device_Record (0x0E), param=0x00
2. Receive Read_Paired_Device_Record_Reply (0x1F)
     payload[0]   = number of paired devices
     payload[1..] = 7 bytes per record:
                    byte 0   = link priority (1=newest, 4=oldest)
                    bytes 1-6 = BD_ADDR (low byte first)
3. Match db_index to record, write BD_ADDR + metadata to SD
```

### Minimum Data to Persist per Device

| Field | Size | Notes |
|---|---|---|
| BD_ADDR | 6 bytes | Unique device identifier |
| Link priority | 1 byte | From paired device record |
| Last connected | 4 bytes | Unix timestamp or session counter |
| Profile | 1 byte | Always `0x02` (A2DP) in this system |

> **WARNING:** SD card write triggered by link-connect must not block the I2S DMA stream. If SD and I2S share interrupt priority, defer the write using a flag and execute it from `loop()` outside the audio interrupt context.

---

## Error Handling

| Condition | Detection | Recovery |
|---|---|---|
| No init event after boot | Timeout after 3000ms past boot window | Re-assert BT_RESET, retry `bm83_init()` |
| UART checksum error | Mismatch on receive | Discard frame, log, continue |
| A2DP disconnected unexpectedly | `BTM_Status state=0x08` | Reset `last_play_status`, trigger reconnect |
| AVRCP dropped while A2DP active | `BTM_Status state=0x0C` | Drop to CONNECTED state, pause polling |
| I2S audio underrun | Teensy audio lib callback | Pad with silence, log warning |
| SD write failure on connect | SD subsystem error return | Log, do not block BT operation |
