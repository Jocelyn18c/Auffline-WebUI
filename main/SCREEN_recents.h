#pragma once
#include "general.h"
#include <SD.h>

// =====================================================
// Recents Screen
// =====================================================
#define MAX_RECENT_SONGS  20
#define RECENTS_PER_PAGE  5
#define RECENT_ITEM_H     35

struct RecentSong {
  char path[72];
  char title[32];
};

inline RecentSong recentSongs[MAX_RECENT_SONGS];
inline int recentCount            = 0;
inline int recentSelectedIndex    = 0;
inline int recentScrollOffset     = 0;
inline int lastRecentSelected     = -1;
inline int lastRecentScroll       = -1;

// Strip path and extension to get display title
inline void recentStemTitle(const char* path, char* dst, int maxLen) {
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;
  int len = strlen(base);
  int stemLen = len;
  if (len > 4) {
    const char* ext = base + len - 4;
    if (strcasecmp(ext, ".wav") == 0 ||
        strcasecmp(ext, ".mp3") == 0) {
      stemLen = len - 4;
    }
  }
  if (stemLen > maxLen - 1) stemLen = maxLen - 1;
  strncpy(dst, base, stemLen);
  dst[stemLen] = '\0';
}

inline void loadRecents() {
  recentCount         = 0;
  recentSelectedIndex = 0;
  recentScrollOffset  = 0;
  lastRecentSelected  = -1;
  lastRecentScroll    = -1;

  File f = SD.open("/recents.txt", FILE_READ);
  if (!f) return;

  while (f.available() && recentCount < MAX_RECENT_SONGS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    strncpy(recentSongs[recentCount].path, line.c_str(), 71);
    recentSongs[recentCount].path[71] = '\0';

    recentStemTitle(line.c_str(), recentSongs[recentCount].title, 32);
    recentCount++;
  }
  f.close();
}

inline void scrollToRecentSelection() {
  if (recentSelectedIndex < recentScrollOffset)
    recentScrollOffset = recentSelectedIndex;
  else if (recentSelectedIndex >= recentScrollOffset + RECENTS_PER_PAGE)
    recentScrollOffset = recentSelectedIndex - RECENTS_PER_PAGE + 1;
}

inline void drawRecentScrollIndicator() {
  if (recentCount <= RECENTS_PER_PAGE) return;
  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int trackH = RECENTS_PER_PAGE * RECENT_ITEM_H;
  const int barX   = 310;
  const int barW   = 4;
  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);
  float thumbRatio  = (float)RECENTS_PER_PAGE / (float)recentCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)recentScrollOffset / (float)(recentCount - RECENTS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);
  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

inline void drawRecentItem(int displayIndex, int itemIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  int y = startY + displayIndex * RECENT_ITEM_H;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, RECENT_ITEM_H - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
    tft.setTextColor(COLOR_TEXT);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, RECENT_ITEM_H - 5, 8, COLOR_BG);
    tft.setTextColor(COLOR_GREY);
  }

  tft.setTextSize(2);
  tft.setCursor(PADDING * 5, y);
  char display[20];
  strncpy(display, recentSongs[itemIndex].title, 19);
  display[19] = '\0';
  tft.print(display);
}

inline void drawRecentsScreen() {
  loadRecents();
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Recently Played");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d song%s", recentCount, recentCount == 1 ? "" : "s");

  if (recentCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("No recent songs");
    tft.setTextSize(1);
    tft.setCursor(50, 145);
    tft.print("Play a song to see it here");
    return;
  }

  for (int i = 0; i < RECENTS_PER_PAGE && (recentScrollOffset + i) < recentCount; i++) {
    int idx = recentScrollOffset + i;
    drawRecentItem(i, idx, idx == recentSelectedIndex);
  }

  drawRecentScrollIndicator();
  lastRecentSelected = recentSelectedIndex;
  lastRecentScroll   = recentScrollOffset;
}

inline void updateRecentSelection() {
  bool scrolled = (recentScrollOffset != lastRecentScroll);
  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, RECENTS_PER_PAGE * RECENT_ITEM_H, COLOR_BG);
    for (int i = 0; i < RECENTS_PER_PAGE && (recentScrollOffset + i) < recentCount; i++) {
      int idx = recentScrollOffset + i;
      drawRecentItem(i, idx, idx == recentSelectedIndex);
    }
    drawRecentScrollIndicator();
  } else {
    int oldD = lastRecentSelected - lastRecentScroll;
    int newD = recentSelectedIndex - recentScrollOffset;
    if (oldD >= 0 && oldD < RECENTS_PER_PAGE &&
        lastRecentSelected >= 0 && lastRecentSelected < recentCount)
      drawRecentItem(oldD, lastRecentSelected, false);
    if (newD >= 0 && newD < RECENTS_PER_PAGE &&
        recentSelectedIndex >= 0 && recentSelectedIndex < recentCount)
      drawRecentItem(newD, recentSelectedIndex, true);
  }
  lastRecentSelected = recentSelectedIndex;
  lastRecentScroll   = recentScrollOffset;
}

inline void handleRecentsNav(NavEvent e) {
  if (e == NAV_UP || e == NAV_LEFT) {
    if (recentSelectedIndex <= 0) return;
    recentSelectedIndex--;
    scrollToRecentSelection();
    updateRecentSelection();
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    if (recentSelectedIndex >= recentCount - 1) return;
    recentSelectedIndex++;
    scrollToRecentSelection();
    updateRecentSelection();
  }
  else if (e == NAV_SELECT) {
    if (recentCount == 0) return;
    songName   = recentSongs[recentSelectedIndex].title;
    songArtist = "Recent";
    songProgressedSeconds = 0;
    songTotalSeconds      = 180;
    navigateTo(SCREEN_NOWPLAYING);
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_MENU);
  }
}