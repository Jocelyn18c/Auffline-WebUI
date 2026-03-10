// ===================== Root.ino =====================
// Responsibilities:
//   1. Hardware init (TFT, SPI, encoder/buttons)
//   2. Main loop — delegates everything to screenManager
//
// Root knows about ONE thing: screenManager.h
// All screen logic, draw routing, and nav dispatch
// live in screenManager.h and the screen headers.
// =====================================================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ---- TFT pins ----
#define TFT_CS   5
#define TFT_DC   22
#define TFT_RST  21
#define TFT_SCK  14
#define TFT_MOSI 13

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ---- Input (must come before screenManager) ----
#include "encoderConfig.h"

// ---- Everything else is managed from here ----
#include "screenManager.h"
#include "SCREEN_splash.h"

// =====================================================
// Arduino Setup / Loop
// =====================================================
void setup() {
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1);

  navBegin();   // encoder + button pins, debounce baselines, ISRs
  showSplash(); // 2-second animated splash screen

  needsRedraw = true;
}

void loop() {
  handleNavEvent(navPoll());
  btLiveRefresh();

  if (needsRedraw) {
    drawCurrentScreen();
    needsRedraw = false;
  }

  // TODO: Non-blocking playback progress update
  // if (isPlaying && needsProgressUpdate) {
  //   songUIprogress();
  //   songTimeProgress();
  // }
}
