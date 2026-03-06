// ===================== Root.ino =====================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ---- TFT pins (keep your wiring) ----
#define TFT_CS   5
#define TFT_DC   22
#define TFT_RST  17
#define TFT_SCK  18
#define TFT_MOSI 23

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ---- Navigation (encoder + 4 buttons) - MUST BE BEFORE OTHER UI MODULES ----
#include "encoderConfig.h"

// ---- UI modules ----
#include "general.h"
#include "menu.h"
#include "nowplaying.h"
#include "songlist.h"
#include "settings.h"

// =====================================================
// App State
// =====================================================
enum Screen : uint8_t {
  SCREEN_MENU = 0,
  SCREEN_SONGLIST,
  SCREEN_NOWPLAYING,
  SCREEN_SETTINGS,
  SCREEN_BLUETOOTH
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

    case SCREEN_SONGLIST:
      drawSongListScreen();
      break;

    case SCREEN_NOWPLAYING:
      tft.fillScreen(COLOR_BG);
      playingUI();
      break;

    case SCREEN_SETTINGS:
      drawSettingsMenu();
      break;

    case SCREEN_BLUETOOTH:
      drawBluetoothScreen();
      break;
  }
}

// =====================================================
// Navigation Handlers
// =====================================================
static void handleNavEvent(NavEvent e) {
  if (e == NAV_NONE) return;

  // ===== MAIN MENU =====
  if (currentScreen == SCREEN_MENU) {
    if (e == NAV_UP || e == NAV_LEFT) {
      selectedIndex--;
      if (selectedIndex < 0) selectedIndex = menuCount - 1;
      updateMenuSelection(selectedIndex); // Partial update instead of full redraw
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      selectedIndex++;
      if (selectedIndex >= menuCount) selectedIndex = 0;
      updateMenuSelection(selectedIndex); // Partial update instead of full redraw
    }
    else if (e == NAV_SELECT) {
      // Route based on selected menu item
      switch (selectedIndex) {
        case 0: // Music
          loadSongsFromSD(); // Load songs (currently uses placeholder data)
          currentScreen = SCREEN_SONGLIST;
          songListSelectedIndex = 0;
          songListScrollOffset = 0;
          needsRedraw = true;
          break;
        
        case 1: // Albums
          // TODO: Implement albums view
          break;
        
        case 2: // Playlists
          // TODO: Implement playlists view
          break;
        
        case 3: // Recent
          // TODO: Implement recent songs view
          break;
        
        case 4: // Settings
          currentScreen = SCREEN_SETTINGS;
          settingsSelectedIndex = 0;
          needsRedraw = true;
          break;
      }
    }
  }

  // ===== SONG LIST SCREEN =====
  else if (currentScreen == SCREEN_SONGLIST) {
    if (e == NAV_UP || e == NAV_LEFT || e == NAV_DOWN || e == NAV_RIGHT) {
      handleSongListNav(e);
      updateSongListSelection(); // Partial update instead of full redraw
    }
    else if (e == NAV_SELECT) {
      // Select song and go to now playing
      selectCurrentSong();
      currentScreen = SCREEN_NOWPLAYING;
      needsRedraw = true;
    }
    else if (e == NAV_BACK) {
      // Go back to main menu
      currentScreen = SCREEN_MENU;
      needsRedraw = true;
    }
  }

  // ===== NOW PLAYING SCREEN =====
  else if (currentScreen == SCREEN_NOWPLAYING) {
    if (e == NAV_BACK) {
      currentScreen = SCREEN_SONGLIST;
      needsRedraw = true;
    }
    // Optional: Add play/pause, seek controls here
    // if (e == NAV_SELECT) { togglePlayPause(); }
    // if (e == NAV_LEFT) { seekBackward(); }
    // if (e == NAV_RIGHT) { seekForward(); }
  }

  // ===== SETTINGS MENU =====
  else if (currentScreen == SCREEN_SETTINGS) {
    if (e == NAV_UP || e == NAV_LEFT || e == NAV_DOWN || e == NAV_RIGHT) {
      handleSettingsNav(e);
      updateSettingsMenuSelection(); // Partial update instead of full redraw
    }
    else if (e == NAV_SELECT) {
      // Route based on selected setting
      switch (settingsSelectedIndex) {
        case SETTINGS_BLUETOOTH:
          if (!btEnabled) {
            initBluetooth();
          }
          startBluetoothScan(); // Start scanning for devices
          currentScreen = SCREEN_BLUETOOTH;
          btSelectedIndex = 0;
          btScrollOffset = 0;
          needsRedraw = true;
          break;
        
        case SETTINGS_WIFI:
          // TODO: Implement Wi-Fi settings
          break;
        
        case SETTINGS_DISPLAY:
          // TODO: Implement display settings
          break;
        
        case SETTINGS_AUDIO:
          // TODO: Implement audio settings
          break;
        
        case SETTINGS_ABOUT:
          // TODO: Implement about screen
          break;
      }
    }
    else if (e == NAV_BACK) {
      currentScreen = SCREEN_MENU;
      needsRedraw = true;
    }
  }

  // ===== BLUETOOTH SCREEN =====
  else if (currentScreen == SCREEN_BLUETOOTH) {
    if (e == NAV_UP || e == NAV_LEFT || e == NAV_DOWN || e == NAV_RIGHT) {
      handleBluetoothNav(e);
      updateBluetoothSelection(); // Partial update instead of full redraw
    }
    else if (e == NAV_SELECT) {
      if (btDeviceCount > 0) {
        selectBluetoothDevice(); // Connect/disconnect
        // Redraw the affected item to show connection change
        int displayIndex = btSelectedIndex - btScrollOffset;
        if (displayIndex >= 0 && displayIndex < BT_DEVICES_PER_PAGE) {
          drawBluetoothDeviceItem(displayIndex, btSelectedIndex, true);
        }
      } else {
        // Re-scan if no devices
        startBluetoothScan();
        needsRedraw = true;
      }
    }
    else if (e == NAV_BACK) {
      currentScreen = SCREEN_SETTINGS;
      needsRedraw = true;
    }
  }
}

// =====================================================
// Arduino setup/loop
// =====================================================
void setup() {
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(3);

  navBegin();        // sets up encoder/buttons pins + debounce baselines
  
  // TODO: Initialize SD card here
  // #include <SD.h>
  // #define SD_CS_PIN 2  // Change to your SD card CS pin
  // if (!SD.begin(SD_CS_PIN)) {
  //   Serial.println("SD card initialization failed!");
  // }
  
  needsRedraw = true;
}

void loop() {
  NavEvent e = navPoll();   // reads encoder + buttons (debounced)
  handleNavEvent(e);

  if (needsRedraw) {
    drawCurrentScreen();
    needsRedraw = false;
  }
  
  // TODO: Update playback progress if playing
  // This should be done in a non-blocking way
  // if (isPlaying) {
  //   updatePlaybackProgress();
  //   if (needsProgressUpdate) {
  //     // Redraw only the progress bar area
  //     songUIprogress();
  //     songTimeProgress();
  //   }
  // }
}
