#pragma once
#include "general.h"
#include <SD.h>
#include <SPI.h>

// =====================================================
// Albums Screen — two states:
//   ALBUM_STATE_LIST  → browse albums (subfolders in /music)
//   ALBUM_STATE_SONGS → browse songs inside a selected album
// Loose songs in /music (not in subfolders) are ignored here
// They still show up in the Song List screen
// =====================================================

// =====================================================
// Constants
// =====================================================
#define MUSIC_DIR         "/music"
#define MAX_ALBUMS        20
#define MAX_ALBUM_SONGS   50
#define ALBUMS_PER_PAGE   5
#define ALBUM_ITEM_H      35

// =====================================================
// State
// =====================================================
enum AlbumViewState : uint8_t {
  ALBUM_STATE_LIST = 0,
  ALBUM_STATE_SONGS
};

inline AlbumViewState albumViewState = ALBUM_STATE_LIST;

// --- Album list ---
struct AlbumEntry {
  char name[32];   // display name (subfolder name e.g. "Absofacto")
  char path[64];   // full path e.g. /music/Absofacto
};

inline AlbumEntry albums[MAX_ALBUMS];
inline int albumCount           = 0;
inline int albumSelectedIndex   = 0;
inline int albumScrollOffset    = 0;
inline int lastAlbumSelected    = -1;
inline int lastAlbumScroll      = -1;

// --- Songs inside selected album ---
struct AlbumSong {
  char path[64];   // e.g. /music/Absofacto/Dissolve.wav
  char title[32];  // display name e.g. "Dissolve"
};

inline AlbumSong albumSongs[MAX_ALBUM_SONGS];
inline int albumSongCount        = 0;
inline int albumSongSelected     = 0;
inline int albumSongScrollOffset = 0;
inline int lastAlbumSongSelected = -1;
inline int lastAlbumSongScroll   = -1;

inline char currentAlbumName[32] = "";  // name of open album

// =====================================================
// Helpers
// =====================================================

// Check if file is a supported audio file
inline bool isAlbumAudioFile(const char* name) {
  int len = strlen(name);
  if (len < 5) return false;
  const char* ext = name + len - 4;
  return (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0);
}

// Strip path and extension to get display title
// e.g. "/music/Absofacto/Dissolve.wav" -> "Dissolve"
inline void albumStemTitle(const char* path, char* dst, int maxLen) {
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;

  int len = strlen(base);
  int stemLen = len;
  if (len > 4) {
    const char* ext = base + len - 4;
    if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0) {
      stemLen = len - 4;
    }
  }
  if (stemLen > maxLen - 1) stemLen = maxLen - 1;
  strncpy(dst, base, stemLen);
  dst[stemLen] = '\0';
}

// =====================================================
// SD Helpers
// =====================================================

// Load all subfolders from /music into albums[]
inline void loadAlbums() {
  albumCount         = 0;
  albumSelectedIndex = 0;
  albumScrollOffset  = 0;
  lastAlbumSelected  = -1;
  lastAlbumScroll    = -1;

  if (!SD.exists(MUSIC_DIR)) {
    SD.mkdir(MUSIC_DIR);
    return;
  }

  File dir = SD.open(MUSIC_DIR);
  if (!dir) return;

  while (albumCount < MAX_ALBUMS) {
    File entry = dir.openNextFile();
    if (!entry) break;

    // Only interested in subfolders
    if (!entry.isDirectory()) { entry.close(); continue; }

    const char* name = entry.name();
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;

    // Skip hidden folders
    if (base[0] == '.') { entry.close(); continue; }

    strncpy(albums[albumCount].name, base, 31);
    albums[albumCount].name[31] = '\0';
    snprintf(albums[albumCount].path, 64, "%s/%s", MUSIC_DIR, base);
    albumCount++;
    entry.close();
  }

  dir.close();
}

// Load all audio files from a selected album subfolder
inline void loadAlbumSongs(const char* albumPath) {
  albumSongCount        = 0;
  albumSongSelected     = 0;
  albumSongScrollOffset = 0;
  lastAlbumSongSelected = -1;
  lastAlbumSongScroll   = -1;

  File dir = SD.open(albumPath);
  if (!dir) return;

  while (albumSongCount < MAX_ALBUM_SONGS) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (entry.isDirectory()) { entry.close(); continue; }

    const char* name = entry.name();
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;

    // Skip hidden files (Mac ._filename metadata)
    if (base[0] == '.') { entry.close(); continue; }

    // Only audio files
    if (!isAlbumAudioFile(base)) { entry.close(); continue; }

    snprintf(albumSongs[albumSongCount].path, 64, "%s/%s", albumPath, base);
    albumStemTitle(base, albumSongs[albumSongCount].title, 32);
    albumSongCount++;
    entry.close();
  }

  dir.close();
}

