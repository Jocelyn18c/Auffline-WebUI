// =============================================================================
// BQ25895 — LiPo Charger Driver
// =============================================================================
// Interface:   I2C0 (Wire), address 0x6A
// Bus:         Shared with BQ27441 (fuel gauge) and TUSB320 (USB-C PD)
// Source:      BQ25895 Datasheet SLUSC88C (TI, Oct 2022)
//
// Pin assignments (Teensy 4.1):
//   Wire SCL/SDA — I2C0 shared PM bus (Wire.begin() owned by system init)
//
// STAT pin: Disabled in software (STAT_DIS=1 in REG07) — not connected on PCB
// INT pin:  Not connected on PCB — no interrupt handler
//           Poll REG0B (status) and REG0C (faults) from the main loop instead
//
// OTG:      Disabled — device is USB peripheral, not USB host
//
// Watchdog: Disabled in init (REG07[5:4]=00). Hardware safety nets remain
//           active: 12h safety timer, thermal shutdown, OVP, NTC protection.
//
// Fuse limit: System fuses rated 1.8A. ICHG = 1472mA (N=23 x 64mA).
//             Do NOT increase ICHG without revisiting fuse ratings.
//             NVDC path means charge + system load share VBUS — effective
//             charge current under full system load will be lower than 1472mA.
//
// Battery:  2x 2200mAh LiPo in parallel = 4400mAh. ICHG = ~0.33C.
//
// WARNING:  Wire.begin() / Wire.setClock() are NOT called here.
//           They are owned by the shared system init function.
//           Do not add Wire.begin() to this driver.
//
// WARNING:  There is a brief window between POR and init completion where the
//           charger runs at the default 2.048A (exceeds fuse limit).
//           Call bq25895_init() as early as possible in setup().
// =============================================================================

#include <Arduino.h>
#include <Wire.h>

// -----------------------------------------------------------------------------
// I2C
// -----------------------------------------------------------------------------
#define BQ25895_I2C_ADDR    0x6A    // Fixed — not configurable

// -----------------------------------------------------------------------------
// Register Map
// Source: BQ25895 Datasheet §8.4 (REG00–REG14)
// -----------------------------------------------------------------------------
#define BQ_REG00    0x00    // Input source control (EN_HIZ, EN_ILIM, IINLIM)
#define BQ_REG01    0x01    // Boost thresholds, VINDPM offset
#define BQ_REG02    0x02    // ADC control (CONV_START, CONV_RATE), ICO, DPDM
#define BQ_REG03    0x03    // WD_RST, OTG_CONFIG, CHG_CONFIG, SYS_MIN
#define BQ_REG04    0x04    // Fast charge current limit (ICHG)
#define BQ_REG05    0x05    // Pre-charge current (IPRECHG), termination current (ITERM)
#define BQ_REG06    0x06    // Charge voltage limit (VREG), BATLOWV, VRECHG
#define BQ_REG07    0x07    // TERM_EN, STAT_DIS, WATCHDOG, EN_TIMER, CHG_TIMER
#define BQ_REG08    0x08    // IR compensation (BAT_COMP, VCLAMP), TREG
#define BQ_REG09    0x09    // OTG voltage, BATFET_DIS, PUMPX control
#define BQ_REG0A    0x0A    // Boost current limit
#define BQ_REG0B    0x0B    // Status register [R] — VBUS_STAT, CHRG_STAT, PG_STAT
#define BQ_REG0C    0x0C    // Fault register [R] — read-twice for live state, no multi-read
#define BQ_REG0D    0x0D    // ADC: VBAT [R] — 2304mV + raw[6:0] x 20mV
#define BQ_REG0E    0x0E    // ADC: VBAT duplicate [R]
#define BQ_REG0F    0x0F    // ADC: VSYS [R] — 2304mV + raw[6:0] x 20mV
#define BQ_REG10    0x10    // ADC: TS% [R] — 21% + raw[6:0] x 0.465%
#define BQ_REG11    0x11    // ADC: VBUS [R] — 2600mV + raw[6:0] x 100mV, bit[7]=VBUS_GD
#define BQ_REG12    0x12    // ADC: ICHG [R] — raw[6:0] x 50mA (0 if VBAT < VBATSHORT)
#define BQ_REG13    0x13    // VINDPM/IINDPM status, ICO current limit [R]
#define BQ_REG14    0x14    // REG_RST, ICO status, device PN, DEV_REV [R/W]

