// =============================================================================
// tusb320_teensy.cpp
// TUSB320 USB Type-C CC Controller Driver
//
// Interface:  I2C0 (Wire) — shared PM bus with BQ25895 and BQ27441
// Address:    0x60 (ADDR pin tied to GND)
// Mode:       UFP only — PORT pin hardwired LOW
//
// Role in system:
//   The TUSB320 presents Rd pull-downs on CC1/CC2 so a host computer recognizes
//   this device as a USB UFP and asserts VBUS. It does not handle USB data.
//   All file transfer is handled by the Teensy's native USB peripheral over D+/D-.
//
// This driver:
//   - Verifies device presence and identity at init
//   - Clears INTERRUPT_STATUS on startup and after each state change
//   - Polls ATTACHED_STATE every 500ms
//   - Exposes tusb320_is_attached() for USB MSD gating
//   - Reports current advertisement level (informational — BQ25895 owns charge control)
//
// WARNING: Wire.begin() / Wire.setClock() are NOT called here.
//          They are owned by the shared system init function.
//
// WARNING: Do NOT call I2C_SOFT_RESET. It resets status registers and re-samples
//          PORT pin (which stays LOW), providing no benefit and causing a
//          momentary detach event. PORT is hardwired — mode cannot change.
//
// WARNING: Do NOT write MODE_SELECT (REG0A[5:4]). PORT is hardwired LOW (UFP).
//          MODE_SELECT only takes effect in unattached state and is redundant here.
//
// Future MSD code must gate all USB transfers on tusb320_is_attached() == true.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>

// -----------------------------------------------------------------------------
// I2C
// -----------------------------------------------------------------------------
#define TUSB320_I2C_ADDR    0x60    // ADDR pin tied to GND

// -----------------------------------------------------------------------------
// Register Map
// Source: TUSB320 Datasheet §7.6 (SLLSEN9F)
// -----------------------------------------------------------------------------
#define TUSB_REG_DEVICE_ID_BASE     0x00    // 0x00–0x07: ASCII device ID "TUSB320\0"
#define TUSB_REG_STATUS1            0x08    // CURRENT_MODE_DETECT, ACCESSORY_CONNECTED
#define TUSB_REG_STATUS2            0x09    // ATTACHED_STATE, CABLE_DIR, INTERRUPT_STATUS
#define TUSB_REG_CONTROL            0x0A    // DEBOUNCE, MODE_SELECT, I2C_SOFT_RESET
#define TUSB_REG_MISC               0x45    // DISABLE_RD_RP (do not set)

// -----------------------------------------------------------------------------
// REG08 (STATUS1) Bit Fields
// -----------------------------------------------------------------------------
#define TUSB_CURRENT_MODE_ADVERTISE_SHIFT   6
#define TUSB_CURRENT_MODE_ADVERTISE_MASK    0xC0
#define TUSB_CURRENT_MODE_DETECT_SHIFT      4
#define TUSB_CURRENT_MODE_DETECT_MASK       0x30
#define TUSB_ACCESSORY_CONNECTED_SHIFT      1
#define TUSB_ACCESSORY_CONNECTED_MASK       0x0E
#define TUSB_ACTIVE_CABLE_BIT               0x01

// CURRENT_MODE_DETECT values (UFP reading what DFP advertises)
#define TUSB_CURRENT_DEFAULT    0x00    // 500mA (USB 2.0) or 900mA (USB 3.1)
#define TUSB_CURRENT_MEDIUM     0x01    // 1.5A
#define TUSB_CURRENT_ACC_500    0x02    // Charge-through accessory 500mA
#define TUSB_CURRENT_HIGH       0x03    // 3A

// -----------------------------------------------------------------------------
// REG09 (STATUS2) Bit Fields
// -----------------------------------------------------------------------------
#define TUSB_ATTACHED_STATE_SHIFT   6
#define TUSB_ATTACHED_STATE_MASK    0xC0
#define TUSB_CABLE_DIR_BIT          0x20
#define TUSB_INTERRUPT_STATUS_BIT   0x10

// ATTACHED_STATE values
#define TUSB_STATE_NOT_ATTACHED     0x00    // Unattached.SNK
#define TUSB_STATE_ATTACHED_SRC     0x01    // DFP connected (host) — normal
#define TUSB_STATE_ATTACHED_SNK     0x02    // UFP — should not occur (PORT is hardwired UFP)
#define TUSB_STATE_ACCESSORY        0x03    // Audio or debug accessory

