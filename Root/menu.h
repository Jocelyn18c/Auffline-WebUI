#pragma once
#include "general.h"

// Menu data (defined inline so no multiple-definition issues)
inline const char* const menuItems[] = {
  "Music",
  "Albums",
  "Playlists",
  "Recent",
  "Settings"
};

inline const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);
inline int lastMenuSelectedIndex = -1; // Track last selected for partial updates

// Draws and undraws menu items based on selection
inline void drawMenuItem(int index, bool selected) {
  const int startY = HEADER_HEIGHT + 20;
  const int itemH  = 40;
  int y = startY + index * itemH;

  if (selected) {
    //color the background of the selected item and draw the orange dot
    tft.fillRoundRect(20, y - 6, 280, 32, 10, COLOR_PANEL);
    tft.fillCircle(35, y + 8, 6, COLOR_ORANGE);
    tft.setTextColor(COLOR_TEXT);
  } else {

    // Clear the background of the unselected item and set text color to grey
    tft.fillRoundRect(20, y - 6, 280, 32, 10, COLOR_BG);
    tft.setTextColor(COLOR_GREY);
  }

  tft.setTextSize(2);
  tft.setCursor(50, y);
  tft.print(menuItems[index]);
}

// Full redraw (used on first draw or screen change)
inline void drawMenuScreen(int selectedIndex) {
  tft.fillScreen(COLOR_BG);
  drawHeader();

  const int startY = HEADER_HEIGHT + 20;
  const int itemH  = 40;

  for (int i = 0; i < menuCount; i++) {
    drawMenuItem(i, i == selectedIndex);
  }
  
  lastMenuSelectedIndex = selectedIndex;
}

// Partial update (only redraw changed items)
inline void updateMenuSelection(int newIndex) {
  if (newIndex == lastMenuSelectedIndex) return;
  
  // Deselect old item
  if (lastMenuSelectedIndex >= 0 && lastMenuSelectedIndex < menuCount) {
    drawMenuItem(lastMenuSelectedIndex, false);
  }
  
  // Select new item
  if (newIndex >= 0 && newIndex < menuCount) {
    drawMenuItem(newIndex, true);
  }
  
  lastMenuSelectedIndex = newIndex;
}
