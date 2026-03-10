#pragma once
#include "general.h"

inline void drawAboutScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("About");

  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int lineH  = 22;

  tft.setTextSize(1);

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, startY);
  tft.print("Device:");
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(PADDING * 3 + 80, startY);
  tft.print("Auffline Player");

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, startY + lineH);
  tft.print("Firmware:");
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(PADDING * 3 + 80, startY + lineH);
  tft.print("v1.0.0");

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, startY + lineH * 2);
  tft.print("Platform:");
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(PADDING * 3 + 80, startY + lineH * 2);
  tft.print("ESP32");

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, startY + lineH * 3);
  tft.print("Display:");
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(PADDING * 3 + 80, startY + lineH * 3);
  tft.print("ILI9341 320x240");

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, 200);
  tft.print("BACK to exit");
}

inline void handleAboutNav(NavEvent e) {
  if (e == NAV_BACK) {
    navigateTo(SCREEN_SETTINGS);
  }
}
