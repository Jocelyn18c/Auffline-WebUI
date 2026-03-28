#pragma once
#include "general.h"
#include <SD.h>
#include <SPI.h>

// =====================================================//
// Playlist Screen — two states:
//   PLAYLIST_STATE_LIST  → browse playlists in /playlists
//   PLAYLIST_STATE_SONGS → browse songs inside a playlist
// =====================================================//

// =====================================================//
// Constants
// =====================================================//
#define PLAYLISTS_DIR       "/playlists"
#define MAX_PLAYLISTS       20
#define MAX_PLAYLIST_SONGS  50
#define PLAYLISTS_PER_PAGE  5
#define PLAYLIST_ITEM_H     35

// =====================================================//
// State
// =====================================================//
enum PlaylistViewState : uint8_t {
  PLAYLIST_STATE_LIST = 0,         // (looking at the list of playlists)
  PLAYLIST_STATE_SONGS             // (open a playlist and looking at the songs inside)
};

inline PlaylistViewState playlistViewState = PLAYLIST_STATE_LIST;

// =====================================================//
// --- Playlist list ---
// =====================================================//
struct PlaylistEntry {
  char name[32];      // display name (filename without .txt)
  char path[64];      // full path e.g. /playlists/workout.txts
};

inline PlaylistEntry playlists[MAX_PLAYLISTS];
inline int  playlistCount            = 0;
inline int  playlistSelectedIndex    = 0;
inline int  playlistScrollOffset     = 0;
inline int  lastPlaylistSelected     = -1;
inline int  lastPlaylistScrollOffset = -1;

// =====================================================//
// --- Songs inside selected playlist ---
// =====================================================//
struct PlaylistSong {
  char path[64];    // e.g. /music/song.wav
  char title[32];   // display name (filename without path + ext)
};

inline PlaylistSong  playlistSongs[MAX_PLAYLIST_SONGS];
inline int  playlistSongCount         = 0;
inline int  playlistSongSelected      = 0;
inline int  playlistSongScrollOffset  = 0;
inline int  lastPlaylistSongSelected  = -1;
inline int  lastPlaylistSongScroll    = -1;

inline char currentPlaylistName[32] = "";  // name of open playlist

// =====================================================//
// SD Helpers
// =====================================================//

// Strip path and extension to get a display title
// e.g. "/music/Sean Paul - She Doesnt Mind.wav" -> "Sean Paul - She Doesnt Mind"

inline void playlistStemTitle(const char* path, char* dst, int maxLen) {
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;

  int len = strlen(base);
  // strip .wav or .mp3 or .txt
  int stemLen = len;
  if (len > 4) {
    const char* ext = base + len - 4;
    if (strcasecmp(ext, ".wav") == 0 ||
        strcasecmp(ext, ".mp3") == 0 ||
        strcasecmp(ext, ".txt") == 0) {
      stemLen = len - 4;
    }
  }
  if (stemLen > maxLen - 1) stemLen = maxLen - 1;
  strncpy(dst, base, stemLen);
  dst[stemLen] = '\0';
}

// Load all .txt files from /playlists into playlists[]

inline void loadPlaylists() {
  playlistCount          = 0;
  playlistSelectedIndex  = 0;
  playlistScrollOffset   = 0;
  lastPlaylistSelected   = -1;
  lastPlaylistScrollOffset = -1;

  if (!SD.exists(PLAYLISTS_DIR)) {
    SD.mkdir(PLAYLISTS_DIR);
    return;
  }

  File dir = SD.open(PLAYLISTS_DIR);
  if (!dir) return;

  while (playlistCount < MAX_PLAYLISTS) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (entry.isDirectory()) { entry.close(); continue; }

    const char* name = entry.name();
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;

    // skip hidden files
    if (base[0] == '.') { entry.close(); continue; }

    // only .txt files
    int len = strlen(base);
    if (len < 5) { entry.close(); continue; }
    if (strcasecmp(base + len - 4, ".txt") != 0) { entry.close(); continue; }

    // store
    playlistStemTitle(base, playlists[playlistCount].name, 32);
    snprintf(playlists[playlistCount].path, 64, "%s/%s", PLAYLISTS_DIR, base);
    playlistCount++;
    entry.close();
  }

  dir.close();
}