// =====================================================
// Scroll helpers
// =====================================================
inline void scrollToAlbumSelection() {
  if (albumSelectedIndex < albumScrollOffset)
    albumScrollOffset = albumSelectedIndex;
  else if (albumSelectedIndex >= albumScrollOffset + ALBUMS_PER_PAGE)
    albumScrollOffset = albumSelectedIndex - ALBUMS_PER_PAGE + 1;
}

inline void scrollToAlbumSongSelection() {
  if (albumSongSelected < albumSongScrollOffset)
    albumSongScrollOffset = albumSongSelected;
  else if (albumSongSelected >= albumSongScrollOffset + ALBUMS_PER_PAGE)
    albumSongScrollOffset = albumSongSelected - ALBUMS_PER_PAGE + 1;
}

// =====================================================
// Drawing — Album List
// =====================================================
inline void drawAlbumScrollIndicator() {
  if (albumCount <= ALBUMS_PER_PAGE) return;

  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int trackH = ALBUMS_PER_PAGE * ALBUM_ITEM_H;
  const int barX   = 310;
  const int barW   = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)ALBUMS_PER_PAGE / (float)albumCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)albumScrollOffset /
                      (float)(albumCount - ALBUMS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

inline void drawAlbumItem(int displayIndex, int itemIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  int y = startY + displayIndex * ALBUM_ITEM_H;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, ALBUM_ITEM_H - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, ALBUM_ITEM_H - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char display[20];
  strncpy(display, albums[itemIndex].name, 19);
  display[19] = '\0';
  tft.print(display);

  // ">" hint on right
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(285, y + 8);
  tft.print(">");
}

inline void drawAlbumListScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Albums");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d album%s", albumCount, albumCount == 1 ? "" : "s");

  if (albumCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(40, 120);
    tft.print("No albums found");
    tft.setTextSize(1);
    tft.setCursor(20, 145);
    tft.print("Add subfolders to /music on SD");
    return;
  }

  for (int i = 0; i < ALBUMS_PER_PAGE && (albumScrollOffset + i) < albumCount; i++) {
    int idx = albumScrollOffset + i;
    drawAlbumItem(i, idx, idx == albumSelectedIndex);
  }

  drawAlbumScrollIndicator();

  lastAlbumSelected = albumSelectedIndex;
  lastAlbumScroll   = albumScrollOffset;
}

inline void updateAlbumListSelection() {
  bool scrolled = (albumScrollOffset != lastAlbumScroll);

  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, ALBUMS_PER_PAGE * ALBUM_ITEM_H, COLOR_BG);

    for (int i = 0; i < ALBUMS_PER_PAGE && (albumScrollOffset + i) < albumCount; i++) {
      int idx = albumScrollOffset + i;
      drawAlbumItem(i, idx, idx == albumSelectedIndex);
    }
    drawAlbumScrollIndicator();
  } else {
    int oldD = lastAlbumSelected - lastAlbumScroll;
    int newD = albumSelectedIndex - albumScrollOffset;

    if (oldD >= 0 && oldD < ALBUMS_PER_PAGE &&
        lastAlbumSelected >= 0 && lastAlbumSelected < albumCount)
      drawAlbumItem(oldD, lastAlbumSelected, false);

    if (newD >= 0 && newD < ALBUMS_PER_PAGE &&
        albumSelectedIndex >= 0 && albumSelectedIndex < albumCount)
      drawAlbumItem(newD, albumSelectedIndex, true);
  }

  lastAlbumSelected = albumSelectedIndex;
  lastAlbumScroll   = albumScrollOffset;
}