// -----------------------------------------------------------------------------
// REG0B — Status field masks and values
// -----------------------------------------------------------------------------
#define BQ_VBUS_STAT_MASK   0xE0
#define BQ_VBUS_STAT_SHIFT  5
#define BQ_CHRG_STAT_MASK   0x18
#define BQ_CHRG_STAT_SHIFT  3
#define BQ_PG_STAT          0x04    // bit 2 — power good
#define BQ_VSYS_STAT        0x01    // bit 0 — in VSYSMIN regulation

// VBUS_STAT[2:0]
#define BQ_VBUS_NONE        0x00    // No input
#define BQ_VBUS_SDP         0x01    // USB SDP 500mA
#define BQ_VBUS_CDP         0x02    // USB CDP 1.5A
#define BQ_VBUS_DCP         0x03    // USB DCP 3.25A
#define BQ_VBUS_MAXCHG      0x04    // MaxCharge DCP 1.5A
#define BQ_VBUS_UNKNOWN     0x05    // Unknown adapter 500mA
#define BQ_VBUS_NONSTAND    0x06    // Non-standard adapter
#define BQ_VBUS_OTG         0x07    // OTG (should not occur — OTG disabled)

// CHRG_STAT[1:0]
#define BQ_CHRG_NONE        0x00    // Not charging
#define BQ_CHRG_PRECHARGE   0x01    // Pre-charge (VBAT < 3.0V)
#define BQ_CHRG_FAST        0x02    // Fast charging (CC or CV)
#define BQ_CHRG_DONE        0x03    // Charge termination done

// -----------------------------------------------------------------------------
// REG0C — Fault field masks and values
// -----------------------------------------------------------------------------
#define BQ_FAULT_WATCHDOG       0x80    // bit 7
#define BQ_FAULT_BOOST          0x40    // bit 6 (OTG disabled — should never set)
#define BQ_FAULT_CHRG_MASK      0x30    // bits[5:4]
#define BQ_FAULT_CHRG_SHIFT     4
#define BQ_FAULT_BAT            0x08    // bit 3 — battery OVP
#define BQ_FAULT_NTC_MASK       0x07    // bits[2:0]

// CHRG_FAULT[1:0]
#define BQ_CHRG_FAULT_NONE      0x00
#define BQ_CHRG_FAULT_INPUT     0x01    // VBUS > VACOV or VBUS < VVBUSMIN
#define BQ_CHRG_FAULT_THERMAL   0x02    // Thermal shutdown
#define BQ_CHRG_FAULT_TIMER     0x03    // 12h safety timer expired

// NTC_FAULT[2:0] (buck mode)
#define BQ_NTC_NORMAL           0x00
#define BQ_NTC_COLD             0x01
#define BQ_NTC_HOT              0x02

// -----------------------------------------------------------------------------
// REG14 — Device identity
// PN[2:0] (bits[5:3]) = 0b111 = 7 identifies BQ25895
// -----------------------------------------------------------------------------
#define BQ_DEVICE_PN_MASK       0x38
#define BQ_DEVICE_PN_SHIFT      3
#define BQ_EXPECTED_PN          0x07

// -----------------------------------------------------------------------------
// ADC conversion macros
// Source: BQ25895 Datasheet §8.4.14–8.4.19
// -----------------------------------------------------------------------------
#define BQ_VBAT_MV(raw)     (2304U + ((uint16_t)((raw) & 0x7F) * 20U))
#define BQ_VSYS_MV(raw)     (2304U + ((uint16_t)((raw) & 0x7F) * 20U))
#define BQ_VBUS_MV(raw)     (2600U + ((uint16_t)((raw) & 0x7F) * 100U))
#define BQ_VBUS_GD(raw)     (((raw) >> 7) & 0x01)
#define BQ_ICHG_MA(raw)     ((uint16_t)((raw) & 0x7F) * 50U)

