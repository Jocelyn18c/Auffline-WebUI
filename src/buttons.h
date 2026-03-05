#pragma once
#include <stdint.h>

enum class ButtonEvent : uint8_t {
    None = 0,
    VolUp, // vol+
    VolDown, // vol-
    Play, // play pause
    Set, // next track
    SetLong     // previous track
  };

void buttons_init();
ButtonEvent buttons_poll();   // returns ONE event when a press is detected
