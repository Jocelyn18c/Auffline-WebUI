#include "file_transfer.h"
#include <MTP_Teensy.h>
#include <SD.h>

// =====================================================
// File Transfer — Teensy 4.1 USB MTP
// Works on Windows
// =====================================================

void file_transfer_init() {
  // Initialize the built-in SD card slot on Teensy 4.1
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("[MTP] SD card init failed!");
    return;
  }
  Serial.println("[MTP] SD card initialized!");

  // Create /music folder if it doesn't exist yet
  if (!SD.exists("/music")) {
    SD.mkdir("/music");
    Serial.println("[MTP] Created /music folder");
  }

  // Create /playlists folder if it doesn't exist yet
  if (!SD.exists("/playlists")) {
    SD.mkdir("/playlists");
    Serial.println("[MTP] Created /playlists folder");
  }

  // Start MTP and add the SD card as "Auffline"
  // This is what makes it show up in Finder / File Explorer
  MTP.begin();
  MTP.addFilesystem(SD, "Auffline");

  Serial.println("[MTP] Ready! Plug into computer to transfer files.");
}

void file_transfer_loop() {
  // Handle USB MTP communication
  // Must be called every loop so the computer can talk to the device
  MTP.loop();
}