// -----------------------------------------------------------------------------
// Device ID
// -----------------------------------------------------------------------------
// Registers 0x07..0x00 (MSB to LSB) = { 0x00, 0x54, 0x55, 0x53, 0x42, 0x33, 0x32, 0x30 }
// Read 0x00..0x07 sequentially = { 0x30, 0x32, 0x33, 0x42, 0x53, 0x55, 0x54, 0x00 }
// ASCII from low address: '0','2','3','B','S','U','T','\0' = "023BSUT\0" (reversed)
// Verify by checking byte at 0x01 == '2' (0x32) and byte at 0x05 == 'U' (0x55), etc.
// Simplest check: read 0x00, confirm it is 0x30 ('0') and 0x07 is 0x00.
static const uint8_t TUSB_DEVICE_ID_EXPECTED[8] = {
    0x30, 0x32, 0x33, 0x42, 0x53, 0x55, 0x54, 0x00
};

// -----------------------------------------------------------------------------
// Driver State
// -----------------------------------------------------------------------------
static bool     _tusb_initialized   = false;
static bool     _tusb_attached      = false;
static uint8_t  _tusb_current_mode  = TUSB_CURRENT_DEFAULT;
static uint8_t  _tusb_cable_dir     = 0;
static uint32_t _tusb_last_poll_ms  = 0;

#define TUSB_POLL_INTERVAL_MS   500

// -----------------------------------------------------------------------------
// Internal Helpers
// -----------------------------------------------------------------------------

