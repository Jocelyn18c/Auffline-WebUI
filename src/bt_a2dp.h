#pragma once
#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

// Initialize Bluetooth A2DP source + optional AVRCP.
// `device_name` is the ESP32 advertised name.
// `data_cb` is your PCM provider callback.
void bt_init(const char* device_name, int32_t (*data_cb)(Frame*, int32_t));

// Expose pause state so Player can tell Bluetooth not to auto-restart.
extern bool bt_user_paused;

// Optional helpers (may be ignored by some speakers)
void bt_vol_up();
void bt_vol_down();

bool bt_is_connected();
bool bt_stream_started();
