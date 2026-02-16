#include "general.h"

// ===== Now Playing Screen Variables =====
int32_t songProgressedSeconds = 110;
int32_t songTotalSeconds = 180;
const char* songName = "Baby";
const char* songArtist = "Justin Chalometh";

#define PADDING 5  // Master X offset control

// ===== UI Functions =====
void drawUI(void) {
  tft.fillScreen(COLOR_BACKGROUND);
  playingUI();
}

void playingUI(void) {
  drawSmallHeader();

  tft.fillRoundRect(PADDING * 3, 45, 290, 180, 10, COLOR_HEADER_BG);
  tft.fillRoundRect(PADDING * 6, 65, 100, 100, 10, COLOR_ORANGE);
  tft.fillRoundRect(PADDING * 14, 185, 170, 5, 10, COLOR_GREY);

  // Song name
  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 28, 70);
  tft.print(songName);

  // Artist name
  tft.setTextColor(COLOR_GREY);
  tft.setTextSize(1);
  tft.setCursor(PADDING * 28, 95);
  tft.print(songArtist);

  EQIcon(150, 140);

  // Progressed time
  int32_t minutes = songProgressedSeconds / 60;
  int32_t seconds = songProgressedSeconds % 60;

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(1);
  tft.setCursor(PADDING * 6, 180);
  tft.printf("%d:%02d", (int)minutes, (int)seconds);

  // Remaining time
  int32_t remainingSeconds = songTotalSeconds - songProgressedSeconds;
  if (remainingSeconds < 0) {
    remainingSeconds = 0;
  }

  int32_t remainingMinutes = remainingSeconds / 60;
  int32_t remainingSecs = remainingSeconds % 60;

  tft.setCursor(PADDING * 50, 180);
  tft.printf("-%d:%02d", (int)remainingMinutes, (int)remainingSecs);

  songUIprogress();
}

void songUIprogress(void) {
  float percentageSongComp = (float)songProgressedSeconds / (float)songTotalSeconds;
  tft.fillRoundRect(
    PADDING * 14,
    185,
    (int)(170 * percentageSongComp),
    5,
    10,
    COLOR_ORANGE
  );
}