static bool tusb320_write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(TUSB320_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

static bool tusb320_read(uint8_t reg, uint8_t* out) {
    Wire.beginTransmission(TUSB320_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)TUSB320_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return false;
    *out = Wire.read();
    return true;
}

static bool tusb320_read_multi(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(TUSB320_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)TUSB320_I2C_ADDR, len);
    if (Wire.available() < len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

// Clear INTERRUPT_STATUS (REG09 bit[4]) by writing 1 to it.
// Other writable bits in REG09 (DRP_DUTY_CYCLE [2:1]) must be preserved.
// In UFP mode DRP_DUTY_CYCLE is irrelevant, but write back what we read
// to avoid accidentally toggling reserved bits.
static bool tusb320_clear_interrupt(uint8_t reg09_current) {
    uint8_t write_val = reg09_current | TUSB_INTERRUPT_STATUS_BIT;
    return tusb320_write(TUSB_REG_STATUS2, write_val);
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

// tusb320_init()
// Call once from setup() after Wire.begin() / Wire.setClock() have been called
// by the shared system init. Returns true on success.
bool tusb320_init() {
    _tusb_initialized = false;

    // --- 1. Verify device is present ---
    uint8_t probe = 0;
    if (!tusb320_read(TUSB_REG_DEVICE_ID_BASE, &probe)) {
        Serial.println("[TUSB320] ERROR: I2C NACK — device not found at 0x60");
        return false;
    }

    // --- 2. Verify device ID ---
    uint8_t id_buf[8] = {0};
    if (!tusb320_read_multi(TUSB_REG_DEVICE_ID_BASE, id_buf, 8)) {
        Serial.println("[TUSB320] ERROR: Failed to read DEVICE_ID");
        return false;
    }
    for (uint8_t i = 0; i < 8; i++) {
        if (id_buf[i] != TUSB_DEVICE_ID_EXPECTED[i]) {
            Serial.print("[TUSB320] ERROR: DEVICE_ID mismatch at byte ");
            Serial.print(i);
            Serial.print(" — got 0x");
            Serial.print(id_buf[i], HEX);
            Serial.print(", expected 0x");
            Serial.println(TUSB_DEVICE_ID_EXPECTED[i], HEX);
            return false;
        }
    }
    Serial.println("[TUSB320] Device ID verified: TUSB320");

    // --- 3. Read initial status ---
    uint8_t reg08 = 0, reg09 = 0;
    if (!tusb320_read(TUSB_REG_STATUS1, &reg08) ||
        !tusb320_read(TUSB_REG_STATUS2, &reg09)) {
        Serial.println("[TUSB320] ERROR: Failed to read status registers");
        return false;
    }

    uint8_t attached_state = (reg09 & TUSB_ATTACHED_STATE_MASK) >> TUSB_ATTACHED_STATE_SHIFT;
    _tusb_cable_dir    = (reg09 & TUSB_CABLE_DIR_BIT) ? 1 : 0;
    _tusb_current_mode = (reg08 & TUSB_CURRENT_MODE_DETECT_MASK) >> TUSB_CURRENT_MODE_DETECT_SHIFT;
    _tusb_attached     = (attached_state == TUSB_STATE_ATTACHED_SRC);

    Serial.print("[TUSB320] Init state — ATTACHED_STATE: ");
    Serial.print(attached_state);
    Serial.print(", CABLE_DIR: CC");
    Serial.print(_tusb_cable_dir ? "2" : "1");
    Serial.print(", CURRENT_MODE: ");
    Serial.println(_tusb_current_mode);

    if (attached_state == TUSB_STATE_ATTACHED_SNK) {
        Serial.println("[TUSB320] WARNING: Unexpected Attached.SNK state — PORT is hardwired UFP");
    }
    if (attached_state == TUSB_STATE_ACCESSORY) {
        Serial.println("[TUSB320] INFO: Accessory connected at init");
    }

    // --- 4. Clear INTERRUPT_STATUS ---
    if (reg09 & TUSB_INTERRUPT_STATUS_BIT) {
        if (!tusb320_clear_interrupt(reg09)) {
            Serial.println("[TUSB320] WARNING: Failed to clear INTERRUPT_STATUS at init");
        }
    }

    _tusb_initialized  = true;
    _tusb_last_poll_ms = millis();

    Serial.print("[TUSB320] Init complete. USB-C host ");
    Serial.println(_tusb_attached ? "attached." : "not attached.");
    return true;
}

// tusb320_poll()
// Call from main loop. Polls attach state every TUSB_POLL_INTERVAL_MS.
// Logs state transitions. Clears INTERRUPT_STATUS on change.
void tusb320_poll() {
    if (!_tusb_initialized) return;
    if ((millis() - _tusb_last_poll_ms) < TUSB_POLL_INTERVAL_MS) return;
    _tusb_last_poll_ms = millis();

    uint8_t reg08 = 0, reg09 = 0;
    if (!tusb320_read(TUSB_REG_STATUS1, &reg08) ||
        !tusb320_read(TUSB_REG_STATUS2, &reg09)) {
        Serial.println("[TUSB320] WARNING: Poll I2C read failed — state stale");
        return;
    }

    uint8_t attached_state = (reg09 & TUSB_ATTACHED_STATE_MASK) >> TUSB_ATTACHED_STATE_SHIFT;
    bool    now_attached   = (attached_state == TUSB_STATE_ATTACHED_SRC);
    uint8_t new_cable_dir  = (reg09 & TUSB_CABLE_DIR_BIT) ? 1 : 0;
    uint8_t new_current    = (reg08 & TUSB_CURRENT_MODE_DETECT_MASK) >> TUSB_CURRENT_MODE_DETECT_SHIFT;

    // Log attach/detach transitions
    if (now_attached != _tusb_attached) {
        if (now_attached) {
            Serial.print("[TUSB320] USB-C host attached. CC");
            Serial.print(new_cable_dir ? "2" : "1");
            Serial.print(" active. Current mode: ");
            Serial.println(new_current);
        } else {
            Serial.println("[TUSB320] USB-C host detached.");
        }
        _tusb_attached = now_attached;
    }

    // Log unexpected states
    if (attached_state == TUSB_STATE_ATTACHED_SNK) {
        Serial.println("[TUSB320] WARNING: Unexpected Attached.SNK — PORT is hardwired UFP");
    }
    if (attached_state == TUSB_STATE_ACCESSORY) {
        Serial.println("[TUSB320] INFO: Accessory state detected");
    }

    _tusb_cable_dir    = new_cable_dir;
    _tusb_current_mode = new_current;

    // Clear INTERRUPT_STATUS if set
    if (reg09 & TUSB_INTERRUPT_STATUS_BIT) {
        if (!tusb320_clear_interrupt(reg09)) {
            Serial.println("[TUSB320] WARNING: Failed to clear INTERRUPT_STATUS");
        }
    }
}

// tusb320_is_attached()
// Returns true when a USB host (DFP) is connected and VBUS is present.
// Gate all USB MSD operations on this function returning true.
bool tusb320_is_attached() {
    return _tusb_initialized && _tusb_attached;
}

// tusb320_get_current_mode()
// Returns the Type-C current advertisement detected from the connected host.
// Values: TUSB_CURRENT_DEFAULT (0), TUSB_CURRENT_MEDIUM (1),
//         TUSB_CURRENT_ACC_500 (2), TUSB_CURRENT_HIGH (3)
// Informational only — BQ25895 DPDM handles charge current independently.
// Only valid when tusb320_is_attached() == true.
uint8_t tusb320_get_current_mode() {
    return _tusb_current_mode;
}

// tusb320_get_cable_dir()
// Returns active CC pin: 0 = CC1, 1 = CC2.
// Informational only — no mux switching required for USB 2.0.
uint8_t tusb320_get_cable_dir() {
    return _tusb_cable_dir;
}

// tusb320_is_initialized()
// Returns true if init completed successfully.
bool tusb320_is_initialized() {
    return _tusb_initialized;
}

// -----------------------------------------------------------------------------
// MSD Readiness Helper (stub for future MSD subsystem)
// -----------------------------------------------------------------------------

// tusb320_usb_ready_for_transfer()
// Returns true when conditions are met for USB file transfer operations:
//   - Driver initialized
//   - USB-C host attached (ATTACHED_STATE == Attached.SRC)
//
// Future MSD code should call this before:
//   - Initiating USB enumeration
//   - Opening MSD endpoints
//   - Beginning any file read/write operations
//
// This function is intentionally separate from tusb320_is_attached() so that
// the MSD layer can add its own readiness conditions (e.g. SD card mounted)
// without modifying this driver.
bool tusb320_usb_ready_for_transfer() {
    return tusb320_is_attached();
}
