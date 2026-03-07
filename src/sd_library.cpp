#include "sd_library.h"
#include <SD_MMC.h>

static Track g_tracks[MAX_TRACKS];
static int g_track_count = 0;

bool sd_library_init() {
  if (!SD_MMC.begin()) {
    Serial.println("Failed to initialize! :(");
    return false;
  }
  Serial.println("SD card initialized successfully! :)");
  return true;
}

int sd_library_scan() {
  g_track_count = 0;
  File root = SD_MMC.open("/music");
  
  if (!root) {
    Serial.println("Failed to open music folder! :(");
    return 0;
  }
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".wav") || name.endsWith(".WAV") || name.endsWith(".mp3") || name.endsWith(".MP3")) {
        strncpy(g_tracks[g_track_count].filename, entry.name(), 63);
        Serial.print("[SD] Found track: ");
        Serial.println(g_tracks[g_track_count].filename);
        g_track_count++;
        if (g_track_count >= MAX_TRACKS) break;
      }
    }
    entry.close();
  }
  root.close();
  Serial.printf("[SD] Total tracks: %d\n", g_track_count);
  return g_track_count;
}

const Track* sd_library_get_track(int index) {
  if (index < 0 || index >= g_track_count) return nullptr;
  return &g_tracks[index];
}

int sd_library_get_count() {
  return g_track_count;
}