// -----------------------------------------------------------------------------
// Private state
// -----------------------------------------------------------------------------
static bool _initialized = false;

// =============================================================================
// Low-level I2C
// =============================================================================

static bool bq25895_write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(BQ25895_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
        Serial.print("[BQ25895] Write FAIL reg=0x");
        Serial.print(reg, HEX);
        Serial.print(" err=");
        Serial.println(result);
        return false;
    }
    return true;
}

static int16_t bq25895_read(uint8_t reg) {
    Wire.beginTransmission(BQ25895_I2C_ADDR);
    Wire.write(reg);
    uint8_t result = Wire.endTransmission(false);  // Repeated START
    if (result != 0) {
        Serial.print("[BQ25895] Read START FAIL reg=0x");
        Serial.print(reg, HEX);
        Serial.print(" err=");
        Serial.println(result);
        return -1;
    }
    uint8_t n = Wire.requestFrom((uint8_t)BQ25895_I2C_ADDR, (uint8_t)1);
    if (n != 1) {
        Serial.print("[BQ25895] Read NACK reg=0x");
        Serial.println(reg, HEX);
        return -1;
    }
    return (int16_t)Wire.read();
}

// Read-modify-write — preserves bits outside mask
static bool bq25895_modify(uint8_t reg, uint8_t mask, uint8_t value) {
    int16_t current = bq25895_read(reg);
    if (current < 0) return false;
    return bq25895_write(reg, ((uint8_t)current & ~mask) | (value & mask));
}

// =============================================================================
// Device Verification
// =============================================================================

static bool bq25895_verify_id() {
    int16_t reg14 = bq25895_read(BQ_REG14);
    if (reg14 < 0) return false;

    uint8_t pn = ((uint8_t)reg14 & BQ_DEVICE_PN_MASK) >> BQ_DEVICE_PN_SHIFT;
    if (pn != BQ_EXPECTED_PN) {
        Serial.print("[BQ25895] ID mismatch — expected PN=7, got PN=");
        Serial.println(pn);
        return false;
    }

    Serial.println("[BQ25895] Device ID OK (PN=7, BQ25895)");
    return true;
}

// =============================================================================
// I2C Bus Scan — bring-up utility, not called in production
// =============================================================================

void bq25895_scan_bus() {
    Serial.println("[BQ25895] Scanning I2C0...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  0x");
            Serial.println(addr, HEX);
        }
    }
}

// =============================================================================
// Initialization
// =============================================================================

