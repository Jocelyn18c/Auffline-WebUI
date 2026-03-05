#include "buttons.h"
#include <Arduino.h>

static const int BTN_ADC_PIN = 39; // IO39 = I39_Button_Array

// expected voltages from schematic (approx)
static const float V_MODE = 2.88f;
static const float V_REC  = 2.27f;
static const float V_PLAY = 1.80f;
static const float V_SET  = 1.34f;
static const float V_UP   = 0.82f;
static const float V_DOWN = 0.30f;

static float adc_to_volts(int raw) {
  // 12-bit ADC -> 0..4095
  return (raw * 3.3f) / 4095.0f;
}

static ButtonEvent classify(float v) {
    // No press
    if (v > 3.10f) return ButtonEvent::None;
  
    // VOL+  (your BTN=1) around 2.87–2.88V
    if (v > 2.70f && v < 3.00f) return ButtonEvent::VolUp;
  
    // VOL-  (your BTN=2) around 2.17–2.19V
    if (v > 2.00f && v < 2.40f) return ButtonEvent::VolDown;
  
    // PLAY  (your BTN=5) around 0.67–0.71V
    if (v > 0.55f && v < 0.85f) return ButtonEvent::Play;
  
    // SET   (your BTN=6) around 0.17–0.19V
    if (v > 0.10f && v < 0.30f) return ButtonEvent::Set;
  
    return ButtonEvent::None;
  }
  

void buttons_init() {
  analogReadResolution(12);
  // for ESP32 Arduino: widen ADC input range closer to 3.3V
  analogSetPinAttenuation(BTN_ADC_PIN, ADC_11db);
}

ButtonEvent buttons_poll() {
  static uint32_t pressStartMs = 0;
  static ButtonEvent lastStable = ButtonEvent::None;
  static ButtonEvent lastSent   = ButtonEvent::None;
  static uint32_t stableSinceMs = 0;

  int raw = analogRead(BTN_ADC_PIN);
  float v = adc_to_volts(raw);
  ButtonEvent now = classify(v);

  static int lastRaw = -1;


  uint32_t t = millis();
  if (now != lastStable) {
    lastStable = now;
    stableSinceMs = t;
  }

  // when button first pressed
if (lastSent == ButtonEvent::None && lastStable != ButtonEvent::None) {
    lastSent = lastStable;
    pressStartMs = millis();
    return lastStable;    // short press (tentative)
  }
  
  // when held
  if (lastStable == ButtonEvent::Set &&
      lastSent == ButtonEvent::Set &&
      (millis() - pressStartMs) > 800) {
  
    lastSent = ButtonEvent::SetLong;
    return ButtonEvent::SetLong;
  }
  
  // reset on release
  if (lastStable == ButtonEvent::None) {
    lastSent = ButtonEvent::None;
  }

  return ButtonEvent::None;
}