// =====================================================//
// Load songs from a playlist .txt file into playlistSongs[]
// Each line in the file should be a path like /music/song.wav
// =====================================================//
inline void loadPlaylistSongs(const char* playlistPath) {
  playlistSongCount        = 0;
  playlistSongSelected     = 0;
  playlistSongScrollOffset = 0;
  lastPlaylistSongSelected = -1;
  lastPlaylistSongScroll   = -1;

  File f = SD.open(playlistPath);
  if (!f) return;

  while (f.available() && playlistSongCount < MAX_PLAYLIST_SONGS) {
    String line = f.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;
    if (line[0] == '#')     continue;  // allow comment lines

    strncpy(playlistSongs[playlistSongCount].path, line.c_str(), 63);
    playlistSongs[playlistSongCount].path[63] = '\0';

    playlistStemTitle(line.c_str(),
                      playlistSongs[playlistSongCount].title, 32);
    playlistSongCount++;
  }

  f.close();
}

// =====================================================//
// Scroll helpers
// =====================================================//
inline void scrollToPlaylistSelection() {
  if (playlistSelectedIndex < playlistScrollOffset)
    playlistScrollOffset = playlistSelectedIndex;
  else if (playlistSelectedIndex >= playlistScrollOffset + PLAYLISTS_PER_PAGE)
    playlistScrollOffset = playlistSelectedIndex - PLAYLISTS_PER_PAGE + 1;
}

inline void scrollToPlaylistSongSelection() {
  if (playlistSongSelected < playlistSongScrollOffset)
    playlistSongScrollOffset = playlistSongSelected;
  else if (playlistSongSelected >= playlistSongScrollOffset + PLAYLISTS_PER_PAGE)
    playlistSongScrollOffset = playlistSongSelected - PLAYLISTS_PER_PAGE + 1;
}

// =====================================================//
// Drawing — Playlist List
// =====================================================//
inline void drawPlaylistScrollIndicator() {
  if (playlistCount <= PLAYLISTS_PER_PAGE) return;

  const int startY  = SMALL_HEADER_HEIGHT + 50;
  const int trackH  = PLAYLISTS_PER_PAGE * PLAYLIST_ITEM_H;
  const int barX    = 310;
  const int barW    = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)PLAYLISTS_PER_PAGE / (float)playlistCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)playlistScrollOffset /
                      (float)(playlistCount - PLAYLISTS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

inline void drawPlaylistItem(int displayIndex, int itemIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  int y = startY + displayIndex * PLAYLIST_ITEM_H;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, PLAYLIST_ITEM_H - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, PLAYLIST_ITEM_H - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char display[20];
  strncpy(display, playlists[itemIndex].name, 19);
  display[19] = '\0';
  tft.print(display);

  // small "▶" hint on right
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(285, y + 8);
  tft.print(">");
}

inline void drawPlaylistListScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Playlists");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d playlist%s", playlistCount, playlistCount == 1 ? "" : "s");

  if (playlistCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(30, 120);
    tft.print("No playlists found");
    tft.setTextSize(1);
    tft.setCursor(30, 145);
    tft.print("Add .txt files to /playlists on SD");
    return;
  }

  for (int i = 0; i < PLAYLISTS_PER_PAGE && (playlistScrollOffset + i) < playlistCount; i++) {
    int idx = playlistScrollOffset + i;
    drawPlaylistItem(i, idx, idx == playlistSelectedIndex);
  }

  drawPlaylistScrollIndicator();

  lastPlaylistSelected     = playlistSelectedIndex;
  lastPlaylistScrollOffset = playlistScrollOffset;
}