bool bq25895_init() {
    // Verify device present and correct before touching any registers
    if (!bq25895_verify_id()) {
        Serial.println("[BQ25895] Init FAILED — device not found or wrong ID");
        return false;
    }

    // -------------------------------------------------------------------------
    // REG07 first — disables watchdog immediately.
    // Any I2C write starts the 40s watchdog; disabling it here prevents
    // register reset partway through init.
    //
    // REG07 = 0xCC:
    //   bit 7   TERM_EN   = 1  (charge termination enabled)
    //   bit 6   STAT_DIS  = 1  (STAT pin disabled — not connected on PCB)
    //   bits5:4 WATCHDOG  = 00 (disabled)
    //   bit 3   EN_TIMER  = 1  (12h safety timer active)
    //   bits2:1 CHG_TIMER = 10 (12h)
    //   bit 0   JEITA_ISET= 0  (default)
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG07, 0xCC)) return false;

    // -------------------------------------------------------------------------
    // REG00 = 0x08
    //   EN_HIZ  = 0  (not in high-impedance)
    //   EN_ILIM = 0  (ILIM pin not used)
    //   IINLIM  = 001000 = 500mA (pre-detection floor)
    // IINLIM is auto-updated by D+/D- detection after VBUS plug-in.
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG00, 0x08)) return false;

    // -------------------------------------------------------------------------
    // REG02 = 0x3D
    //   CONV_RATE    = 0  (one-shot ADC — trigger manually each reading)
    //   BOOST_FREQ   = 1  (500kHz)
    //   ICO_EN       = 1  (input current optimizer enabled)
    //   HVDCP_EN     = 1  (high-voltage DCP handshake enabled)
    //   MAXC_EN      = 1  (MaxCharge handshake enabled)
    //   FORCE_DPDM   = 0
    //   AUTO_DPDM_EN = 1  (auto D+/D- detection on VBUS plug-in)
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG02, 0x3D)) return false;

    // -------------------------------------------------------------------------
    // REG03 = 0x1A
    //   BAT_LOADEN  = 0  (battery load disabled)
    //   WD_RST      = 0
    //   OTG_CONFIG  = 0  (OTG/boost disabled — device is USB peripheral)
    //   CHG_CONFIG  = 1  (charging enabled)
    //   SYS_MIN     = 101 = 3.5V
    //   Reserved    = 0
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG03, 0x1A)) return false;

    // -------------------------------------------------------------------------
    // REG04 = 0x17
    //   EN_PUMPX = 0
    //   ICHG     = 0010111 = N=23 → 23 x 64mA = 1472mA
    //
    // Fuse limit is 1.8A. 1472mA is the highest safe setting (N=24 = 1536mA
    // would exceed the fuse under combined system load).
    // Battery: 4400mAh. 1472mA ≈ 0.33C.
    // DO NOT increase without revisiting fuse ratings.
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG04, 0x17)) return false;

    // -------------------------------------------------------------------------
    // REG05 = 0x13
    //   IPRECHG = 0001 → 64 + 64 = 128mA
    //   ITERM   = 0011 → 64 + 128 + 64 = 256mA
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG05, 0x13)) return false;

    // -------------------------------------------------------------------------
    // REG06 = 0x5E
    //   VREG    = 010111 → 3840 + (23 x 16) = 4208mV = 4.208V
    //   BATLOWV = 1 → 3.0V threshold (pre-charge to fast-charge transition)
    //   VRECHG  = 0 → recharge threshold 100mV below VREG
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG06, 0x5E)) return false;

    // -------------------------------------------------------------------------
    // REG08 = 0x03
    //   BAT_COMP = 000 → 0Ω (IR compensation disabled)
    //   VCLAMP   = 000 → 0mV
    //   TREG     = 11  → 120°C thermal regulation threshold
    // -------------------------------------------------------------------------
    if (!bq25895_write(BQ_REG08, 0x03)) return false;

    // Flush latched POR faults from REG0C before normal polling begins
    bq25895_read(BQ_REG0C);  // Read 1: clears latched state
    bq25895_read(BQ_REG0C);  // Read 2: discard live state at init

    _initialized = true;
    Serial.println("[BQ25895] Init complete");
    return true;
}

// =============================================================================
// Status — call periodically from main loop (suggested: every 1–5s)
// =============================================================================

struct BQ25895Status {
    uint8_t vbus_stat;      // VBUS_STAT[2:0] — see BQ_VBUS_* constants
    uint8_t chrg_stat;      // CHRG_STAT[1:0] — see BQ_CHRG_* constants
    bool    power_good;     // PG_STAT — valid VBUS present
    bool    vsys_reg;       // VSYS_STAT — in VSYSMIN regulation (battery critically low)
};

bool bq25895_get_status(BQ25895Status &status) {
    int16_t reg0b = bq25895_read(BQ_REG0B);
    if (reg0b < 0) return false;

    status.vbus_stat  = ((uint8_t)reg0b & BQ_VBUS_STAT_MASK) >> BQ_VBUS_STAT_SHIFT;
    status.chrg_stat  = ((uint8_t)reg0b & BQ_CHRG_STAT_MASK) >> BQ_CHRG_STAT_SHIFT;
    status.power_good = ((uint8_t)reg0b & BQ_PG_STAT) != 0;
    status.vsys_reg   = ((uint8_t)reg0b & BQ_VSYS_STAT) != 0;

    return true;
}

// =============================================================================
// Fault Register — call every poll cycle, returns live fault byte
// Read-twice protocol: first read clears latched state, second is live.
// Returns -1 on I2C error, otherwise fault byte (0x00 = no faults).
// Check result against BQ_FAULT_* masks.
// =============================================================================

