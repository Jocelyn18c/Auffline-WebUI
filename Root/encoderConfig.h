#pragma once
#include <Arduino.h>

enum NavEvent : uint8_t {
  NAV_NONE,
  NAV_LEFT,
  NAV_RIGHT,
  NAV_UP,
  NAV_DOWN,
  NAV_SELECT,
  NAV_BACK
};

// ===== Pins =====
constexpr uint8_t PIN_ENC_A  = 32;
constexpr uint8_t PIN_ENC_B  = 25;
constexpr uint8_t PIN_ENC_SW = 16;

constexpr uint8_t PIN_BTN_UP    = 19;//
constexpr uint8_t PIN_BTN_DOWN  = 4;    //
constexpr uint8_t PIN_BTN_BACK  = 16;
constexpr uint8_t PIN_BTN_MODE  = 21; // optional -> you choose mapping

// this controls the amount of ticks of the encoder to changein the UI menu.
constexpr uint8_t TICKS_PER_DETENT = 2;

// ===== Debounce =====
struct ButtonDebounce {
  int pin = -1;
  bool lastStable = HIGH;
  bool lastRead   = HIGH;
  uint32_t lastChangeMs = 0;
};

static const uint32_t NAV_DEBOUNCE_MS = 25;

static inline void navInitButton(ButtonDebounce &b, int pin) {
  b.pin = pin;
  b.lastStable = digitalRead(pin);
  b.lastRead = b.lastStable;
  b.lastChangeMs = millis();
}

static inline bool navFell(ButtonDebounce &b) {
  if (b.pin < 0) return false;

  bool r = digitalRead(b.pin);
  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChangeMs = millis();
  }
  if ((millis() - b.lastChangeMs) > NAV_DEBOUNCE_MS) {
    if (b.lastStable != b.lastRead) {
      b.lastStable = b.lastRead;
      if (b.lastStable == LOW) return true; // fell (pressed)
    }
  }
  return false;
}

// ===== Internal State =====
static ButtonDebounce btnEnc, btnUp, btnDown, btnBack, btnMode;

// Encoder state (interrupt-driven)
volatile int32_t encTicks = 0;
volatile uint8_t encPrev = 0;
static int32_t encAccum = 0;

// Gray code transition table for quadrature decoding
static const int8_t ENC_TABLE[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

// ISR for encoder (both A and B trigger this)
void IRAM_ATTR isrEncoderAB() {
  uint8_t a = (uint8_t)digitalRead(PIN_ENC_A);
  uint8_t b = (uint8_t)digitalRead(PIN_ENC_B);
  uint8_t curr = (a << 1) | b;

  uint8_t idx = (encPrev << 2) | curr;
  encTicks += ENC_TABLE[idx];
  encPrev = curr;
}

static inline void navBegin() {
  // Encoder pins
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  // Initialize encoder state
  encPrev = ((uint8_t)digitalRead(PIN_ENC_A) << 1) | (uint8_t)digitalRead(PIN_ENC_B);
  encTicks = 0;
  encAccum = 0;

  // Attach interrupts to both encoder pins for CHANGE
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isrEncoderAB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isrEncoderAB, CHANGE);

  // Button pins
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);

  navInitButton(btnEnc,  PIN_ENC_SW);
  navInitButton(btnUp,   PIN_BTN_UP);
  navInitButton(btnDown, PIN_BTN_DOWN);
  navInitButton(btnBack, PIN_BTN_BACK);
  navInitButton(btnMode, PIN_BTN_MODE);
}

static inline NavEvent navPoll() {
  // 1) Encoder rotation: accumulate ticks and emit events per detent
  int32_t d;
  noInterrupts();
  d = encTicks;
  encTicks = 0;
  interrupts();

  if (d != 0) {
    encAccum += d;

    // Apply exactly ONE step per detent threshold crossed
    if (encAccum >= TICKS_PER_DETENT) {
      encAccum -= TICKS_PER_DETENT;
      return NAV_RIGHT;
    }
    if (encAccum <= -TICKS_PER_DETENT) {
      encAccum += TICKS_PER_DETENT;
      return NAV_LEFT;
    }
  }

  // 2) Encoder press
  if (navFell(btnEnc)) return NAV_SELECT;

  // 3) Extra buttons
  if (navFell(btnUp))   return NAV_UP;
  if (navFell(btnDown)) return NAV_DOWN;
  if (navFell(btnBack)) return NAV_BACK;

  // 4) Optional button mapping
  if (navFell(btnMode)) return NAV_SELECT; // or NAV_BACK / NAV_MODE etc.

  return NAV_NONE;
}