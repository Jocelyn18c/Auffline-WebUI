#pragma once
#include "general.h"
#include <SD.h>
#include <SPI.h>

// =====================================================
// SD Card Pins (separate SPI bus from TFT)
// =====================================================
#define SD_MISO  19
#define SD_MOSI  23
#define SD_SCK   18
#define SD_CS    17

// =====================================================
// Song Data Structure
// =====================================================
struct Song {
  char filename[64];
  char title[32];
  char artist[32];
};

inline Song songList[50];
inline int  songCount             = 0;
inline int  songListSelectedIndex = 0;
inline int  songListScrollOffset  = 0;

const int SONGS_PER_PAGE   = 5;
const int SONG_ITEM_HEIGHT = 35;

inline int lastSongListSelectedIndex = -1;
inline int lastSongListScrollOffset  = -1;

// =====================================================
// SD Helpers
// =====================================================

// Returns true if name ends with .mp3 (case-insensitive)
inline bool isMp3(const char* name) {
  int len = strlen(name);
  if (len < 4) return false;
  const char* ext = name + len - 4;
  return (ext[0] == '.' &&
          (ext[1] == 'm' || ext[1] == 'M') &&
          (ext[2] == 'p' || ext[2] == 'P') &&
          (ext[3] == '3'));
}

// Copies filename stem (strips leading '/' and '.mp3') into dst[maxLen]
inline void stemFromPath(const char* path, char* dst, int maxLen) {
  // Skip leading '/'
  const char* base = path;
  const char* slash = strrchr(path, '/');
  if (slash) base = slash + 1;

  int len = strlen(base);
  int stemLen = (len >= 4) ? len - 4 : len;  // drop ".mp3"
  if (stemLen > maxLen - 1) stemLen = maxLen - 1;

  strncpy(dst, base, stemLen);
  dst[stemLen] = '\0';
}

// =====================================================
// Song Loading
// =====================================================
inline void loadSongsFromSD() {
  songCount             = 0;
  songListSelectedIndex = 0;
  songListScrollOffset  = 0;

  static SPIClass sdSPI(HSPI);
  static bool     sdSpiBegun = false;

  if (!sdSpiBegun) {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdSpiBegun = true;
  }

  // Re-attempt SD.begin() each call so hotplug works
  if (!SD.begin(SD_CS, sdSPI)) return;  // SD absent or failed — songCount stays 0

  File root = SD.open("/");
  if (!root) return;

  while (songCount < 50) {
    File entry = root.openNextFile();
    if (!entry) break;

    // Skip directories — only files are shown
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }

    const char* name = entry.name();

    // Skip macOS resource fork files (._filename) and any other dot-files
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;
    if (base[0] == '.') {
      entry.close();
      continue;
    }

    // Only .mp3 files
    if (!isMp3(name)) {
      entry.close();
      continue;
    }

    // filename (full path as returned by SD lib)
    strncpy(songList[songCount].filename, name, 63);
    songList[songCount].filename[63] = '\0';

    // title = filename without path prefix and without .mp3 extension
    stemFromPath(name, songList[songCount].title, 32);

    // artist — not parsed from ID3 yet
    strcpy(songList[songCount].artist, "Unknown");

    songCount++;
    entry.close();
  }

  root.close();
}

inline Song* getCurrentSong() {
  if (songCount == 0) return nullptr;
  return &songList[songListSelectedIndex];
}

inline void scrollToSelection() {
  if (songListSelectedIndex < songListScrollOffset)
    songListScrollOffset = songListSelectedIndex;
  else if (songListSelectedIndex >= songListScrollOffset + SONGS_PER_PAGE)
    songListScrollOffset = songListSelectedIndex - SONGS_PER_PAGE + 1;
}

// =====================================================
// Drawing
// =====================================================
inline void drawSongItem(int displayIndex, int songIndex, bool selected) {
  const int listStartY = SMALL_HEADER_HEIGHT + 50;
  int y = listStartY + (displayIndex * SONG_ITEM_HEIGHT);

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, SONG_ITEM_HEIGHT - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, SONG_ITEM_HEIGHT - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char displayTitle[20];
  strncpy(displayTitle, songList[songIndex].title, 19);
  displayTitle[19] = '\0';
  tft.print(displayTitle);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 5, y + 16);

  char displayArtist[25];
  strncpy(displayArtist, songList[songIndex].artist, 24);
  displayArtist[24] = '\0';
  tft.print(displayArtist);
}

