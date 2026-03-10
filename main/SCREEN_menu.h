#pragma once
#include "general.h"

// Forward declarations for state owned by other screens
// (needed only to reset their state on navigation)
inline void loadSongsFromSD();
extern int songListSelectedIndex;
extern int songListScrollOffset;
extern int settingsSelectedIndex;

// =====================================================
// Menu Data
// =====================================================
inline const char* const menuItems[] = {
  "Music",
  "Albums",
  "Playlists",
  "Recent",
  "Settings"
};

inline const int menuCount           = sizeof(menuItems) / sizeof(menuItems[0]);
const int        MENU_ITEMS_PER_PAGE = 4;   // 4 items fit without cropping

inline int menuSelectedIndex     = 0;
inline int menuScrollOffset      = 0;       // which item is at the top of the visible window
inline int lastMenuSelectedIndex = -1;
inline int lastMenuScrollOffset  = -1;

// =====================================================
// Drawing
// =====================================================

// Clamp the scroll window so the selected item is always visible.
// Called by handleMenuNav after every selection change.
// This is what connects navigation events to the scroll bar position:
// the bar thumb recomputes from menuScrollOffset, which only changes here.
inline void scrollToMenuSelection() {
  if (menuSelectedIndex < menuScrollOffset)
    menuScrollOffset = menuSelectedIndex;
  else if (menuSelectedIndex >= menuScrollOffset + MENU_ITEMS_PER_PAGE)
    menuScrollOffset = menuSelectedIndex - MENU_ITEMS_PER_PAGE + 1;
}

// Draws the scroll bar thumb. Position is derived from menuScrollOffset,
// which is updated by navigation events — so the thumb always reflects
// the current nav position in the list.
inline void drawMenuScrollIndicator() {
  if (menuCount <= MENU_ITEMS_PER_PAGE) return;

  const int startY = HEADER_HEIGHT + 20;
  const int trackH = MENU_ITEMS_PER_PAGE * 40;
  const int barX   = 310;
  const int barW   = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)MENU_ITEMS_PER_PAGE / (float)menuCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)menuScrollOffset / (float)(menuCount - MENU_ITEMS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

// displayIndex = position on screen (0..MENU_ITEMS_PER_PAGE-1)
// itemIndex    = actual index into menuItems[]
inline void drawMenuItem(int displayIndex, int itemIndex, bool selected) {
  const int startY = HEADER_HEIGHT + 20;
  const int itemH  = 40;
  int y = startY + displayIndex * itemH;

  if (selected) {
    tft.fillRoundRect(20, y - 6, 286, 32, 10, COLOR_PANEL);
    tft.fillCircle(35, y + 8, 6, COLOR_ORANGE);
    tft.setTextColor(COLOR_TEXT);
  } else {
    tft.fillRoundRect(20, y - 6, 286, 32, 10, COLOR_BG);
    tft.setTextColor(COLOR_GREY);
  }

  tft.setTextSize(2);
  tft.setCursor(50, y);
  tft.print(menuItems[itemIndex]);
}

inline void drawMenuScreen(int selectedIndex) {
  tft.fillScreen(COLOR_BG);
  drawHeader();

  for (int i = 0; i < MENU_ITEMS_PER_PAGE && (menuScrollOffset + i) < menuCount; i++) {
    int idx = menuScrollOffset + i;
    drawMenuItem(i, idx, idx == selectedIndex);
  }

  drawMenuScrollIndicator();

  lastMenuSelectedIndex = selectedIndex;
  lastMenuScrollOffset  = menuScrollOffset;
}

// Partial update — redraws only what changed.
// On a scroll event: redraws all visible rows + scroll bar.
// On a selection-only change: redraws 2 rows (old deselect, new select).
inline void updateMenuSelection(int newIndex) {
  if (newIndex == lastMenuSelectedIndex && menuScrollOffset == lastMenuScrollOffset) return;

  bool scrolled = (menuScrollOffset != lastMenuScrollOffset);

  if (scrolled) {
    const int startY = HEADER_HEIGHT + 20;
    tft.fillRect(0, startY, 310, MENU_ITEMS_PER_PAGE * 40, COLOR_BG);

    for (int i = 0; i < MENU_ITEMS_PER_PAGE && (menuScrollOffset + i) < menuCount; i++) {
      int idx = menuScrollOffset + i;
      drawMenuItem(i, idx, idx == newIndex);
    }

    drawMenuScrollIndicator();
  } else {
    int oldD = lastMenuSelectedIndex - lastMenuScrollOffset;
    int newD = newIndex - menuScrollOffset;

    if (oldD >= 0 && oldD < MENU_ITEMS_PER_PAGE &&
        lastMenuSelectedIndex >= 0 && lastMenuSelectedIndex < menuCount)
      drawMenuItem(oldD, lastMenuSelectedIndex, false);

    if (newD >= 0 && newD < MENU_ITEMS_PER_PAGE &&
        newIndex >= 0 && newIndex < menuCount)
      drawMenuItem(newD, newIndex, true);
  }

  lastMenuSelectedIndex = newIndex;
  lastMenuScrollOffset  = menuScrollOffset;
}

// =====================================================
// Navigation Handler
// =====================================================
inline void handleMenuNav(NavEvent e) {
  if (e == NAV_UP || e == NAV_LEFT) {
    if (menuSelectedIndex <= 0) return;  // already at top, stop
    menuSelectedIndex--;
    scrollToMenuSelection();
    updateMenuSelection(menuSelectedIndex);
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    if (menuSelectedIndex >= menuCount - 1) return;  // already at bottom, stop
    menuSelectedIndex++;
    scrollToMenuSelection();
    updateMenuSelection(menuSelectedIndex);
  }
  else if (e == NAV_SELECT) {
    switch (menuSelectedIndex) {
      case 0: // Music
        loadSongsFromSD();
        songListSelectedIndex = 0;
        songListScrollOffset  = 0;
        navigateTo(SCREEN_SONGLIST);
        break;

      case 1: // Albums    — TODO
        break;

      case 2: // Playlists — TODO
        break;

      case 3: // Recent    — TODO
        break;

      case 4: // Settings
        settingsSelectedIndex = 0;
        navigateTo(SCREEN_SETTINGS);
        break;
    }
  }
}