int16_t bq25895_read_faults() {
    bq25895_read(BQ_REG0C);        // Read 1: clears latched faults
    return bq25895_read(BQ_REG0C); // Read 2: live state
}

// =============================================================================
// ADC — trigger one-shot conversion and return all readings
// Blocks up to ~50ms waiting for conversion to complete (typically ~1ms).
// =============================================================================

struct BQ25895ADC {
    uint16_t vbat_mv;       // Battery voltage (mV)
    uint16_t vsys_mv;       // System voltage (mV)
    uint16_t vbus_mv;       // VBUS voltage (mV)
    uint16_t ichg_ma;       // Charge current (mA) — 0 if not in fast charge
    bool     vbus_good;     // VBUS_GD flag
};

bool bq25895_read_adc(BQ25895ADC &adc) {
    // Trigger one-shot conversion
    int16_t reg02 = bq25895_read(BQ_REG02);
    if (reg02 < 0) return false;
    if (!bq25895_write(BQ_REG02, (uint8_t)reg02 | 0x80)) return false;

    // Poll CONV_START until self-cleared
    uint32_t t_start = millis();
    while (true) {
        int16_t val = bq25895_read(BQ_REG02);
        if (val < 0) return false;
        if (((uint8_t)val & 0x80) == 0) break;
        if (millis() - t_start > 50) {
            Serial.println("[BQ25895] ADC timeout");
            return false;
        }
        delay(1);
    }

    int16_t r0d = bq25895_read(BQ_REG0D);
    int16_t r0f = bq25895_read(BQ_REG0F);
    int16_t r11 = bq25895_read(BQ_REG11);
    int16_t r12 = bq25895_read(BQ_REG12);

    if (r0d < 0 || r0f < 0 || r11 < 0 || r12 < 0) return false;

    adc.vbat_mv   = BQ_VBAT_MV((uint8_t)r0d);
    adc.vsys_mv   = BQ_VSYS_MV((uint8_t)r0f);
    adc.vbus_mv   = BQ_VBUS_MV((uint8_t)r11);
    adc.ichg_ma   = BQ_ICHG_MA((uint8_t)r12);
    adc.vbus_good = BQ_VBUS_GD((uint8_t)r11) != 0;

    return true;
}

// =============================================================================
// Configuration Helpers
// =============================================================================

// Charge current. Range: 0–5056mA, 64mA steps.
// HARD LIMIT: Do not set above 1472mA (N=23) — fuse rating is 1.8A.
bool bq25895_set_charge_current_ma(uint16_t ma) {
    if (ma > 1472) {
        Serial.println("[BQ25895] REFUSED: ICHG > 1472mA exceeds fuse limit");
        return false;
    }
    uint8_t reg = (ma / 64) & 0x7F;
    return bq25895_modify(BQ_REG04, 0x7F, reg);
}

// Charge voltage. Range: 3840–4608mV, 16mV steps.
bool bq25895_set_charge_voltage_mv(uint16_t mv) {
    if (mv < 3840) mv = 3840;
    if (mv > 4608) mv = 4608;
    uint8_t reg = ((mv - 3840) / 16) & 0x3F;
    return bq25895_modify(BQ_REG06, 0xFC, (uint8_t)(reg << 2));
}

// Input current limit. Range: 100–3250mA, 50mA steps.
// Note: auto-updated by D+/D- detection after VBUS plug-in.
bool bq25895_set_input_current_ma(uint16_t ma) {
    if (ma < 100) ma = 100;
    if (ma > 3250) ma = 3250;
    uint8_t reg = ((ma - 100) / 50) & 0x3F;
    return bq25895_modify(BQ_REG00, 0x3F, reg);
}

// Enable or disable charging
bool bq25895_set_charge_enable(bool enable) {
    return bq25895_modify(BQ_REG03, 0x10, enable ? 0x10 : 0x00);
}

// HIZ mode — disconnects input, system runs on battery only
bool bq25895_set_hiz(bool enable) {
    return bq25895_modify(BQ_REG00, 0x80, enable ? 0x80 : 0x00);
}

