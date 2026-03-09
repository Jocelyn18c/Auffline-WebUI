// =============================================================================
// TLV320AIC3204IRHBR — Audio Codec Driver
// =============================================================================
// Profile:     Full-duplex — DAC (headphone + line out) + ADC (MEMS mic)
// I2S:         Teensy master on I2S2 — AIC3204 is slave
//              Teensy drives MCLK2 (12.288 MHz), BCLK2, LRCLK2
// I2C:         Wire1 (I2C1), address 0x18
// Sample rate: 48kHz, 16-bit stereo
//
// Pin assignments (Teensy 4.1):
//   I2S2 pins      — MCLK2, BCLK2, LRCLK2, OUT2, IN2  (Teensy master)
//   Wire1 SCL/SDA  — I2C1 to AIC3204
//   CODEC_RESET    — HIGH = codec in reset via 2N7002 Q3 (active-low RST_N)
//
// WARNING: Do NOT use AudioOutputI2Sslave for this path.
//          AudioOutputI2Sslave is used on I2S1 for the BM83 only.
//          This is the opposite polarity — Teensy is master here.
//
// I2S1/BM83 objects are declared in bm83_teensy.cpp.
// Do not declare I2S1 objects here.
//
// NOTE: CODEC_RESET pin must be confirmed from top-level schematic.
//       Placeholder PIN definition below — verify before building.
// =============================================================================

#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>

// -----------------------------------------------------------------------------
// Pin Definitions
// -----------------------------------------------------------------------------
#define CODEC_RESET_PIN   31   // CODEC_RESET — HIGH = RST_N LOW via 2N7002 Q3

// -----------------------------------------------------------------------------
// I2C
// -----------------------------------------------------------------------------
#define AIC3204_I2C_ADDR  0x18   // Default address — SPI_SELECT tied to GNDD
                                  // VERIFY with I2C scan on first bring-up

// -----------------------------------------------------------------------------
// AIC3204 Register Map (Page 0 unless noted)
// Source: TLV320AIC3204 Application Reference Guide (SLAA557)
// -----------------------------------------------------------------------------
#define AIC_PAGE_SELECT       0x00  // Both pages — selects active page

// Page 0
#define AIC_P0_RESET          0x01
#define AIC_P0_CLK_MUX        0x04
#define AIC_P0_PLL_P_R        0x05
#define AIC_P0_NDAC           0x0B
#define AIC_P0_MDAC           0x0C
#define AIC_P0_DOSR_MSB       0x0D
#define AIC_P0_DOSR_LSB       0x0E
#define AIC_P0_NADC           0x12
#define AIC_P0_MADC           0x13
#define AIC_P0_AOSR           0x14
#define AIC_P0_IFACE1         0x1B
#define AIC_P0_IFACE2         0x1C
#define AIC_P0_DAC_SETUP1     0x3F
#define AIC_P0_DAC_SETUP2     0x40
#define AIC_P0_DAC_LVOL       0x41
#define AIC_P0_DAC_RVOL       0x42
#define AIC_P0_ADC_SETUP      0x51
#define AIC_P0_ADC_FGA        0x52

// Page 1
#define AIC_P1_PWR_CFG        0x01
#define AIC_P1_LDO_CFG        0x02
#define AIC_P1_MICBIAS        0x33
#define AIC_P1_HP_DRIVERS     0x09
#define AIC_P1_LO_DRIVERS     0x0A
#define AIC_P1_HPL_ROUTE      0x0C
#define AIC_P1_HPR_ROUTE      0x0D
#define AIC_P1_LOL_ROUTE      0x0E
#define AIC_P1_LOR_ROUTE      0x0F
#define AIC_P1_HPL_GAIN       0x10
#define AIC_P1_HPR_GAIN       0x11
#define AIC_P1_LOL_GAIN       0x12
#define AIC_P1_LOR_GAIN       0x13
#define AIC_P1_LMICPGA_P      0x34  // Left MICPGA positive terminal
#define AIC_P1_LMICPGA_N      0x36  // Left MICPGA negative terminal
#define AIC_P1_LMICPGA_GAIN   0x37
#define AIC_P1_RMICPGA_P      0x38
#define AIC_P1_RMICPGA_N      0x39
#define AIC_P1_RMICPGA_GAIN   0x3A

