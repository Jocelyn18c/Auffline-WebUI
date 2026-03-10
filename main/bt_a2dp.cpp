//contains actual logics
#include "bt_a2dp.h"

static bool s_has_connected_bda = false;
static esp_bd_addr_t s_connected_bda = {0};
static bool s_has_pending_bda = false;
static esp_bd_addr_t s_pending_bda = {0};

// ---------- Internal BT objects/state ----------

// Main A2DP source object from the ESP32-A2DP library.
// Owns Bluetooth stack setup and connection commands.
static BluetoothA2DPSource a2dp;

// Internal connection flags updated by callbacks.
static bool is_connected = false;
static bool stream_started = false;

// Exposed pause flag (Player writes, BT reads).
bool bt_user_paused = false;

// Used to prevent infinite restart loop after remote suspend.
static bool retried_after_suspend = false;

// AVRCP controller readiness (used by bt_vol_up/down)
static volatile bool avrcp_ready = false;


// ---------- NEW: scan storage ----------

// List of discovered devices (deduped by MAC address).
static std::vector<BtScanDevice> g_scan;

// Cap how many devices we keep (prevents memory growth)
static const int kMaxScanDevices = 40;

// Mutex to protect g_scan because BT callbacks may run in another task.
static portMUX_TYPE g_scan_mux = portMUX_INITIALIZER_UNLOCKED;


// same_bda()
// Helper: compares two Bluetooth MAC addresses.
static bool same_bda(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  return memcmp(a, b, 6) == 0;
}


// scan_upsert()
// Adds/updates a device in the g_scan list.
// - If device already exists (same MAC), update name/rssi/last_seen.
// - Otherwise insert new entry (up to kMaxScanDevices).
static void scan_upsert(const char* name, const esp_bd_addr_t bda, int rssi) {
  portENTER_CRITICAL(&g_scan_mux);

  // Update existing device
  for (auto &d : g_scan) {
    if (same_bda(d.bda, bda)) {
      if (name && name[0]) {
        strncpy(d.name, name, sizeof(d.name) - 1);
        d.name[sizeof(d.name) - 1] = '\0';
      }
      d.rssi = rssi;
      d.last_seen_ms = millis();
      portEXIT_CRITICAL(&g_scan_mux);
      return;
    }
  }

  // Insert new device (if room)
  if ((int)g_scan.size() < kMaxScanDevices) {
    BtScanDevice nd{};
    memset(&nd, 0, sizeof(nd));

    if (name && name[0]) {
      strncpy(nd.name, name, sizeof(nd.name) - 1);
      nd.name[sizeof(nd.name) - 1] = '\0';
    } else {
      nd.name[0] = '\0';
    }

    memcpy(nd.bda, bda, 6);
    nd.rssi = rssi;
    nd.last_seen_ms = millis();
    g_scan.push_back(nd);
  }

  portEXIT_CRITICAL(&g_scan_mux);
}


// start_stream_once()
// Requests Bluetooth stack to start streaming audio.
// This should only happen once per connection.
static void start_stream_once() {
  if (!is_connected) return;
  if (stream_started) return;

  // ESP-IDF call to start A2DP media stream.
  esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);

  Serial.print("[A2DP] START -> ");
  Serial.println(err == ESP_OK ? "OK" : "FAIL");

  if (err == ESP_OK) stream_started = true;
}


// on_conn_state()
// Called by BT stack when connection state changes.
static void on_conn_state(esp_a2d_connection_state_t state, void *) {
  Serial.print("[A2DP] Connection state: ");
  Serial.println(a2dp.to_str(state));

  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    is_connected = true;
    stream_started = false;
  
    // Promote "pending" -> "connected"
    if (s_has_pending_bda) {
      memcpy(s_connected_bda, s_pending_bda, 6);
      s_has_connected_bda = true;
    }
  
    delay(300);
    Serial.println("[A2DP] Connected -> start stream once");
    start_stream_once();
  }
  
  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    is_connected = false;
    stream_started = false;
  
    s_has_connected_bda = false;
    memset(s_connected_bda, 0, 6);
  
    s_has_pending_bda = false;
    memset(s_pending_bda, 0, 6);
  
    bt_scan_clear();
  }
}


// on_audio_state()
// Called by BT stack when audio streaming state changes.
static void on_audio_state(esp_a2d_audio_state_t state, void *) {
  Serial.print("[A2DP] Audio state: ");
  Serial.println(a2dp.to_str(state));

  // If audio started successfully, allow future retries if suspend happens later
  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    retried_after_suspend = false;
  }

  // Some speakers send REMOTE_SUSPEND right after connect.
  // We do ONE auto-restart unless the user intentionally paused.
  if (state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND && is_connected) {

    // If user paused, never auto-restart
    if (bt_user_paused) {
      Serial.println("[A2DP] Suspended (user paused) -> no auto-restart");
      return;
    }

    // Otherwise retry START once
    if (!retried_after_suspend) {
      retried_after_suspend = true;
      Serial.println("[A2DP] Suspended -> one retry START");
      delay(200);
      stream_started = false;
      start_stream_once();
    }
  }
}


// on_discovery_state()
// Prints scan state transitions (started/stopped).
static void on_discovery_state(esp_bt_gap_discovery_state_t st) {
  Serial.print("[GAP] Discovery: ");
  Serial.println(a2dp.to_str(st));
}


// on_found_device()
// Called repeatedly during inquiry scan for each discovered device.
// IMPORTANT return value:
// - returning true tells the library to auto-connect.
// We always return false so selection/connection is manual via frontend/UI.
static bool on_found_device(const char *name, esp_bd_addr_t address, int rssi) {
  // store in scan list
  scan_upsert(name, address, rssi);

  // debug print
  Serial.print("[SCAN] ");
  Serial.print(name && name[0] ? name : "(no-name)");
  Serial.print("  mac=");
  Serial.print(a2dp.to_str(address));
  Serial.print("  rssi=");
  Serial.println(rssi);

  return false; // stop auto-connect/hardcoding
}


