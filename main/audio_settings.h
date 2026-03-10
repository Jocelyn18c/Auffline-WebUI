#pragma once
#include "general.h"

inline int audioVolume = 75;  // 0-100

inline void drawAudioVolumeBar() {
  const int barX = PADDING * 3;
  const int barY = SMALL_HEADER_HEIGHT + 80;
  const int barW = 280;
  const int barH = 16;

  tft.fillRoundRect(barX, barY, barW, barH, 4, COLOR_PANEL);
  int filled = (int)((float)audioVolume / 100.0f * barW);
  tft.fillRoundRect(barX, barY, filled, barH, 4, COLOR_ORANGE);

  tft.fillRect(barX, barY + 22, 120, 10, COLOR_BG);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(barX, barY + 22);
  tft.printf("Volume: %d%%", audioVolume);
}

inline void drawAudioScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Audio");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 55);
  tft.print("Volume");

  drawAudioVolumeBar();

  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, 200);
  tft.print("LEFT/RIGHT to adjust  BACK to exit");
}

inline void handleAudioNav(NavEvent e) {
  if (e == NAV_RIGHT || e == NAV_UP) {
    audioVolume = min(100, audioVolume + 5);
    // TODO: set hardware volume
    drawAudioVolumeBar();
  }
  else if (e == NAV_LEFT || e == NAV_DOWN) {
    audioVolume = max(0, audioVolume - 5);
    // TODO: set hardware volume
    drawAudioVolumeBar();
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_SETTINGS);
  }
}
