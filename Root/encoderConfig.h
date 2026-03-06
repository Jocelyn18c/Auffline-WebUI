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
constexpr uint8_t PIN_ENC_B  = 33;
constexpr uint8_t PIN_ENC_SW = 25;

constexpr uint8_t PIN_BTN_UP    = 19;//
constexpr uint8_t PIN_BTN_DOWN  = 4;//
constexpr uint8_t PIN_BTN_BACK  = 16;
constexpr uint8_t PIN_BTN_MODE  = 21; // optional -> you choose mapping

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
static int lastEncA = HIGH;
static uint32_t lastEncStepMs = 0;
static const uint32_t ENC_STEP_GUARD_MS = 2; // small gate against bounce

static inline void navBegin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);

  navInitButton(btnEnc,  PIN_ENC_SW);
  navInitButton(btnUp,   PIN_BTN_UP);
  navInitButton(btnDown, PIN_BTN_DOWN);
  navInitButton(btnBack, PIN_BTN_BACK);
  navInitButton(btnMode, PIN_BTN_MODE);

  lastEncA = digitalRead(PIN_ENC_A);
  lastEncStepMs = millis();
}

static inline NavEvent navPoll() {
  // 1) Encoder rotation: rising edge on A
  int A = digitalRead(PIN_ENC_A);
  if (A != lastEncA) {
    lastEncA = A;
    if (A == HIGH) {
      uint32_t now = millis();
      if (now - lastEncStepMs >= ENC_STEP_GUARD_MS) {
        lastEncStepMs = now;
        int B = digitalRead(PIN_ENC_B);
        return (B == LOW) ? NAV_RIGHT : NAV_LEFT;
      }
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