#pragma once
#include "general.h"

// Forward declaration
enum NavEvent : uint8_t;

// ===== Song List Data Structure =====
struct Song {
  char filename[64];  // MP3 filename
  char title[32];     // Display title (extracted from filename or ID3 tag later)
  char artist[32];    // Artist name (extracted from ID3 tag later)
};

// Global song list (will be populated from SD card)
inline Song songList[50];  // Max 50 songs for now
inline int songCount = 0;
inline int songListSelectedIndex = 0;
inline int songListScrollOffset = 0;

// ===== Display Constants =====
const int SONGS_PER_PAGE = 5;  // How many songs visible at once
const int SONG_ITEM_HEIGHT = 35;

// Track last state for partial updates
inline int lastSongListSelectedIndex = -1;
inline int lastSongListScrollOffset = -1;

// ===== Helper Functions =====

// Initialize with placeholder data (will be replaced with SD card reading)
inline void initializeSongList() {
  // Template: This will be replaced with SD card reading functionality
  // For now, creating placeholder songs
  songCount = 8;  // Example count
  
  // Placeholder songs - replace this with SD card reading later
  strcpy(songList[0].filename, "song1.mp3");
  strcpy(songList[0].title, "Baby");
  strcpy(songList[0].artist, "Justin Bieber");
  
  strcpy(songList[1].filename, "song2.mp3");
  strcpy(songList[1].title, "Shape of You");
  strcpy(songList[1].artist, "Ed Sheeran");
  
  strcpy(songList[2].filename, "song3.mp3");
  strcpy(songList[2].title, "Blinding Lights");
  strcpy(songList[2].artist, "The Weeknd");
  
  strcpy(songList[3].filename, "song4.mp3");
  strcpy(songList[3].title, "Levitating");
  strcpy(songList[3].artist, "Dua Lipa");
  
  strcpy(songList[4].filename, "song5.mp3");
  strcpy(songList[4].title, "Starboy");
  strcpy(songList[4].artist, "The Weeknd");
  
  strcpy(songList[5].filename, "song6.mp3");
  strcpy(songList[5].title, "Dance Monkey");
  strcpy(songList[5].artist, "Tones and I");
  
  strcpy(songList[6].filename, "song7.mp3");
  strcpy(songList[6].title, "Sunflower");
  strcpy(songList[6].artist, "Post Malone");
  
  strcpy(songList[7].filename, "song8.mp3");
  strcpy(songList[7].title, "Circles");
  strcpy(songList[7].artist, "Post Malone");
}

// Function to load songs from SD card (TEMPLATE - implement later)
inline void loadSongsFromSD() {
  /*
   * TODO: Implement SD card reading functionality
   * 
   * Steps:
   * 1. Initialize SD card:
   *    - #include <SD.h>
   *    - SD.begin(SD_CS_PIN);
   * 
   * 2. Open root directory or /music folder:
   *    - File root = SD.open("/");
   * 
   * 3. Iterate through files:
   *    - while (File entry = root.openNextFile()) {
   *        if (!entry.isDirectory() && strstr(entry.name(), ".mp3")) {
   *          strcpy(songList[songCount].filename, entry.name());
   *          // Extract title from filename or read ID3 tags
   *          songCount++;
   *        }
   *      }
   * 
   * 4. Optional: Parse ID3 tags for proper title/artist
   *    - Use a library like "esp32-id3" or parse manually
   * 
   * For now, using initializeSongList() with placeholder data
   */
  
  initializeSongList();  // Remove this when implementing SD card reading
}

// Get the currently selected song
inline Song* getCurrentSong() {
  if (songCount == 0) return nullptr;
  return &songList[songListSelectedIndex];
}

// Scroll handling for large song lists
inline void scrollToSelection() {
  if (songListSelectedIndex < songListScrollOffset) {
    songListScrollOffset = songListSelectedIndex;
  }
  else if (songListSelectedIndex >= songListScrollOffset + SONGS_PER_PAGE) {
    songListScrollOffset = songListSelectedIndex - SONGS_PER_PAGE + 1;
  }
}

// ===== Drawing Helper Functions =====

// Draw a single song item
inline void drawSongItem(int displayIndex, int songIndex, bool selected) {
  const int listStartY = SMALL_HEADER_HEIGHT + 50;
  int y = listStartY + (displayIndex * SONG_ITEM_HEIGHT);
  
  // Draw/clear selection highlight
  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, SONG_ITEM_HEIGHT - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, SONG_ITEM_HEIGHT - 5, 8, COLOR_BG);
  }
  
  // Draw song title
  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);
  
  char displayTitle[20];
  strncpy(displayTitle, songList[songIndex].title, 19);
  displayTitle[19] = '\0';
  tft.print(displayTitle);
  
  // Draw artist (smaller)
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 5, y + 16);
  
  char displayArtist[25];
  strncpy(displayArtist, songList[songIndex].artist, 24);
  displayArtist[24] = '\0';
  tft.print(displayArtist);
}

