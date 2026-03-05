#pragma once
#include <Arduino.h>

#define SD_CS_PIN 5
#define SD_CMD_PIN 18
#define SD_DATA0_PIN 19

#define MAX_TRACKS 50

// info about songs on SD card 
struct Track {
    char filename[64];
};

bool sd_library_init();

int sd_library_scan();

const Track* sd_library_get_track(int index);

int sd_library_get_count();