// -----------------------------------------------------------------------------
// I2S2 — Master output/input to AIC3204
// WARNING: Teensy is MASTER here. BM83 (I2S1) is the master on its own bus.
//          These are completely independent I2S peripherals.
// I2S1 objects are declared in bm83_teensy.cpp — do not redeclare here.
// -----------------------------------------------------------------------------
AudioOutputI2S2   i2s2_out;   // DAC path — Teensy master, AIC3204 slave
AudioInputI2S2    i2s2_in;    // ADC path — simultaneous full-duplex
// TODO: wire audio sources and sinks via AudioConnection

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------
static bool codec_ready = false;

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
bool aic3204_init();
bool aic3204_write(uint8_t reg, uint8_t val);
bool aic3204_select_page(uint8_t page);
void aic3204_set_dac_volume(float db_left, float db_right);
void aic3204_set_headphone_gain(float db_left, float db_right);
void aic3204_set_lineout_gain(float db_left, float db_right);
void aic3204_set_mic_gain(float db);
void aic3204_mute_dac(bool mute);
bool aic3204_scan_i2c();

// =============================================================================
// Setup & Loop
// =============================================================================

void setup() {
    Serial.begin(115200);
    Wire1.begin();
    Wire1.setClock(400000);  // 400kHz I2C — AIC3204 supports fast mode
    AudioMemory(16);

    // I2C scan — verify address before init
    if (!aic3204_scan_i2c()) {
        Serial.println("[AIC3204] ERROR: Device not found at 0x18 — check I2C1 wiring and power");
        return;
    }

    if (!aic3204_init()) {
        Serial.println("[AIC3204] ERROR: Init failed");
        return;
    }

    Serial.println("[AIC3204] Ready");
    codec_ready = true;
}

void loop() {
    // TODO: audio processing
}

// =============================================================================
// I2C Helpers
// =============================================================================

bool aic3204_write(uint8_t reg, uint8_t val) {
    Wire1.beginTransmission(AIC3204_I2C_ADDR);
    Wire1.write(reg);
    Wire1.write(val);
    uint8_t result = Wire1.endTransmission();
    if (result != 0) {
        Serial.print("[AIC3204] I2C write failed: reg=0x");
        Serial.print(reg, HEX);
        Serial.print(" val=0x");
        Serial.print(val, HEX);
        Serial.print(" err=");
        Serial.println(result);
        return false;
    }
    return true;
}

bool aic3204_select_page(uint8_t page) {
    return aic3204_write(AIC_PAGE_SELECT, page);
}

bool aic3204_scan_i2c() {
    Wire1.beginTransmission(AIC3204_I2C_ADDR);
    uint8_t result = Wire1.endTransmission();
    if (result == 0) {
        Serial.println("[AIC3204] I2C scan: 0x18 found");
        return true;
    }
    // Scan full range and report what is actually present
    Serial.println("[AIC3204] I2C scan: 0x18 not found. Devices found:");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire1.beginTransmission(addr);
        if (Wire1.endTransmission() == 0) {
            Serial.print("  0x");
            Serial.println(addr, HEX);
        }
    }
    return false;
}

// =============================================================================
// Initialization
// =============================================================================

