// =============================================================================
// BM83SM1-00TA — Bluetooth A2DP Source Driver
// =============================================================================
// Profile:     A2DP Source (Teensy → BM83 → BT Speaker/Headphones)
// I2S:         Teensy slave on I2S1 — BM83 drives BCLK + LRCLK
// UART:        Serial1, 115200 8N1, no flow control
// Source:      AudioUARTCommandSet_v2_02
//
// Pin assignments (Teensy 4.1):
//   Pin 36 — PWR(MFB)     BM83 power-on control
//   Pin 32 — BT_RESET     HIGH = RST_N LOW via 2N7002 (active low reset)
//   Serial1 TX/RX         UART to BM83
//   I2S1                  Slave — BM83 is master
//
// AIC3204 codec is a separate subsystem — see aic3204_teensy.cpp.
// I2S2 audio objects are declared there and linked at build time.
// =============================================================================

#include <Arduino.h>
#include <Audio.h>

// -----------------------------------------------------------------------------
// Pin Definitions
// -----------------------------------------------------------------------------
#define MFB_PIN         36    // PWR(MFB) — HIGH to power on BM83
#define BT_RESET_PIN    32    // HIGH → MOSFET ON → RST_N LOW (chip in reset)
                              // LOW  → MOSFET OFF → RST_N HIGH via pull-up (running)

// -----------------------------------------------------------------------------
// UART
// -----------------------------------------------------------------------------
#define BM83_SERIAL      Serial8
#define BM83_BAUD        115200
#define BM83_START_BYTE  0xAA
#define RX_BUF_SIZE      64

// -----------------------------------------------------------------------------
// Command IDs (Teensy → BM83)
// Source: AudioUARTCommandSet_v2_02
// -----------------------------------------------------------------------------
#define CMD_MMI_ACTION          0x02
#define CMD_MUSIC_CONTROL       0x04
#define CMD_BTM_PARAM_SETTING   0x07
#define CMD_READ_LINK_STATUS    0x0D
#define CMD_READ_PAIRED_DEVICES 0x0E
#define CMD_PROFILE_LINK_BACK   0x17
#define CMD_DISCONNECT          0x18
#define CMD_SET_OVERALL_GAIN    0x23

// MMI_Action sub-commands (second parameter byte of CMD_MMI_ACTION)
#define MMI_ENTER_PAIRING       0x51  // Enter discoverable / pairing mode
#define MMI_A2DP_48K_PCM        0xE7  // Enter Aux-in 48kHz PCM encoder mode
#define MMI_A2DP_EXIT_PCM       0xE8  // Exit Aux-in PCM encoder mode

// Music_Control action bytes (CMD_MUSIC_CONTROL second parameter)
#define MUSIC_PLAY              0x05
#define MUSIC_PAUSE             0x06
#define MUSIC_STOP              0x08
#define MUSIC_NEXT              0x09
#define MUSIC_PREV              0x0A

// -----------------------------------------------------------------------------
// Event IDs (BM83 → Teensy)
// Source: AudioUARTCommandSet_v2_02
// -----------------------------------------------------------------------------
#define EVT_BTM_STATUS          0x01  // All link/state changes
#define EVT_LINK_STATUS_REPLY   0x1E  // Reply to CMD_READ_LINK_STATUS
#define EVT_PAIRED_DEV_REPLY    0x1F  // Reply to CMD_READ_PAIRED_DEVICES
#define EVT_BTM_INIT_COMPLETE   0x30  // Boot complete

// BTM_Status state bytes (payload[0] of EVT_BTM_STATUS)
#define BTM_STATE_POWER_OFF     0x00
#define BTM_STATE_PAIRING       0x01
#define BTM_STATE_POWER_ON      0x02
#define BTM_STATE_PAIR_OK       0x03
#define BTM_STATE_PAIR_FAIL     0x04
#define BTM_STATE_A2DP_CONN     0x06
#define BTM_STATE_A2DP_DISC     0x08
#define BTM_STATE_AVRCP_CONN    0x0B
#define BTM_STATE_AVRCP_DISC    0x0C
#define BTM_STATE_STANDBY       0x0F

// Play status values from EVT_LINK_STATUS_REPLY payload[3]
#define PLAY_STATUS_STOP        0x00
#define PLAY_STATUS_PLAYING     0x01
#define PLAY_STATUS_PAUSED      0x02
#define PLAY_STATUS_FWD_SEEK    0x03
#define PLAY_STATUS_REV_SEEK    0x04
#define PLAY_STATUS_WAIT_PLAY   0x07
#define PLAY_STATUS_WAIT_PAUSE  0x08

