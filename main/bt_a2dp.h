// what functions exist
#pragma once
#include <Arduino.h>
#include <vector>
#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"   // esp_bt_gap_start_discovery / cancel

static bool s_connected = false;
static bool s_has_bda = false;
bool bt_get_connected_bda(esp_bd_addr_t out_bda);

// bt_init()
// Called once from main.cpp setup().
// - Starts ESP32 A2DP Source with advertised name (device_name)
// - Registers callbacks for connection/audio state/discovery
// - Registers your PCM audio callback (data_cb) used by A2DP streaming
void bt_init(const char* device_name, int32_t (*data_cb)(Frame*, int32_t));

// bt_user_paused
// Shared flag used by Player layer.
// - When user presses Pause: Player sets bt_user_paused=true
// - When user presses Play:  Player sets bt_user_paused=false
// Bluetooth uses it to prevent "auto-restart after suspend" when pause was intentional.
extern bool bt_user_paused;

// Optional AVRCP passthrough helpers (may be ignored by some speakers)
void bt_vol_up();
void bt_vol_down();

// Connection/stream state queries used by AudioOut/Player to reflect state
bool bt_is_connected();
bool bt_stream_started();
bool bt_get_connected_bda(esp_bd_addr_t out_bda);

// BtScanDevice
// A single discovered Bluetooth device entry (for frontend to display)
struct BtScanDevice {
  char name[64];          // device name (can be empty if unknown)
  esp_bd_addr_t bda;      // MAC address (6 bytes)
  int rssi;               // signal strength (may be 0 or negative)
  uint32_t last_seen_ms;  // timestamp when last updated (millis())
};

// bt_scan_start()
// Starts classic Bluetooth inquiry scan for nearby devices.
// Discovered devices will trigger on_found_device() callback internally.
bool bt_scan_start(uint8_t seconds = 10);

// bt_scan_stop()
// Stops inquiry scan early.
void bt_scan_stop();

// bt_scan_clear()
// Clears stored scan results list.
void bt_scan_clear();

// bt_scan_count() / bt_scan_get()
// Frontend/UI uses these to render a list of found devices.
int  bt_scan_count();
bool bt_scan_get(int idx, BtScanDevice& out);

// bt_connect_index() / bt_connect_bda()
// Manual connect API:
// - choose device by index from scan list OR explicit MAC.
// Once connected, on_conn_state() triggers and streaming starts automatically.
bool bt_connect_index(int idx);
bool bt_connect_bda(const esp_bd_addr_t& bda);

// bt_disconnect()
// Disconnect current A2DP sink device.
void bt_disconnect();
