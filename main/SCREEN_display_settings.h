#pragma once
#include "general.h"

inline int displayBrightness = 128;  // 0-255

inline void drawDisplaySettingsBar() {
  const int barX = PADDING * 3;
  const int barY = SMALL_HEADER_HEIGHT + 80;
  const int barW = 280;
  const int barH = 16;

  tft.fillRoundRect(barX, barY, barW, barH, 4, COLOR_PANEL);
  int filled = (int)((float)displayBrightness / 255.0f * barW);
  tft.fillRoundRect(barX, barY, filled, barH, 4, COLOR_ORANGE);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(barX, barY + 22);
  tft.printf("Brightness: %d%%", (int)((float)displayBrightness / 255.0f * 100));
}

inline void drawDisplayScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Display");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 55);
  tft.print("Brightness");

  drawDisplaySettingsBar();

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, 200);
  tft.print("LEFT/RIGHT to adjust  BACK to exit");
}

inline void handleDisplayNav(NavEvent e) {
  if (e == NAV_RIGHT || e == NAV_UP) {
    displayBrightness = min(255, displayBrightness + 16);
    // TODO: analogWrite(TFT_BL, displayBrightness);
    drawDisplaySettingsBar();
  }
  else if (e == NAV_LEFT || e == NAV_DOWN) {
    displayBrightness = max(0, displayBrightness - 16);
    // TODO: analogWrite(TFT_BL, displayBrightness);
    drawDisplaySettingsBar();
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_SETTINGS);
  }
}