// -----------------------------------------------------------------------------
// I2S1 — Slave output to BM83
// WARNING: AudioOutputI2Sslave MUST be used. BM83 drives BCLK and LRCLK.
//          Using AudioOutputI2S will cause clock conflict and corrupt audio.
// I2S2 objects (AIC3204) are declared in aic3204_teensy.cpp.
// -----------------------------------------------------------------------------
AudioOutputI2Sslave   i2s1_out;
// TODO: connect your audio source to i2s1_out via AudioConnection

// -----------------------------------------------------------------------------
// State Machine
// -----------------------------------------------------------------------------
typedef enum {
    BM83_STATE_OFF,
    BM83_STATE_BOOTING,
    BM83_STATE_READY,
    BM83_STATE_SCANNING,
    BM83_STATE_CONNECTING,
    BM83_STATE_CONNECTED,
    BM83_STATE_STREAMING
} bm83_state_t;

static bm83_state_t bm83_state       = BM83_STATE_OFF;
static uint8_t      last_play_status = 0xFF;  // 0xFF = uninitialised
static uint32_t     last_poll_time   = 0;

#define AVRCP_POLL_INTERVAL_MS  300

// -----------------------------------------------------------------------------
// UART RX Buffer
// -----------------------------------------------------------------------------
static uint8_t rx_buf[RX_BUF_SIZE];
static uint8_t rx_len = 0;

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
void bm83_init();
void bm83_send(uint8_t cmd_id, uint8_t *params, uint8_t param_len);
void bm83_uart_poll();
void bm83_handle_event(uint8_t event_id, uint8_t *payload, uint8_t payload_len);
void bm83_handle_btm_status(uint8_t state, uint8_t link_info);
void bm83_handle_play_status(uint8_t status);
void bm83_poll_playback_status();
void bm83_start_discovery();
void bm83_connect(uint8_t *bdaddr);
void bm83_connect_last();
void bm83_disconnect();
void audio_engine_play();
void audio_engine_pause();
void audio_engine_stop();
void audio_engine_next();
void audio_engine_previous();
void sd_save_device(uint8_t db_index);

// =============================================================================
// Setup & Loop
// =============================================================================

void setup() {
    Serial.begin(115200);
    BM83_SERIAL.begin(BM83_BAUD);
    AudioMemory(16);
    bm83_init();
}

void loop() {
    bm83_uart_poll();
    bm83_poll_playback_status();
    // TODO: audio source update logic
}

// =============================================================================
// Initialization
// =============================================================================

void bm83_init() {
    pinMode(BT_RESET_PIN, OUTPUT);
    pinMode(MFB_PIN, OUTPUT);

    // Step 1: Assert RST_N LOW — BT_RESET HIGH turns MOSFET on, pulling RST_N LOW
    digitalWrite(BT_RESET_PIN, HIGH);
    digitalWrite(MFB_PIN, LOW);
    delay(10);

    // Step 2: MFB must go HIGH before RST_N is released
    digitalWrite(MFB_PIN, HIGH);
    delay(20);

    // Step 3: Release RST_N — BT_RESET LOW turns MOSFET off, RST_N floats HIGH via pull-up
    digitalWrite(BT_RESET_PIN, LOW);

    // Step 4: 500ms boot window — poll UART but state machine stays in BOOTING
    bm83_state = BM83_STATE_BOOTING;
    uint32_t boot_end = millis() + 500;
    while (millis() < boot_end) {
        bm83_uart_poll();
    }

    // Step 5: Wait for EVT_BTM_INIT_COMPLETE (0x30), status byte 0x00
    Serial.println("[BM83] Waiting for init complete...");
    uint32_t timeout = millis() + 3000;
    while (bm83_state == BM83_STATE_BOOTING && millis() < timeout) {
        bm83_uart_poll();
    }

    if (bm83_state != BM83_STATE_READY) {
        Serial.println("[BM83] ERROR: Init timeout — check MFB (pin 36) and RST_N (pin 32)");
        return;
    }

    // Step 6: Set A2DP source mode at 48kHz
    // MMI_Action (0x02), data_base_index=0x00, action=0xE7
    uint8_t mmi_48k[] = { 0x00, MMI_A2DP_48K_PCM };
    bm83_send(CMD_MMI_ACTION, mmi_48k, 2);
    Serial.println("[BM83] A2DP 48kHz PCM encoder mode set");
}

// =============================================================================
// UART — Transmit
// =============================================================================

void bm83_send(uint8_t cmd_id, uint8_t *params, uint8_t param_len) {
    uint8_t frame[RX_BUF_SIZE];
    uint8_t total_len = param_len + 1;  // +1 for cmd_id byte

    frame[0] = BM83_START_BYTE;
    frame[1] = (total_len >> 8) & 0xFF;
    frame[2] = total_len & 0xFF;
    frame[3] = cmd_id;

    for (uint8_t i = 0; i < param_len; i++) {
        frame[4 + i] = params[i];
    }

    uint16_t sum = frame[1] + frame[2] + frame[3];
    for (uint8_t i = 0; i < param_len; i++) sum += params[i];
    frame[4 + param_len] = (uint8_t)(0x100 - (sum & 0xFF));

    BM83_SERIAL.write(frame, 5 + param_len);
}

