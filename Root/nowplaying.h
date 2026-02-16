#pragma once
#include "general.h"

inline void songUIprogress() {
  float percentageSongComp = (songTotalSeconds > 0)
    ? (float)songProgressedSeconds / (float)songTotalSeconds
    : 0.0f;

  if (percentageSongComp < 0) percentageSongComp = 0;
  if (percentageSongComp > 1) percentageSongComp = 1;

  tft.fillRoundRect(PADDING * 14, 185, (int)(170 * percentageSongComp), 5, 10, COLOR_ORANGE);
}

inline void songTimeProgress() {
  int32_t minutes = songProgressedSeconds / 60;
  int32_t seconds = songProgressedSeconds % 60;

  int32_t remainingSeconds = songTotalSeconds - songProgressedSeconds;
  if (remainingSeconds < 0) remainingSeconds = 0;

  int32_t remainingMinutes = remainingSeconds / 60;
  int32_t remainingSecs    = remainingSeconds % 60;

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(1);

  tft.setCursor(PADDING * 6, 180);
  tft.printf("%d:%02d", (int)minutes, (int)seconds);

  tft.setCursor(PADDING * 50, 180);
  tft.printf("-%d:%02d", (int)remainingMinutes, (int)remainingSecs);
}

inline void playingUI() {
  drawSmallHeader();

  tft.fillRoundRect(PADDING * 3, 45, 290, 180, 10, COLOR_HEADER_BG);
  tft.fillRoundRect(PADDING * 6, 65, 100, 100, 10, COLOR_ORANGE);
  tft.fillRoundRect(PADDING * 14, 185, 170, 5, 10, COLOR_GREY);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 28, 70);
  tft.print(songName);

  tft.setTextColor(COLOR_GREY);
  tft.setTextSize(1);
  tft.setCursor(PADDING * 28, 95);
  tft.print(songArtist);

  EQIcon(150, 140);
  songTimeProgress();
  songUIprogress();
}