#pragma once
#include "general.h"

// =====================================================
// Drawing
// =====================================================
inline void songUIprogress() {
  float pct = (songTotalSeconds > 0)
    ? (float)songProgressedSeconds / (float)songTotalSeconds
    : 0.0f;

  if (pct < 0) pct = 0;
  if (pct > 1) pct = 1;

  tft.fillRoundRect(PADDING * 14, 200, (int)(170 * pct), 5, 10, COLOR_ORANGE);
}

inline void songTimeProgress() {
  int32_t elapsed   = songProgressedSeconds;
  int32_t remaining = max((int32_t)0, songTotalSeconds - elapsed);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(1);

  tft.setCursor(PADDING * 6, 180);
  tft.printf("%d:%02d", (int)(elapsed / 60), (int)(elapsed % 60));

  tft.setCursor(PADDING * 50, 180);
  tft.printf("-%d:%02d", (int)(remaining / 60), (int)(remaining % 60));
}

inline void playingUI() {
  drawSmallHeader();

  // Main container
  tft.fillRoundRect(PADDING * 3, 45, 290, 180, 10, COLOR_HEADER_BG);

  // Album art placeholder
  tft.fillRoundRect(PADDING * 6, 65, 100, 100, 10, COLOR_ORANGE);

  // Progress bar background
  tft.fillRoundRect(PADDING * 14, 200, 170, 5, 10, COLOR_GREY);

  // Song name
  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 28, 70);
  tft.print(songName);

  // Artist name
  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(1);
  tft.setCursor(PADDING * 28, 95);
  tft.print(songArtist);

  EQIcon(150, 140);
  songTimeProgress();
  songUIprogress();
}

// =====================================================
// Navigation Handler
// =====================================================
inline void handleNowPlayingNav(NavEvent e) {
  if (e == NAV_BACK) {
    navigateTo(SCREEN_SONGLIST);
  }
  // TODO: playback controls
  // else if (e == NAV_SELECT) { togglePlayPause(); }
  // else if (e == NAV_LEFT)   { seekBackward(); }
  // else if (e == NAV_RIGHT)  { seekForward(); }
}