// =============================================================================
// UART — Receive & Parse
// =============================================================================

void bm83_uart_poll() {
    while (BM83_SERIAL.available()) {
        uint8_t b = BM83_SERIAL.read();

        if (rx_len == 0 && b != BM83_START_BYTE) continue;

        if (rx_len >= RX_BUF_SIZE) {
            Serial.println("[BM83] RX overflow — resetting");
            rx_len = 0;
            continue;
        }

        rx_buf[rx_len++] = b;

        if (rx_len < 4) continue;

        uint16_t payload_len = ((uint16_t)rx_buf[1] << 8) | rx_buf[2];
        uint16_t total_frame = payload_len + 4;  // start(1) + len(2) + data + chk(1)

        if (rx_len < total_frame) continue;

        uint16_t sum = 0;
        for (uint8_t i = 1; i < total_frame - 1; i++) sum += rx_buf[i];
        uint8_t expected_chk = (uint8_t)(0x100 - (sum & 0xFF));

        if (rx_buf[total_frame - 1] != expected_chk) {
            Serial.println("[BM83] Checksum error — frame discarded");
            rx_len = 0;
            continue;
        }

        uint8_t  event_id = rx_buf[3];
        uint8_t *payload  = &rx_buf[4];
        uint8_t  plen     = (uint8_t)(payload_len - 1);

        bm83_handle_event(event_id, payload, plen);
        rx_len = 0;
    }
}

// =============================================================================
// Event Handler
// =============================================================================

void bm83_handle_event(uint8_t event_id, uint8_t *payload, uint8_t payload_len) {
    switch (event_id) {

        case EVT_BTM_INIT_COMPLETE:
            if (payload_len >= 1 && payload[0] == 0x00) {
                Serial.println("[BM83] Init complete");
                bm83_state = BM83_STATE_READY;
            }
            break;

        case EVT_BTM_STATUS:
            if (payload_len >= 1) {
                uint8_t link_info = (payload_len > 1) ? payload[1] : 0x00;
                bm83_handle_btm_status(payload[0], link_info);
            }
            break;

        case EVT_LINK_STATUS_REPLY:
            // payload[0]=Device_State, [1]=DB0_Connect, [2]=DB1_Connect,
            // [3]=DB0_Play_Status,     [4]=DB1_Play_Status,
            // [5]=DB0_Stream_Status,   [6]=DB1_Stream_Status
            if (payload_len >= 4) {
                bm83_handle_play_status(payload[3]);
            }
            break;

        case EVT_PAIRED_DEV_REPLY:
            // payload[0] = number of paired devices
            // payload[1..] = 7 bytes per record: [priority(1), BD_ADDR(6)]
            // TODO: parse and pass to SD persistence layer
            Serial.print("[BM83] Paired device records: ");
            Serial.println(payload[0]);
            break;

        default:
            Serial.print("[BM83] Unhandled event: 0x");
            Serial.println(event_id, HEX);
            break;
    }
}

void bm83_handle_btm_status(uint8_t state, uint8_t link_info) {
    switch (state) {

        case BTM_STATE_POWER_ON:
            Serial.println("[BM83] Power on");
            break;

        case BTM_STATE_STANDBY:
            Serial.println("[BM83] Standby");
            bm83_state = BM83_STATE_READY;
            break;

        case BTM_STATE_PAIRING:
            Serial.println("[BM83] Discoverable / pairing");
            bm83_state = BM83_STATE_SCANNING;
            break;

        case BTM_STATE_PAIR_OK:
            Serial.println("[BM83] Pairing successful");
            break;

        case BTM_STATE_PAIR_FAIL:
            Serial.println("[BM83] Pairing failed");
            bm83_state = BM83_STATE_READY;
            break;

        case BTM_STATE_A2DP_CONN:
            Serial.println("[BM83] A2DP connected");
            bm83_state = BM83_STATE_CONNECTED;
            // link_info = database index of connected device
            // Retrieve full device record for SD: send CMD_READ_PAIRED_DEVICES (0x0E)
            // then handle EVT_PAIRED_DEV_REPLY (0x1F) for BD_ADDR
            // TODO: sd_save_device(link_info);
            break;

        case BTM_STATE_A2DP_DISC:
            Serial.println("[BM83] A2DP disconnected");
            bm83_state = BM83_STATE_READY;
            last_play_status = 0xFF;
            // TODO: reconnect logic
            break;

        case BTM_STATE_AVRCP_CONN:
            Serial.println("[BM83] AVRCP connected — streaming active");
            bm83_state = BM83_STATE_STREAMING;
            break;

        case BTM_STATE_AVRCP_DISC:
            Serial.println("[BM83] AVRCP disconnected");
            if (bm83_state == BM83_STATE_STREAMING) {
                bm83_state = BM83_STATE_CONNECTED;
            }
            break;

        default:
            Serial.print("[BM83] BTM_Status state: 0x");
            Serial.println(state, HEX);
            break;
    }
}