// =====================================================
// Drawing — Songs Inside Album
// =====================================================
inline void drawAlbumSongScrollIndicator() {
  if (albumSongCount <= ALBUMS_PER_PAGE) return;

  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int trackH = ALBUMS_PER_PAGE * ALBUM_ITEM_H;
  const int barX   = 310;
  const int barW   = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)ALBUMS_PER_PAGE / (float)albumSongCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)albumSongScrollOffset /
                      (float)(albumSongCount - ALBUMS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

inline void drawAlbumSongItem(int displayIndex, int songIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  int y = startY + displayIndex * ALBUM_ITEM_H;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, ALBUM_ITEM_H - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, ALBUM_ITEM_H - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char display[20];
  strncpy(display, albumSongs[songIndex].title, 19);
  display[19] = '\0';
  tft.print(display);
}

inline void drawAlbumSongsScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  // Header shows album name
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  char headerName[20];
  strncpy(headerName, currentAlbumName, 19);
  headerName[19] = '\0';
  tft.print(headerName);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d song%s", albumSongCount, albumSongCount == 1 ? "" : "s");

  if (albumSongCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("Album empty");
    tft.setTextSize(1);
    tft.setCursor(30, 145);
    tft.print("Add audio files to this folder");
    return;
  }

  for (int i = 0; i < ALBUMS_PER_PAGE && (albumSongScrollOffset + i) < albumSongCount; i++) {
    int idx = albumSongScrollOffset + i;
    drawAlbumSongItem(i, idx, idx == albumSongSelected);
  }

  drawAlbumSongScrollIndicator();

  lastAlbumSongSelected = albumSongSelected;
  lastAlbumSongScroll   = albumSongScrollOffset;
}

inline void updateAlbumSongSelection() {
  bool scrolled = (albumSongScrollOffset != lastAlbumSongScroll);

  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, ALBUMS_PER_PAGE * ALBUM_ITEM_H, COLOR_BG);

    for (int i = 0; i < ALBUMS_PER_PAGE && (albumSongScrollOffset + i) < albumSongCount; i++) {
      int idx = albumSongScrollOffset + i;
      drawAlbumSongItem(i, idx, idx == albumSongSelected);
    }
    drawAlbumSongScrollIndicator();
  } else {
    int oldD = lastAlbumSongSelected - lastAlbumSongScroll;
    int newD = albumSongSelected - albumSongScrollOffset;

    if (oldD >= 0 && oldD < ALBUMS_PER_PAGE &&
        lastAlbumSongSelected >= 0 && lastAlbumSongSelected < albumSongCount)
      drawAlbumSongItem(oldD, lastAlbumSongSelected, false);

    if (newD >= 0 && newD < ALBUMS_PER_PAGE &&
        albumSongSelected >= 0 && albumSongSelected < albumSongCount)
      drawAlbumSongItem(newD, albumSongSelected, true);
  }

  lastAlbumSongSelected = albumSongSelected;
  lastAlbumSongScroll   = albumSongScrollOffset;
}

// =====================================================
// Main Draw + Nav Entry Points
// =====================================================
inline void drawAlbumScreen() {
  if (albumViewState == ALBUM_STATE_LIST) {
    loadAlbums();
    drawAlbumListScreen();
  } else {
    drawAlbumSongsScreen();
  }
}

inline void handleAlbumNav(NavEvent e) {

  // ===== ALBUM LIST =====
  if (albumViewState == ALBUM_STATE_LIST) {
    if (e == NAV_UP || e == NAV_LEFT) {
      if (albumSelectedIndex <= 0) return;
      albumSelectedIndex--;
      scrollToAlbumSelection();
      updateAlbumListSelection();
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      if (albumSelectedIndex >= albumCount - 1) return;
      albumSelectedIndex++;
      scrollToAlbumSelection();
      updateAlbumListSelection();
    }
    else if (e == NAV_SELECT) {
      if (albumCount == 0) return;
      strncpy(currentAlbumName, albums[albumSelectedIndex].name, 31);
      currentAlbumName[31] = '\0';
      loadAlbumSongs(albums[albumSelectedIndex].path);
      albumViewState = ALBUM_STATE_SONGS;
      needsRedraw = true;
    }
    else if (e == NAV_BACK) {
      navigateTo(SCREEN_MENU);
    }
  }

  // ===== SONGS INSIDE ALBUM =====
  else if (albumViewState == ALBUM_STATE_SONGS) {
    if (e == NAV_UP || e == NAV_LEFT) {
      if (albumSongSelected <= 0) return;
      albumSongSelected--;
      scrollToAlbumSongSelection();
      updateAlbumSongSelection();
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      if (albumSongSelected >= albumSongCount - 1) return;
      albumSongSelected++;
      scrollToAlbumSongSelection();
      updateAlbumSongSelection();
    }
    else if (e == NAV_SELECT) {
      if (albumSongCount == 0) return;
      songName   = albumSongs[albumSongSelected].title;
      songArtist = currentAlbumName;  // album name as artist
      songProgressedSeconds = 0;
      songTotalSeconds      = 180;
      navigateTo(SCREEN_NOWPLAYING);
    }
    else if (e == NAV_BACK) {
      albumViewState = ALBUM_STATE_LIST;
      needsRedraw = true;
    }
  }
}