// avrc_ct_callback()
// AVRCP controller callback (optional). Tracks if AVRCP is connected.
static void avrc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT) {
    avrcp_ready = param->conn_stat.connected;
    Serial.printf("[AVRCP] connected=%d\n", (int)avrcp_ready);
  }
}


// bt_init()
// Public entrypoint: set callbacks, disable auto reconnect, start A2DP,
// register PCM callback, and init AVRCP.
void bt_init(const char* device_name, int32_t (*data_cb)(Frame*, int32_t)) {
  a2dp.set_on_connection_state_changed(on_conn_state);
  a2dp.set_on_audio_state_changed(on_audio_state);
  a2dp.set_discovery_mode_callback(on_discovery_state);
  a2dp.set_ssid_callback(on_found_device);

  // Prevent ESP32 from reconnecting to the last device automatically.
  a2dp.set_auto_reconnect(false);
  a2dp.clean_last_connection();

  // Start A2DP source and expose advertised name
  a2dp.start(device_name);

  // Register audio callback that supplies PCM frames
  a2dp.set_data_callback_in_frames(data_cb);

  // Optional AVRCP (for debug/future volume control)
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrc_ct_callback);
}


// bt_vol_up() / bt_vol_down()
// Sends AVRCP passthrough volume commands if AVRCP is ready.
void bt_vol_up() {
  if (!avrcp_ready) { Serial.println("[AVRCP] not ready"); return; }
  Serial.println("[AVRCP] VOL_UP");
  esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_VOL_UP, ESP_AVRC_PT_CMD_STATE_PRESSED);
  esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_VOL_UP, ESP_AVRC_PT_CMD_STATE_RELEASED);
}

void bt_vol_down() {
  if (!avrcp_ready) { Serial.println("[AVRCP] not ready"); return; }
  Serial.println("[AVRCP] VOL_DOWN");
  esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_VOL_DOWN, ESP_AVRC_PT_CMD_STATE_PRESSED);
  esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_VOL_DOWN, ESP_AVRC_PT_CMD_STATE_RELEASED);
}


// bt_is_connected() / bt_stream_started()
// Small getters for other modules (AudioOut/Player).
bool bt_is_connected() { return is_connected; }
bool bt_stream_started() { return stream_started; }


// bt_scan_start()
// Starts classic Bluetooth inquiry scan via ESP-IDF.
// During scan, on_found_device() will be called for each device.
bool bt_scan_start(uint8_t seconds) {
  if (seconds == 0) seconds = 10;

  bt_scan_clear(); // start fresh

  Serial.printf("[GAP] scan start (%u s)\n", seconds);

  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, seconds, 0);
  if (err != ESP_OK) {
    Serial.printf("[GAP] start_discovery failed: %d\n", (int)err);
    return false;
  }
  return true;
}


// bt_scan_stop()
// Cancels discovery early.
void bt_scan_stop() {
  Serial.println("[GAP] scan stop");
  esp_bt_gap_cancel_discovery();
}


// bt_scan_clear()
// Clears stored scan results list.
void bt_scan_clear() {
  portENTER_CRITICAL(&g_scan_mux);
  g_scan.clear();
  portEXIT_CRITICAL(&g_scan_mux);
}


// bt_scan_count()
// Returns number of devices currently in the scan list.
int bt_scan_count() {
  portENTER_CRITICAL(&g_scan_mux);
  int n = (int)g_scan.size();
  portEXIT_CRITICAL(&g_scan_mux);
  return n;
}


// bt_scan_get()
// Copies scan entry into out for UI rendering.
bool bt_scan_get(int idx, BtScanDevice& out) {
  portENTER_CRITICAL(&g_scan_mux);
  if (idx < 0 || idx >= (int)g_scan.size()) {
    portEXIT_CRITICAL(&g_scan_mux);
    return false;
  }
  out = g_scan[idx];
  portEXIT_CRITICAL(&g_scan_mux);
  return true;
}


// bt_connect_index()
// Convenience: connect to device by index in scan list.
bool bt_connect_index(int idx) {
  BtScanDevice d{};
  if (!bt_scan_get(idx, d)) return false;

  s_has_pending_bda = true;
  memcpy(s_pending_bda, d.bda, 6);

  return bt_connect_bda(d.bda);
}


// bt_connect_bda()
// Manual connect API using explicit MAC address.
// Flow:
// - stop scanning
// - disconnect current device (if any)
// - connect_to(new_mac)
// After connect succeeds, on_conn_state(CONNECTED) will start streaming.
bool bt_connect_bda(const esp_bd_addr_t& bda) {
  uint8_t* mac = (uint8_t*)(&bda[0]);   // pointer to 6 bytes

  Serial.print("[A2DP] connect_to mac=");
  Serial.println(a2dp.to_str(mac));

  bt_scan_stop();

  a2dp.disconnect();
  delay(100);

  bool ok = a2dp.connect_to(mac);
  Serial.printf("[A2DP] connect_to -> %s\n", ok ? "OK" : "FAIL");
  return ok;
}

bool bt_get_connected_bda(esp_bd_addr_t out_bda) {
  if (!bt_is_connected() || !s_has_connected_bda) return false;
  memcpy(out_bda, s_connected_bda, 6);
  return true;
}

// bt_disconnect()
// Disconnect current A2DP sink.
void bt_disconnect() {
  Serial.println("[A2DP] disconnect()");
  a2dp.disconnect();
}


