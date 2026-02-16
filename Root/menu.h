#pragma once
#include "general.h"

// Menu data (defined inline so no multiple-definition issues)
inline const char* const menuItems[] = {
  "Music",
  "Albums",
  "Playlists",
  "Recent"
};

inline const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);

inline void drawMenuScreen(int selectedIndex) {
  tft.fillScreen(COLOR_BG);
  drawHeader();

  const int startY = HEADER_HEIGHT + 20;
  const int itemH  = 40;

  for (int i = 0; i < menuCount; i++) {
    int y = startY + i * itemH;

    if (i == selectedIndex) {
      tft.fillRoundRect(20, y - 6, 280, 32, 10, COLOR_PANEL);
      tft.fillCircle(35, y + 8, 6, COLOR_ORANGE);
      tft.setTextColor(COLOR_TEXT);
    } else {
      tft.setTextColor(COLOR_GREY);
    }

    tft.setTextSize(2);
    tft.setCursor(50, y);
    tft.print(menuItems[i]);
  }
}