bool aic3204_init() {
    // Step 1: Assert reset
    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, HIGH);  // MOSFET on → RST_N LOW
    delay(10);
    digitalWrite(CODEC_RESET_PIN, LOW);   // MOSFET off → RST_N HIGH via pull-up
    delay(2);                             // Datasheet: >1ms settling after reset release

    // Step 2: Software reset (belt-and-suspenders after hardware reset)
    if (!aic3204_select_page(0)) return false;
    if (!aic3204_write(AIC_P0_RESET, 0x01)) return false;
    delay(1);  // Self-clearing, but give it a cycle

    // -------------------------------------------------------------------------
    // Page 0 — Clock configuration
    // Assumes Teensy MCLK2 = 12.288 MHz (Teensy Audio library: 256 × 48kHz)
    // NDAC=1, MDAC=2, DOSR=128 → DAC_FS = 12.288M / 1 / 2 / 128 = 48kHz ✓
    // NADC=1, MADC=2, AOSR=128 → ADC_FS = 48kHz ✓
    // PLL disabled — clocking directly from MCLK2
    // -------------------------------------------------------------------------
    if (!aic3204_select_page(0)) return false;

    if (!aic3204_write(AIC_P0_CLK_MUX,  0x00)) return false;  // MCLK → codec clk, PLL off
    if (!aic3204_write(AIC_P0_PLL_P_R,  0x11)) return false;  // P=1, R=1 (PLL not used)
    if (!aic3204_write(AIC_P0_NDAC,     0x81)) return false;  // NDAC=1, powered
    if (!aic3204_write(AIC_P0_MDAC,     0x82)) return false;  // MDAC=2, powered
    if (!aic3204_write(AIC_P0_DOSR_MSB, 0x00)) return false;  // DOSR MSB
    if (!aic3204_write(AIC_P0_DOSR_LSB, 0x80)) return false;  // DOSR=128
    if (!aic3204_write(AIC_P0_NADC,     0x81)) return false;  // NADC=1, powered
    if (!aic3204_write(AIC_P0_MADC,     0x82)) return false;  // MADC=2, powered
    if (!aic3204_write(AIC_P0_AOSR,     0x80)) return false;  // AOSR=128

    // I2S interface — slave, 16-bit, I2S format
    if (!aic3204_write(AIC_P0_IFACE1,   0x00)) return false;  // I2S, 16-bit, slave, no BCLK invert
    if (!aic3204_write(AIC_P0_IFACE2,   0x00)) return false;  // Data offset = 0

    // DAC data path — left=I2S_L, right=I2S_R, both powered
    if (!aic3204_write(AIC_P0_DAC_SETUP1, 0xD6)) return false;
    if (!aic3204_write(AIC_P0_DAC_SETUP2, 0x00)) return false;  // Unmuted
    if (!aic3204_write(AIC_P0_DAC_LVOL,   0x00)) return false;  // 0dB
    if (!aic3204_write(AIC_P0_DAC_RVOL,   0x00)) return false;  // 0dB

    // -------------------------------------------------------------------------
    // Page 1 — Analog routing
    // -------------------------------------------------------------------------
    if (!aic3204_select_page(1)) return false;

    // Power config — use external AVDD (SYS_3V3), disable internal LDO
    if (!aic3204_write(AIC_P1_PWR_CFG, 0x08)) return false;
    if (!aic3204_write(AIC_P1_LDO_CFG, 0x01)) return false;

    // MICBIAS = AVDD (~3.3V) — within ICS-40720 VDD range (1.8–3.6V)
    if (!aic3204_write(AIC_P1_MICBIAS, 0x40)) return false;

    // Power headphone and line out drivers
    // Headphone: HPL + HPR powered, 100ms power-on ramp (avoids pop)
    if (!aic3204_write(AIC_P1_HP_DRIVERS, 0x3C)) return false;
    if (!aic3204_write(AIC_P1_LO_DRIVERS, 0x0F)) return false;

    // Route DAC_L → HPL, DAC_R → HPR, DAC_L → LOL, DAC_R → LOR
    if (!aic3204_write(AIC_P1_HPL_ROUTE, 0x08)) return false;
    if (!aic3204_write(AIC_P1_HPR_ROUTE, 0x08)) return false;
    if (!aic3204_write(AIC_P1_LOL_ROUTE, 0x08)) return false;
    if (!aic3204_write(AIC_P1_LOR_ROUTE, 0x08)) return false;

    // Output gains — 0dB starting point
    if (!aic3204_write(AIC_P1_HPL_GAIN, 0x00)) return false;
    if (!aic3204_write(AIC_P1_HPR_GAIN, 0x00)) return false;
    if (!aic3204_write(AIC_P1_LOL_GAIN, 0x00)) return false;
    if (!aic3204_write(AIC_P1_LOR_GAIN, 0x00)) return false;

    // ADC input routing — ICS-40720 differential on IN3_L/IN3_R
    // MIC+ (OUTPUT+) → R47 100Ω → IN3_L → Left MICPGA+
    // MIC- (OUTPUT-) → R46 100Ω → IN3_R → Left MICPGA- (differential)
    // Mono mic → route same signal to both ADC channels
    if (!aic3204_write(AIC_P1_LMICPGA_P,    0x30)) return false;  // IN3_L, 10kΩ
    if (!aic3204_write(AIC_P1_LMICPGA_N,    0x30)) return false;  // IN3_R/CM, 10kΩ
    if (!aic3204_write(AIC_P1_LMICPGA_GAIN, 0x04)) return false;  // 6dB — adjust during bring-up
    if (!aic3204_write(AIC_P1_RMICPGA_P,    0x30)) return false;
    if (!aic3204_write(AIC_P1_RMICPGA_N,    0x30)) return false;
    if (!aic3204_write(AIC_P1_RMICPGA_GAIN, 0x04)) return false;  // 6dB

    // -------------------------------------------------------------------------
    // Page 0 — ADC power on
    // -------------------------------------------------------------------------
    if (!aic3204_select_page(0)) return false;
    if (!aic3204_write(AIC_P0_ADC_SETUP, 0xC0)) return false;  // Left + right ADC powered
    if (!aic3204_write(AIC_P0_ADC_FGA,   0x00)) return false;  // Fine gain = 0dB, unmuted

    return true;
}

