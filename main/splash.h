#pragma once
#include "general.h"

// 2-second animated splash screen with progress bar
inline void showSplash() {
  const uint32_t SPLASH_MS  = 2000;
  const int      BAR_X      = 40;
  const int      BAR_Y      = 195;
  const int      BAR_W      = 240;
  const int      BAR_H      = 6;
  const int      BAR_RADIUS = 3;

  tft.fillScreen(COLOR_BG);

  // ---- Logo circle ----
  tft.fillCircle(160, 80, 36, COLOR_ORANGE);
  // Inner cutout for "ring" look

  // ---- App name ----
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(3);
  // Centre "auffline" (8 chars * 18px per char at size 3 = 144px wide)
  tft.setCursor(88, 130);
  tft.print("auffline");

  // ---- Tagline ----
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(110, 158);
  tft.print("offline music player");

  // ---- Progress bar track ----
  tft.fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_RADIUS, COLOR_PANEL);

  // ---- Animate fill over SPLASH_MS ----
  uint32_t start = millis();
  int lastFilled = 0;

  while (true) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= SPLASH_MS) break;

    int filled = (int)((float)elapsed / (float)SPLASH_MS * BAR_W);
    if (filled > lastFilled) {
      tft.fillRoundRect(BAR_X, BAR_Y, filled, BAR_H, BAR_RADIUS, COLOR_ORANGE);
      lastFilled = filled;
    }
  }

  // Complete the bar before exiting
  tft.fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_RADIUS, COLOR_ORANGE);
}
