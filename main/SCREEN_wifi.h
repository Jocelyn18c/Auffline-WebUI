#pragma once
#include "general.h"

inline bool wifiEnabled = false;
inline const char* wifiSSID = "Not connected";

inline void drawWifiScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Wi-Fi");

  // Status row
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 35);
  tft.print("Status:");
  tft.setTextColor(wifiEnabled ? COLOR_ORANGE : COLOR_GREY);
  tft.setCursor(PADDING * 3 + 60, SMALL_HEADER_HEIGHT + 35);
  tft.print(wifiEnabled ? "ON" : "OFF");

  // SSID row
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 55);
  tft.print("Network: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(wifiSSID);

  // Hint
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, 200);
  tft.print("Press SELECT to toggle  BACK to exit");
}

inline void handleWifiNav(NavEvent e) {
  if (e == NAV_SELECT) {
    wifiEnabled = !wifiEnabled;
    // TODO: call wifi enable/disable API
    drawWifiScreen();
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_SETTINGS);
  }
}