// =============================================================================
// Runtime Control
// =============================================================================

// DAC digital volume. Range: 0dB to -63.5dB in 0.5dB steps.
// val = 0x00 (0dB) to 0x7F (-63.5dB). 0x80 = muted.
void aic3204_set_dac_volume(float db_left, float db_right) {
    aic3204_select_page(0);
    uint8_t l = (uint8_t)constrain(-db_left * 2.0f, 0, 127);
    uint8_t r = (uint8_t)constrain(-db_right * 2.0f, 0, 127);
    aic3204_write(AIC_P0_DAC_LVOL, l);
    aic3204_write(AIC_P0_DAC_RVOL, r);
}

// Headphone analog gain. Range: 0dB to -6dB in 1dB steps (Page 1 R16/R17).
// Positive values not supported for HP amp — use DAC volume for boost.
void aic3204_set_headphone_gain(float db_left, float db_right) {
    aic3204_select_page(1);
    uint8_t l = (uint8_t)constrain(-db_left, 0, 6);
    uint8_t r = (uint8_t)constrain(-db_right, 0, 6);
    aic3204_write(AIC_P1_HPL_GAIN, l);
    aic3204_write(AIC_P1_HPR_GAIN, r);
}

// Line out analog gain. Range: 0dB to -6dB (Page 1 R18/R19).
void aic3204_set_lineout_gain(float db_left, float db_right) {
    aic3204_select_page(1);
    uint8_t l = (uint8_t)constrain(-db_left, 0, 6);
    uint8_t r = (uint8_t)constrain(-db_right, 0, 6);
    aic3204_write(AIC_P1_LOL_GAIN, l);
    aic3204_write(AIC_P1_LOR_GAIN, r);
}

// MICPGA gain in dB. Range: 0–47.5dB in 0.5dB steps.
// val = dB * 2 (e.g. 6dB = 0x0C, 20dB = 0x28)
void aic3204_set_mic_gain(float db) {
    aic3204_select_page(1);
    uint8_t val = (uint8_t)constrain(db * 2.0f, 0, 95);
    aic3204_write(AIC_P1_LMICPGA_GAIN, val);
    aic3204_write(AIC_P1_RMICPGA_GAIN, val);
}

// DAC soft mute — use before power-down to avoid output pop
void aic3204_mute_dac(bool mute) {
    aic3204_select_page(0);
    aic3204_write(AIC_P0_DAC_SETUP2, mute ? 0x0C : 0x00);
}
