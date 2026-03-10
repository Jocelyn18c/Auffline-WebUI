#pragma once
#include "general.h"
#include <math.h>


// ===== Settings Menu Items =====
enum SettingsItem : uint8_t {
  SETTINGS_BLUETOOTH = 0,
  SETTINGS_WIFI,
  SETTINGS_DISPLAY,
  SETTINGS_AUDIO,
  SETTINGS_ABOUT,
  SETTINGS_COUNT
};

inline const char* const settingsItems[] = {
  "Bluetooth",
  "Wi-Fi",
  "Display",
  "Audio",
  "About"
};

const int SETTINGS_ITEMS_PER_PAGE = 4;

inline int settingsScrollOffset     = 0;
inline int lastSettingsScrollOffset = -1;

// Clamp scroll so selected item is always visible
inline void scrollToSettingsSelection() {
  if (settingsSelectedIndex < settingsScrollOffset)
    settingsScrollOffset = settingsSelectedIndex;
  else if (settingsSelectedIndex >= settingsScrollOffset + SETTINGS_ITEMS_PER_PAGE)
    settingsScrollOffset = settingsSelectedIndex - SETTINGS_ITEMS_PER_PAGE + 1;
}

// Scroll bar thumb on right
inline void drawSettingsScrollIndicator() {
  if (SETTINGS_COUNT <= SETTINGS_ITEMS_PER_PAGE) return;

  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int trackH = SETTINGS_ITEMS_PER_PAGE * 40;
  const int barX   = 310;
  const int barW   = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)SETTINGS_ITEMS_PER_PAGE / (float)SETTINGS_COUNT;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)settingsScrollOffset / (float)(SETTINGS_COUNT - SETTINGS_ITEMS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

// displayIndex = position on screen (0..SETTINGS_ITEMS_PER_PAGE-1)
// itemIndex    = actual index into settingsItems[]
inline void drawSettingsMenuItem(int displayIndex, int itemIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int itemH  = 40;
  int y = startY + displayIndex * itemH;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 6, 300, 32, 10, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 8, 6, COLOR_ORANGE);
    tft.setTextColor(COLOR_TEXT);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 6, 300, 32, 10, COLOR_BG);
    tft.setTextColor(COLOR_GREY);
  }

  tft.setTextSize(2);
  tft.setCursor(PADDING * 5, y);
  tft.print(settingsItems[itemIndex]);

  // Draw status indicator for Bluetooth
  if (itemIndex == SETTINGS_BLUETOOTH) {
    tft.setTextSize(1);
    tft.setCursor(240, y + 5);
    tft.setTextColor(btEnabled ? COLOR_ORANGE : COLOR_GREY);
    tft.print(btEnabled ? "ON" : "OFF");
  }
}

// Partial update — redraws only what changed
inline void updateSettingsMenuSelection() {
  if (settingsSelectedIndex == lastSettingsSelectedIndex &&
      settingsScrollOffset  == lastSettingsScrollOffset) return;

  bool scrolled = (settingsScrollOffset != lastSettingsScrollOffset);

  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, SETTINGS_ITEMS_PER_PAGE * 40, COLOR_BG);

    for (int i = 0; i < SETTINGS_ITEMS_PER_PAGE && (settingsScrollOffset + i) < SETTINGS_COUNT; i++) {
      int idx = settingsScrollOffset + i;
      drawSettingsMenuItem(i, idx, idx == settingsSelectedIndex);
    }

    drawSettingsScrollIndicator();
  } else {
    int oldD = lastSettingsSelectedIndex - lastSettingsScrollOffset;
    int newD = settingsSelectedIndex - settingsScrollOffset;

    if (oldD >= 0 && oldD < SETTINGS_ITEMS_PER_PAGE &&
        lastSettingsSelectedIndex >= 0 && lastSettingsSelectedIndex < SETTINGS_COUNT)
      drawSettingsMenuItem(oldD, lastSettingsSelectedIndex, false);

    if (newD >= 0 && newD < SETTINGS_ITEMS_PER_PAGE &&
        settingsSelectedIndex >= 0 && settingsSelectedIndex < SETTINGS_COUNT)
      drawSettingsMenuItem(newD, settingsSelectedIndex, true);
  }

  lastSettingsSelectedIndex = settingsSelectedIndex;
  lastSettingsScrollOffset  = settingsScrollOffset;
}

// Draw full settings menu
inline void drawSettingsMenu() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Settings");

  for (int i = 0; i < SETTINGS_ITEMS_PER_PAGE && (settingsScrollOffset + i) < SETTINGS_COUNT; i++) {
    int idx = settingsScrollOffset + i;
    drawSettingsMenuItem(i, idx, idx == settingsSelectedIndex);
  }

  drawSettingsScrollIndicator();

  lastSettingsSelectedIndex = settingsSelectedIndex;
  lastSettingsScrollOffset  = settingsScrollOffset;
}

// Handle navigation in settings menu
inline void handleSettingsNav(NavEvent e) {
  if (e == NAV_UP || e == NAV_LEFT) {
    if (settingsSelectedIndex <= 0) return;  // already at top, stop
    settingsSelectedIndex--;
    scrollToSettingsSelection();
    updateSettingsMenuSelection();
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    if (settingsSelectedIndex >= SETTINGS_COUNT - 1) return;  // already at bottom, stop
    settingsSelectedIndex++;
    scrollToSettingsSelection();
    updateSettingsMenuSelection();
  }
  else if (e == NAV_SELECT) {
    switch (settingsSelectedIndex) {
      case SETTINGS_BLUETOOTH:
        btSelectedIndex = 0;
        btScrollOffset = 0;
        navigateTo(SCREEN_BLUETOOTH);
        break;
      case SETTINGS_WIFI:
        navigateTo(SCREEN_WIFI);
        break;
      case SETTINGS_DISPLAY:
        navigateTo(SCREEN_DISPLAY);
        break;
      case SETTINGS_AUDIO:
        navigateTo(SCREEN_AUDIO);
        break;
      case SETTINGS_ABOUT:
        navigateTo(SCREEN_ABOUT);
        break;
    }
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_MENU);
  }
}
