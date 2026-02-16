// ===================== Root.ino =====================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

//test joy
// ---- TFT pins (keep your wiring) ----
#define TFT_CS   5
#define TFT_DC   22
#define TFT_RST  17
#define TFT_SCK  18
#define TFT_MOSI 23

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ---- UI modules ----
#include "general.h"
#include "menu.h"
#include "nowplaying.h"

// ---- Navigation (encoder + 4 buttons) ----
#include "encoderConfig.h"

// =====================================================
// App State
// =====================================================
enum Screen : uint8_t {
  SCREEN_MENU = 0,
  SCREEN_NOWPLAYING
};

Screen currentScreen = SCREEN_MENU;
bool needsRedraw = true;

int selectedIndex = 0;

// =====================================================
// Screen Draw Router
// =====================================================
static void drawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_MENU:
      drawMenuScreen(selectedIndex);
      break;

    case SCREEN_NOWPLAYING:
      tft.fillScreen(COLOR_BG);
      playingUI();
      break;
  }
}

// =====================================================
// Navigation Handlers
// =====================================================
static void handleNavEvent(NavEvent e) {
  if (e == NAV_NONE) return;

  if (currentScreen == SCREEN_MENU) {
    // Encoder rotation OR button presses can drive selection.
    if (e == NAV_UP || e == NAV_LEFT) {
      selectedIndex--;
      if (selectedIndex < 0) selectedIndex = menuCount - 1;
      needsRedraw = true;
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      selectedIndex++;
      if (selectedIndex >= menuCount) selectedIndex = 0;
      needsRedraw = true;
    }
    else if (e == NAV_SELECT) {
      // For now: selecting anything goes to Now Playing.
      // Later: route based on selectedIndex.
      currentScreen = SCREEN_NOWPLAYING;
      needsRedraw = true;
    }
    else if (e == NAV_BACK) {
      // Back in menu is usually no-op
    }
  }
  else {
    // On other screens
    if (e == NAV_BACK) {
      currentScreen = SCREEN_MENU;
      needsRedraw = true;
    }

    // Optional: you can use NAV_LEFT/NAV_RIGHT on nowplaying for seek later
    // Optional: NAV_SELECT as play/pause later
  }
}

// =====================================================
// Arduino setup/loop
// =====================================================
void setup() {
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1);

  navBegin();        // sets up encoder/buttons pins + debounce baselines
  needsRedraw = true;
}

void loop() {
  NavEvent e = navPoll();   // reads encoder + buttons (debounced)
  handleNavEvent(e);

  if (needsRedraw) {
    drawCurrentScreen();
    needsRedraw = false;
  }
}