// Ship mode — disables BATFET, cuts battery output to minimize leakage
// Exit: plug in VBUS, or clear BATFET_DIS bit via I2C
bool bq25895_enter_ship_mode() {
    return bq25895_modify(BQ_REG09, 0x20, 0x20);
}

// =============================================================================
// Diagnostics
// =============================================================================

void bq25895_print_status() {
    if (!_initialized) {
        Serial.println("[BQ25895] Not initialized");
        return;
    }

    BQ25895Status status;
    BQ25895ADC adc;
    bool status_ok = bq25895_get_status(status);
    bool adc_ok    = bq25895_read_adc(adc);
    int16_t faults = bq25895_read_faults();

    Serial.println("=== BQ25895 ===");

    if (status_ok) {
        Serial.print("  Power Good:  "); Serial.println(status.power_good ? "YES" : "NO");
        Serial.print("  VSYS Reg:    "); Serial.println(status.vsys_reg   ? "YES (battery low)" : "NO");

        Serial.print("  VBUS Source: ");
        switch (status.vbus_stat) {
            case BQ_VBUS_NONE:     Serial.println("None"); break;
            case BQ_VBUS_SDP:      Serial.println("USB SDP (500mA)"); break;
            case BQ_VBUS_CDP:      Serial.println("USB CDP (1.5A)"); break;
            case BQ_VBUS_DCP:      Serial.println("USB DCP (3.25A)"); break;
            case BQ_VBUS_MAXCHG:   Serial.println("MaxCharge (1.5A)"); break;
            case BQ_VBUS_UNKNOWN:  Serial.println("Unknown (500mA)"); break;
            case BQ_VBUS_NONSTAND: Serial.println("Non-standard"); break;
            case BQ_VBUS_OTG:      Serial.println("OTG (unexpected)"); break;
        }

        Serial.print("  Charge:      ");
        switch (status.chrg_stat) {
            case BQ_CHRG_NONE:      Serial.println("Not charging"); break;
            case BQ_CHRG_PRECHARGE: Serial.println("Pre-charge"); break;
            case BQ_CHRG_FAST:      Serial.println("Fast charging"); break;
            case BQ_CHRG_DONE:      Serial.println("Done"); break;
        }
    }

    if (adc_ok) {
        Serial.print("  VBAT:        "); Serial.print(adc.vbat_mv); Serial.println(" mV");
        Serial.print("  VSYS:        "); Serial.print(adc.vsys_mv); Serial.println(" mV");
        Serial.print("  VBUS:        "); Serial.print(adc.vbus_mv); Serial.println(" mV");
        Serial.print("  ICHG:        "); Serial.print(adc.ichg_ma); Serial.println(" mA");
    }

    if (faults >= 0) {
        if ((uint8_t)faults == 0) {
            Serial.println("  Faults:      None");
        } else {
            Serial.print("  Faults:      0x"); Serial.println((uint8_t)faults, HEX);
            if ((uint8_t)faults & BQ_FAULT_WATCHDOG) Serial.println("    ! Watchdog expired — re-init required");
            if ((uint8_t)faults & BQ_FAULT_BOOST)    Serial.println("    ! Boost fault (OTG disabled — unexpected)");
            if ((uint8_t)faults & BQ_FAULT_BAT)      Serial.println("    ! Battery OVP");
            uint8_t cf = ((uint8_t)faults & BQ_FAULT_CHRG_MASK) >> BQ_FAULT_CHRG_SHIFT;
            if (cf == BQ_CHRG_FAULT_INPUT)   Serial.println("    ! Input fault");
            if (cf == BQ_CHRG_FAULT_THERMAL) Serial.println("    ! Thermal shutdown");
            if (cf == BQ_CHRG_FAULT_TIMER)   Serial.println("    ! Safety timer expired (12h)");
            uint8_t ntc = (uint8_t)faults & BQ_FAULT_NTC_MASK;
            if (ntc == BQ_NTC_COLD) Serial.println("    ! NTC: TS Cold");
            if (ntc == BQ_NTC_HOT)  Serial.println("    ! NTC: TS Hot");
        }
    }

    Serial.println("===============");
}