inline void updateScrollIndicator() {
  if (songCount <= SONGS_PER_PAGE) return;

  const int listStartY      = SMALL_HEADER_HEIGHT + 50;
  const int scrollBarHeight = 100;
  const int scrollBarX      = 310;
  const int scrollBarWidth  = 4;

  tft.fillRoundRect(scrollBarX, listStartY, scrollBarWidth, scrollBarHeight, 2, COLOR_GREY);

  float thumbRatio  = (float)SONGS_PER_PAGE / (float)songCount;
  int   thumbHeight = max(10, (int)(scrollBarHeight * thumbRatio));
  float scrollRatio = (float)songListScrollOffset / (float)(songCount - SONGS_PER_PAGE);
  int   thumbY      = listStartY + (int)((scrollBarHeight - thumbHeight) * scrollRatio);

  tft.fillRoundRect(scrollBarX, thumbY, scrollBarWidth, thumbHeight, 2, COLOR_ORANGE);
}

inline void updateSongListSelection() {
  bool scrolled = (songListScrollOffset != lastSongListScrollOffset);

  if (scrolled) {
    const int listStartY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, listStartY, 320, 190, COLOR_BG);

    for (int i = 0; i < SONGS_PER_PAGE && (songListScrollOffset + i) < songCount; i++) {
      int idx = songListScrollOffset + i;
      drawSongItem(i, idx, idx == songListSelectedIndex);
    }
    updateScrollIndicator();
  } else {
    int oldD = lastSongListSelectedIndex - lastSongListScrollOffset;
    int newD = songListSelectedIndex     - songListScrollOffset;

    if (oldD >= 0 && oldD < SONGS_PER_PAGE && lastSongListSelectedIndex >= 0 && lastSongListSelectedIndex < songCount)
      drawSongItem(oldD, lastSongListSelectedIndex, false);

    if (newD >= 0 && newD < SONGS_PER_PAGE && songListSelectedIndex >= 0 && songListSelectedIndex < songCount)
      drawSongItem(newD, songListSelectedIndex, true);
  }

  lastSongListSelectedIndex = songListSelectedIndex;
  lastSongListScrollOffset  = songListScrollOffset;
}

inline void drawSongListScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Music Library");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d songs", songCount);

  if (songCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("No songs found");
    tft.setTextSize(1);
    tft.setCursor(50, 145);
    tft.print("Insert SD card with MP3s");
    return;
  }

  for (int i = 0; i < SONGS_PER_PAGE && (songListScrollOffset + i) < songCount; i++) {
    int idx = songListScrollOffset + i;
    drawSongItem(i, idx, idx == songListSelectedIndex);
  }

  updateScrollIndicator();

  lastSongListSelectedIndex = songListSelectedIndex;
  lastSongListScrollOffset  = songListScrollOffset;
}

// =====================================================
// Navigation Handler
// =====================================================
inline void selectCurrentSong() {
  Song* selected = getCurrentSong();
  if (!selected) return;

  songName   = selected->title;
  songArtist = selected->artist;

  songProgressedSeconds = 0;
  songTotalSeconds      = 180; // TODO: read actual duration from MP3 metadata
}

inline void handleSongListNav(NavEvent e) {
  if (e == NAV_UP || e == NAV_LEFT) {
    songListSelectedIndex--;
    if (songListSelectedIndex < 0) {
      songListSelectedIndex = songCount - 1;
      songListScrollOffset  = max(0, songCount - SONGS_PER_PAGE);
    } else {
      scrollToSelection();
    }
    if (songCount > 0) updateSongListSelection();
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    songListSelectedIndex++;
    if (songListSelectedIndex >= songCount) {
      songListSelectedIndex = 0;
      songListScrollOffset  = 0;
    } else {
      scrollToSelection();
    }
    if (songCount > 0) updateSongListSelection();
  }
  else if (e == NAV_SELECT) {
    selectCurrentSong();
    navigateTo(SCREEN_NOWPLAYING);
  }
  else if (e == NAV_BACK) {
    navigateTo(SCREEN_MENU);
  }
}
