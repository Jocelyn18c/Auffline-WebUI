#pragma once
#include <stdint.h>

// Forward-declare so this header can use `tft` without including the whole driver.
class Adafruit_ILI9341;
extern Adafruit_ILI9341 tft;

// ===== Standard 16-bit RGB565 Colors =====
#define COLOR_BACKGROUND  0x2945
#define COLOR_ORANGE      0xFC00
#define COLOR_HEADER_BG   0xB595
#define COLOR_BG          0x0841
#define COLOR_PANEL       0x2965
#define COLOR_TEXT        0xF7BE
#define COLOR_DIVIDER     0xFD20
#define COLOR_WHITE       0xFFFF
#define COLOR_GREY        0x5AEB

// ===== Layout Constants =====
#define HEADER_HEIGHT           45
#define SMALL_HEADER_HEIGHT     30
#define PADDING                 5

// ===== Now Playing Globals
inline int32_t songProgressedSeconds = 110;
inline int32_t songTotalSeconds      = 180;
inline const char* songName          = "Baby";
inline const char* songArtist        = "Justin Chalometh";

// ===== Drawing Functions (mark as inline so multiple includes are OK) =====
inline void drawSmallHeader() {
  tft.fillRect(0, 0, 320, SMALL_HEADER_HEIGHT, COLOR_HEADER_BG);
  tft.fillCircle(15, 15, 7, COLOR_ORANGE);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(30, 5);
  tft.print("auffline");

  tft.setCursor(280, 5);
  tft.print("87%");
}

inline void drawHeader() {
  tft.fillRect(0, 0, 320, HEADER_HEIGHT, COLOR_HEADER_BG);
  tft.fillCircle(35, 35, 20, COLOR_ORANGE);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(65, 15);
  tft.print("auffline");

  tft.setCursor(255, 15);
  tft.print("87%");
}

inline void EQIcon(int x, int y) {
  tft.fillRoundRect(x, y,      4, 30, 10, COLOR_BACKGROUND);
  tft.fillRoundRect(x + 10, y, 4, 30, 10, COLOR_BACKGROUND);
  tft.fillRoundRect(x + 20, y, 3.5, 30, 10, COLOR_BACKGROUND);
  tft.fillRoundRect(x + 30, y, 3, 30, 10, COLOR_BACKGROUND);

  tft.fillRoundRect(x - 3,      y + 5,  10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 10, y + 10, 10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 20, y + 15, 10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 30, y + 7,  10, 5, 2, COLOR_ORANGE);
}