#include <Arduino.h>
#include <math.h>
//#include "song_pcm.h" // - replacing this with SD card 
//#include <pgmspace.h>
#include "player.h"
#include "audio_out.h"
#include "buttons.h"
#include "bt_a2dp.h"
#include "sd_library.h"
#include "file_transfer.h"

/*static size_t pcm_index = 0;

static inline int16_t read_i16_progmem(const uint8_t* p) {
  uint8_t lo = pgm_read_byte(p);
  uint8_t hi = pgm_read_byte(p + 1);
  return (int16_t)((hi << 8) | lo); // little-endian
}

volatile int g_pcm_volume = 80; // for safety max to 80. can delete later. 

static inline int16_t apply_gain_i16(int16_t s) {
  int32_t x = (int32_t)s * g_pcm_volume / 100;
  if (x > 32767) x = 32767;
  if (x < -32768) x = -32768;
  return (int16_t)x;
}

int32_t get_sound_data(Frame *frame, int32_t frame_count) {
  const size_t bytes_per_frame = 4;
  const size_t total_frames = voice10_raw_len / bytes_per_frame;

  for (int i = 0; i < frame_count; i++) {
    if (pcm_index >= total_frames) pcm_index = 0;

    const uint8_t* base = voice10_raw + pcm_index * bytes_per_frame;
    frame[i].channel1 = apply_gain_i16(read_i16_progmem(base + 0));
    frame[i].channel2 = apply_gain_i16(read_i16_progmem(base + 2));
    pcm_index++;
  }
  return frame_count;
} */


IAudioOut* getAudioOut();

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("BOOT");

  sd_library_init();
  sd_library_scan();
  
  if (Serial) {
    file_transfer_init();
}

 //bt_init("ECE396-ESP32-A2DP", get_sound_data);

  player_init();
  player_attach_audio_out(getAudioOut());
  buttons_init();

  Serial.println("Pair headphones to: ECE396-ESP32-A2DP (but ESP32 will actively scan/connect to sinks)");
}


void loop() {
ButtonEvent ev = buttons_poll();
  if (ev != ButtonEvent::None) {
    switch (ev) {
      case ButtonEvent::Play:
        player_toggle();          // play <-> pause
        break;
  
      case ButtonEvent::Set:
        player_next();            // next track
        break;

      case ButtonEvent::SetLong:
        player_prev();          // LONG press = previous track
        break;
  
      case ButtonEvent::VolUp:
        player_volume_up();       // we’ll add this
        break;
  
      case ButtonEvent::VolDown:
        player_volume_down();     // we’ll add this
        break;
  
      default:
        break;
    }
  }
  
  file_transfer_loop();
  delay(10);
  
}