inline void updatePlaylistListSelection() {
  bool scrolled = (playlistScrollOffset != lastPlaylistScrollOffset);

  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, PLAYLISTS_PER_PAGE * PLAYLIST_ITEM_H, COLOR_BG);

    for (int i = 0; i < PLAYLISTS_PER_PAGE && (playlistScrollOffset + i) < playlistCount; i++) {
      int idx = playlistScrollOffset + i;
      drawPlaylistItem(i, idx, idx == playlistSelectedIndex);
    }
    drawPlaylistScrollIndicator();
  } else {
    int oldD = lastPlaylistSelected - lastPlaylistScrollOffset;
    int newD = playlistSelectedIndex - playlistScrollOffset;

    if (oldD >= 0 && oldD < PLAYLISTS_PER_PAGE &&
        lastPlaylistSelected >= 0 && lastPlaylistSelected < playlistCount)
      drawPlaylistItem(oldD, lastPlaylistSelected, false);

    if (newD >= 0 && newD < PLAYLISTS_PER_PAGE &&
        playlistSelectedIndex >= 0 && playlistSelectedIndex < playlistCount)
      drawPlaylistItem(newD, playlistSelectedIndex, true);
  }

  lastPlaylistSelected     = playlistSelectedIndex;
  lastPlaylistScrollOffset = playlistScrollOffset;
}

// =====================================================//
// Drawing — Songs Inside Playlist
// =====================================================//
inline void drawPlaylistSongScrollIndicator() {
  if (playlistSongCount <= PLAYLISTS_PER_PAGE) return;

  const int startY  = SMALL_HEADER_HEIGHT + 50;
  const int trackH  = PLAYLISTS_PER_PAGE * PLAYLIST_ITEM_H;
  const int barX    = 310;
  const int barW    = 4;

  tft.fillRoundRect(barX, startY, barW, trackH, 2, COLOR_GREY);

  float thumbRatio  = (float)PLAYLISTS_PER_PAGE / (float)playlistSongCount;
  int   thumbH      = max(10, (int)(trackH * thumbRatio));
  float scrollRatio = (float)playlistSongScrollOffset /
                      (float)(playlistSongCount - PLAYLISTS_PER_PAGE);
  int   thumbY      = startY + (int)((trackH - thumbH) * scrollRatio);

  tft.fillRoundRect(barX, thumbY, barW, thumbH, 2, COLOR_ORANGE);
}

inline void drawPlaylistSongItem(int displayIndex, int songIndex, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  int y = startY + displayIndex * PLAYLIST_ITEM_H;

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, PLAYLIST_ITEM_H - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, PLAYLIST_ITEM_H - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char display[20];
  strncpy(display, playlistSongs[songIndex].title, 19);
  display[19] = '\0';
  tft.print(display);
}

inline void drawPlaylistSongsScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  // Header shows playlist name
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  char headerName[20];
  strncpy(headerName, currentPlaylistName, 19);
  headerName[19] = '\0';
  tft.print(headerName);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d song%s", playlistSongCount, playlistSongCount == 1 ? "" : "s");

  if (playlistSongCount == 0) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("Playlist empty");
    tft.setTextSize(1);
    tft.setCursor(50, 145);
    tft.print("Add song paths to the .txt file");
    return;
  }

  for (int i = 0; i < PLAYLISTS_PER_PAGE && (playlistSongScrollOffset + i) < playlistSongCount; i++) {
    int idx = playlistSongScrollOffset + i;
    drawPlaylistSongItem(i, idx, idx == playlistSongSelected);
  }

  drawPlaylistSongScrollIndicator();

  lastPlaylistSongSelected = playlistSongSelected;
  lastPlaylistSongScroll   = playlistSongScrollOffset;
}