// Update scroll indicator
inline void updateScrollIndicator() {
  if (songCount <= SONGS_PER_PAGE) return;
  
  const int listStartY = SMALL_HEADER_HEIGHT + 50;
  int scrollBarHeight = 100;
  int scrollBarY = listStartY;
  int scrollBarWidth = 4;
  int scrollBarX = 310;
  
  // Clear and redraw background
  tft.fillRoundRect(scrollBarX, scrollBarY, scrollBarWidth, scrollBarHeight, 2, COLOR_GREY);
  
  // Calculate thumb position
  float thumbRatio = (float)SONGS_PER_PAGE / (float)songCount;
  int thumbHeight = (int)(scrollBarHeight * thumbRatio);
  if (thumbHeight < 10) thumbHeight = 10;
  
  float scrollRatio = (float)songListScrollOffset / (float)(songCount - SONGS_PER_PAGE);
  int thumbY = scrollBarY + (int)((scrollBarHeight - thumbHeight) * scrollRatio);
  
  // Thumb
  tft.fillRoundRect(scrollBarX, thumbY, scrollBarWidth, thumbHeight, 2, COLOR_ORANGE);
}

// Partial update - only redraw what changed
inline void updateSongListSelection() {
  // Check if we scrolled
  bool scrolled = (songListScrollOffset != lastSongListScrollOffset);
  
  if (scrolled) {
    // Full redraw of list area only (not header)
    const int listStartY = SMALL_HEADER_HEIGHT + 50;
    tft.fillRect(0, listStartY, 320, 190, COLOR_BG);
    
    // Redraw all visible songs
    for (int i = 0; i < SONGS_PER_PAGE && (songListScrollOffset + i) < songCount; i++) {
      int songIndex = songListScrollOffset + i;
      drawSongItem(i, songIndex, songIndex == songListSelectedIndex);
    }
    
    updateScrollIndicator();
  } else {
    // Just selection changed - update only 2 items
    // Find display indices for old and new selection
    int oldDisplayIndex = lastSongListSelectedIndex - lastSongListScrollOffset;
    int newDisplayIndex = songListSelectedIndex - songListScrollOffset;
    
    // Deselect old if visible
    if (oldDisplayIndex >= 0 && oldDisplayIndex < SONGS_PER_PAGE && 
        lastSongListSelectedIndex >= 0 && lastSongListSelectedIndex < songCount) {
      drawSongItem(oldDisplayIndex, lastSongListSelectedIndex, false);
    }
    
    // Select new if visible
    if (newDisplayIndex >= 0 && newDisplayIndex < SONGS_PER_PAGE && 
        songListSelectedIndex >= 0 && songListSelectedIndex < songCount) {
      drawSongItem(newDisplayIndex, songListSelectedIndex, true);
    }
  }
  
  lastSongListSelectedIndex = songListSelectedIndex;
  lastSongListScrollOffset = songListScrollOffset;
}

// ===== Main Drawing Function =====
inline void drawSongListScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();
  
  // Draw title
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Music Library");
  
  // Draw song count
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  tft.printf("%d songs", songCount);
  
  if (songCount == 0) {
    // No songs found
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("No songs found");
    tft.setTextSize(1);
    tft.setCursor(50, 145);
    tft.print("Insert SD card with MP3s");
    return;
  }
  
  // Draw song list
  for (int i = 0; i < SONGS_PER_PAGE && (songListScrollOffset + i) < songCount; i++) {
    int songIndex = songListScrollOffset + i;
    drawSongItem(i, songIndex, songIndex == songListSelectedIndex);
  }
  
  // Draw scroll indicator if needed
  updateScrollIndicator();
  
  // Update tracking variables
  lastSongListSelectedIndex = songListSelectedIndex;
  lastSongListScrollOffset = songListScrollOffset;
}

// ===== Navigation Handlers =====
inline void handleSongListNav(NavEvent e) {
  if (songCount == 0) return;
  
  if (e == NAV_UP || e == NAV_LEFT) {
    songListSelectedIndex--;
    if (songListSelectedIndex < 0) {
      songListSelectedIndex = songCount - 1;
      songListScrollOffset = max(0, songCount - SONGS_PER_PAGE);
    } else {
      scrollToSelection();
    }
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    songListSelectedIndex++;
    if (songListSelectedIndex >= songCount) {
      songListSelectedIndex = 0;
      songListScrollOffset = 0;
    } else {
      scrollToSelection();
    }
  }
}

// ===== Song Selection Handler =====
inline void selectCurrentSong() {
  Song* selected = getCurrentSong();
  if (selected) {
    // Update the now playing globals
    songName = selected->title;
    songArtist = selected->artist;
    
    // TODO: Actually load and play the MP3 file
    // Example:
    // loadMP3File(selected->filename);
    // startPlayback();
    
    // Reset playback time
    songProgressedSeconds = 0;
    songTotalSeconds = 180;  // TODO: Get actual duration from MP3 file
  }
}