// =============================================================================
// AVRCP Playback State Polling
// BM83 does NOT push AVRCP passthrough events to the MCU unsolicited.
// Playback state is obtained by polling Read_Link_Status (0x0D) every 300ms.
// Transitions are detected by comparing against last known state.
// =============================================================================

void bm83_poll_playback_status() {
    if (bm83_state != BM83_STATE_STREAMING) return;
    if (millis() - last_poll_time < AVRCP_POLL_INTERVAL_MS) return;
    last_poll_time = millis();

    uint8_t param = 0x00;
    bm83_send(CMD_READ_LINK_STATUS, &param, 1);
}

void bm83_handle_play_status(uint8_t status) {
    if (status == last_play_status) return;
    last_play_status = status;

    switch (status) {
        case PLAY_STATUS_PLAYING:
            Serial.println("[AVRCP] PLAYING");
            audio_engine_play();
            break;
        case PLAY_STATUS_PAUSED:
        case PLAY_STATUS_WAIT_PAUSE:
            Serial.println("[AVRCP] PAUSED");
            audio_engine_pause();
            break;
        case PLAY_STATUS_STOP:
            Serial.println("[AVRCP] STOP");
            audio_engine_stop();
            break;
        case PLAY_STATUS_FWD_SEEK:
            Serial.println("[AVRCP] FWD SEEK");
            audio_engine_next();
            break;
        case PLAY_STATUS_REV_SEEK:
            Serial.println("[AVRCP] REV SEEK");
            audio_engine_previous();
            break;
        default:
            break;
    }
}

// =============================================================================
// BM83 Command Functions
// =============================================================================

void bm83_start_discovery() {
    Serial.println("[BM83] Entering pairing/discoverable mode");
    bm83_state = BM83_STATE_SCANNING;
    uint8_t params[] = { 0x00, MMI_ENTER_PAIRING };
    bm83_send(CMD_MMI_ACTION, params, 2);
}

void bm83_connect(uint8_t *bdaddr) {
    // Profile_Link_Back (0x17), Type=0x05, Profile=A2DP, BT_Addr=bdaddr
    Serial.println("[BM83] Connecting to specified device");
    bm83_state = BM83_STATE_CONNECTING;
    uint8_t params[9];
    params[0] = 0x05;  // Type: connect to specified BD address
    params[1] = 0x00;  // Device_Index
    params[2] = 0x02;  // Profile: A2DP
    memcpy(&params[3], bdaddr, 6);
    bm83_send(CMD_PROFILE_LINK_BACK, params, 9);
}

void bm83_connect_last() {
    // Profile_Link_Back (0x17), Type=0x02 = last A2DP device
    Serial.println("[BM83] Connecting to last A2DP device");
    bm83_state = BM83_STATE_CONNECTING;
    uint8_t params[] = { 0x02 };
    bm83_send(CMD_PROFILE_LINK_BACK, params, 1);
}

void bm83_disconnect() {
    Serial.println("[BM83] Disconnecting");
    uint8_t params[] = { 0x00, 0x00 };  // database_index=0, flag=0 (all profiles)
    bm83_send(CMD_DISCONNECT, params, 2);
}

// =============================================================================
// Audio Engine Callbacks
// TODO: Replace stubs with actual audio source control
// =============================================================================

void audio_engine_play()     { Serial.println("[AUDIO] Play");     }
void audio_engine_pause()    { Serial.println("[AUDIO] Pause");    }
void audio_engine_stop()     { Serial.println("[AUDIO] Stop");     }
void audio_engine_next()     { Serial.println("[AUDIO] Next");     }
void audio_engine_previous() { Serial.println("[AUDIO] Previous"); }

// =============================================================================
// SD Persistence Stub
// Triggered on BTM_STATE_A2DP_CONN. Must not block I2S DMA.
// Use CMD_READ_PAIRED_DEVICES (0x0E) → EVT_PAIRED_DEV_REPLY (0x1F) to get BD_ADDR.
// WARNING: Defer write if SD and I2S share interrupt priority.
// =============================================================================

void sd_save_device(uint8_t db_index) {
    // TODO: send CMD_READ_PAIRED_DEVICES, extract BD_ADDR for db_index, write to SD
    Serial.print("[SD] Save triggered for DB index: ");
    Serial.println(db_index);
}
