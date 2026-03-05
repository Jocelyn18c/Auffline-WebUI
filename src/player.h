#pragma once
#include <Arduino.h>
#include "audio_out.h"

enum PlayerState : uint8_t {
  PLAYER_STOPPED = 0,
  PLAYER_PLAYING,
  PLAYER_PAUSED
};

// Init + optional periodic work
void player_init();
void player_loop();

// Transport
void player_play();
void player_pause();
void player_stop();
void player_toggle();
PlayerState player_get_state();

// Track navigation
void player_next();
void player_prev();
int  player_get_track_index();
int  player_get_track_count();

// Seek
void  player_seek_to(float seconds);       // absolute seek
void  player_seek_by(float delta_seconds); // relative seek
float player_get_position_sec();
float player_get_duration_sec();

// Volume
void player_volume_up();
void player_volume_down();
int  player_get_volume();   // optional for debug


void player_attach_audio_out(IAudioOut* out);

