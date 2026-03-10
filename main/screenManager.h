#pragma once
#include "encoderConfig.h"
#include "general.h"

// =====================================================
// Screen Enum — single source of truth for all screens
// =====================================================
enum Screen : uint8_t {
  SCREEN_MENU = 0,
  SCREEN_SONGLIST,
  SCREEN_NOWPLAYING,
  SCREEN_SETTINGS,
  SCREEN_BLUETOOTH,
  SCREEN_WIFI,
  SCREEN_DISPLAY,
  SCREEN_AUDIO,
  SCREEN_ABOUT
};

// =====================================================
// Global App State
// =====================================================
inline Screen currentScreen = SCREEN_MENU;
inline bool   needsRedraw   = true;

// =====================================================
// Screen Change Helper
// =====================================================
inline void navigateTo(Screen s) {
  currentScreen = s;
  needsRedraw   = true;
}

// =====================================================
// Screen Modules
// Included here so Root.ino never has to touch them.
// Each header owns: its state, its draw functions,
// and its handleXxxNav(NavEvent) function.
// =====================================================
#include "menu.h"
#include "songlist.h"
#include "nowplaying.h"
#include "bluetooth.h"
#include "wifi.h"
#include "display_settings.h"
#include "audio_settings.h"
#include "about.h"
#include "settings.h"

// =====================================================
// Draw Router
// =====================================================
inline void drawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_MENU:       drawMenuScreen(menuSelectedIndex); break;
    case SCREEN_SONGLIST:   drawSongListScreen();              break;
    case SCREEN_NOWPLAYING: tft.fillScreen(COLOR_BG);
                            playingUI();                       break;
    case SCREEN_SETTINGS:   drawSettingsMenu();                break;
    case SCREEN_BLUETOOTH:  drawBluetoothScreen();             break;
    case SCREEN_WIFI:       drawWifiScreen();                  break;
    case SCREEN_DISPLAY:    drawDisplayScreen();               break;
    case SCREEN_AUDIO:      drawAudioScreen();                 break;
    case SCREEN_ABOUT:      drawAboutScreen();                 break;
  }
}

// =====================================================
// Nav Dispatcher
// =====================================================
inline void handleNavEvent(NavEvent e) {
  if (e == NAV_NONE) return;

  switch (currentScreen) {
    case SCREEN_MENU:       handleMenuNav(e);       break;
    case SCREEN_SONGLIST:   handleSongListNav(e);   break;
    case SCREEN_NOWPLAYING: handleNowPlayingNav(e); break;
    case SCREEN_SETTINGS:   handleSettingsNav(e);   break;
    case SCREEN_BLUETOOTH:  handleBluetoothNav(e);  break;
    case SCREEN_WIFI:       handleWifiNav(e);       break;
    case SCREEN_DISPLAY:    handleDisplayNav(e);    break;
    case SCREEN_AUDIO:      handleAudioNav(e);      break;
    case SCREEN_ABOUT:      handleAboutNav(e);      break;
  }
}
