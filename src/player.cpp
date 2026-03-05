#include "player.h"
#include "audio_out.h"
#include "bt_a2dp.h"

static int g_volume = 70;   // 0–100, start reasonable
static IAudioOut* g_out = nullptr;
// extern volatile int g_pcm_volume; // for safety, can delete later


void player_attach_audio_out(IAudioOut* out) {
  g_out = out;
}

// ------------------------
// Minimal internal state
// ------------------------
static volatile PlayerState g_state = PLAYER_STOPPED;

// For now: fake playlist (replace later with SD scan)
static int g_track_index = 0;
static int g_track_count = 1;     // TODO: update later
static float g_duration_sec = 120.0f; // TODO: real duration from WAV
static float g_position_sec = 0.0f;

static inline float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

// ------------------------
// API
// ------------------------
void player_init() {
  g_state = PLAYER_STOPPED;
  g_track_index = 0;
  g_track_count = 1;
  g_duration_sec = 120.0f;
  g_position_sec = 0.0f;
}

void player_loop() {
  // Later: update position, handle end-of-track, etc.
}

void player_play() {
    bt_user_paused = false;
    g_state = PLAYER_PLAYING;
    if (g_out) g_out->play();
  }
  
  void player_pause() {
    bt_user_paused = true;
    g_state = PLAYER_PAUSED;
    if (g_out) g_out->pause();
  }
  
  void player_stop() {
    g_state = PLAYER_STOPPED;
    g_position_sec = 0.0f;
    if (g_out) g_out->stop();
  }

  void player_toggle() {
    if (g_state == PLAYER_PLAYING) player_pause();
    else player_play();
  }
  

PlayerState player_get_state() {
  return g_state;
}

void player_volume_up() {
    //if (g_pcm_volume < 80) g_pcm_volume += 5;   // CLAMP HERE
    //Serial.printf("[VOL] %d\n", g_pcm_volume);
  }
  
  void player_volume_down() {
    //if (g_pcm_volume > 0) g_pcm_volume -= 5;
    //Serial.printf("[VOL] %d\n", g_pcm_volume);
  }
  
  
  int player_get_volume() {
    return g_volume;
  }
  

void player_next() {
  if (g_track_count <= 0) return;
  g_track_index = (g_track_index + 1) % g_track_count;
  g_position_sec = 0.0f;

  // Optional behavior choice:
  // - keep playing if currently playing
  // - or stop on track change
  // Here: keep same play/pause state.
}

void player_prev() {
  if (g_track_count <= 0) return;
  g_track_index--;
  if (g_track_index < 0) g_track_index = g_track_count - 1;
  g_position_sec = 0.0f;
}

int player_get_track_index() {
  return g_track_index;
}

int player_get_track_count() {
  return g_track_count;
}

void player_seek_to(float seconds) {
  g_position_sec = clampf(seconds, 0.0f, g_duration_sec);
  // Later: translate to byte offset + file seek + buffer flush
}

void player_seek_by(float delta_seconds) {
  player_seek_to(g_position_sec + delta_seconds);
}

float player_get_position_sec() {
  return g_position_sec;
}

float player_get_duration_sec() {
  return g_duration_sec;
}