inline void updatePlaylistSongSelection() {
  bool scrolled = (playlistSongScrollOffset != lastPlaylistSongScroll);

  if (scrolled) {
    const int startY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, startY, 310, PLAYLISTS_PER_PAGE * PLAYLIST_ITEM_H, COLOR_BG);

    for (int i = 0; i < PLAYLISTS_PER_PAGE && (playlistSongScrollOffset + i) < playlistSongCount; i++) {
      int idx = playlistSongScrollOffset + i;
      drawPlaylistSongItem(i, idx, idx == playlistSongSelected);
    }
    drawPlaylistSongScrollIndicator();
  } else {
    int oldD = lastPlaylistSongSelected - lastPlaylistSongScroll;
    int newD = playlistSongSelected - playlistSongScrollOffset;

    if (oldD >= 0 && oldD < PLAYLISTS_PER_PAGE &&
        lastPlaylistSongSelected >= 0 && lastPlaylistSongSelected < playlistSongCount)
      drawPlaylistSongItem(oldD, lastPlaylistSongSelected, false);

    if (newD >= 0 && newD < PLAYLISTS_PER_PAGE &&
        playlistSongSelected >= 0 && playlistSongSelected < playlistSongCount)
      drawPlaylistSongItem(newD, playlistSongSelected, true);
  }

  lastPlaylistSongSelected = playlistSongSelected;
  lastPlaylistSongScroll   = playlistSongScrollOffset;
}

// =====================================================//
// Main Draw + Nav Entry Points
// =====================================================//
inline void drawPlaylistScreen() {
  if (playlistViewState == PLAYLIST_STATE_LIST) {
    loadPlaylists();
    drawPlaylistListScreen();
  } else {
    drawPlaylistSongsScreen();
  }
}

inline void handlePlaylistNav(NavEvent e) {

  // ===== PLAYLIST LIST =====
  if (playlistViewState == PLAYLIST_STATE_LIST) {
    if (e == NAV_UP || e == NAV_LEFT) {
      if (playlistSelectedIndex <= 0) return;
      playlistSelectedIndex--;
      scrollToPlaylistSelection();
      updatePlaylistListSelection();
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      if (playlistSelectedIndex >= playlistCount - 1) return;
      playlistSelectedIndex++;
      scrollToPlaylistSelection();
      updatePlaylistListSelection();
    }
    else if (e == NAV_SELECT) {
      if (playlistCount == 0) return;
      // Load songs from selected playlist
      strncpy(currentPlaylistName, playlists[playlistSelectedIndex].name, 31);
      currentPlaylistName[31] = '\0';
      loadPlaylistSongs(playlists[playlistSelectedIndex].path);
      playlistViewState = PLAYLIST_STATE_SONGS;
      needsRedraw = true;
    }
    else if (e == NAV_BACK) {
      navigateTo(SCREEN_MENU);
    }
  }

  // ===== SONGS INSIDE PLAYLIST =====
  else if (playlistViewState == PLAYLIST_STATE_SONGS) {
    if (e == NAV_UP || e == NAV_LEFT) {
      if (playlistSongSelected <= 0) return;
      playlistSongSelected--;
      scrollToPlaylistSongSelection();
      updatePlaylistSongSelection();
    }
    else if (e == NAV_DOWN || e == NAV_RIGHT) {
      if (playlistSongSelected >= playlistSongCount - 1) return;
      playlistSongSelected++;
      scrollToPlaylistSongSelection();
      updatePlaylistSongSelection();
    }
    else if (e == NAV_SELECT) {
      if (playlistSongCount == 0) return;
      // Set now playing from playlist song
      songName   = playlistSongs[playlistSongSelected].title;
      songArtist = "Playlist";
      songProgressedSeconds = 0;
      songTotalSeconds      = 180;
      navigateTo(SCREEN_NOWPLAYING);
    }
    else if (e == NAV_BACK) {
      // Go back to playlist list
      playlistViewState = PLAYLIST_STATE_LIST;
      needsRedraw = true;
    }
  }
}
