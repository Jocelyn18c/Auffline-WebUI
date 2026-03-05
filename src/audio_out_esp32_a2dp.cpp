#include "audio_out.h"
#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "bt_a2dp.h"

// These will come from main.cpp (we will make them non-static)
extern BluetoothA2DPSource a2dp;

class Esp32A2dpAudioOut : public IAudioOut {
public:
  bool begin() override { return true; }

  bool setFormat(const AudioFormat& fmt) override {
    (void)fmt; // accept for now
    return true;
  }

  bool play() override {
    if (!bt_is_connected()) return false;
  
    esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
    Serial.print("[AudioOut] START -> ");
    Serial.println(err == ESP_OK ? "OK" : "FAIL");
  
    return err == ESP_OK;
  }

  bool pause() override {
    if (!bt_is_connected()) return false;
  
    esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    Serial.print("[AudioOut] SUSPEND -> ");
    Serial.println(err == ESP_OK ? "OK" : "FAIL");
    return err == ESP_OK;
  }

  bool stop() override {
    if (!bt_is_connected()) return false;

    esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
    Serial.print("[AudioOut] STOP -> ");
    Serial.println(err == ESP_OK ? "OK" : "FAIL");
    return err == ESP_OK;
  }

  AudioOutState state() const override {
    if (!bt_is_connected()) {
      return AudioOutState::Stopped;
    }
  
    if (!bt_stream_started()) {
      return AudioOutState::Paused;
    }
  
    return AudioOutState::Playing;
  }
};

static Esp32A2dpAudioOut g_audioOut;

// Player will call this to get the output
IAudioOut* getAudioOut() { return &g_audioOut; }
