#include "bt_a2dp.h"

// ---------- Internal BT objects/state ----------
static BluetoothA2DPSource a2dp;

static bool is_connected = false;
static bool stream_started = false;
bool bt_user_paused = false;              // <-- exposed (extern in header)
static bool retried_after_suspend = false;

static volatile bool avrcp_ready = false;


// ---------- Internal helpers ----------
static void start_stream_once() {
  if (!is_connected) return;
  if (stream_started) return;

  esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
  Serial.print("[A2DP] START -> ");
  Serial.println(err == ESP_OK ? "OK" : "FAIL");

  if (err == ESP_OK) stream_started = true;
}

// ---------- Callbacks ----------
static void on_conn_state(esp_a2d_connection_state_t state, void *) {
  Serial.print("[A2DP] Connection state: ");
  Serial.println(a2dp.to_str(state));

  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    is_connected = true;
    stream_started = false;

    delay(300);
    Serial.println("[A2DP] Connected -> start stream once");
    start_stream_once();
  }

  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    is_connected = false;
    stream_started = false;
  }
}

static void on_audio_state(esp_a2d_audio_state_t state, void *) {
  Serial.print("[A2DP] Audio state: ");
  Serial.println(a2dp.to_str(state));

  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    retried_after_suspend = false;
  }

  if (state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND && is_connected) {
    if (bt_user_paused) {
      Serial.println("[A2DP] Suspended (user paused) -> no auto-restart");
      return;
    }

    if (!retried_after_suspend) {
      retried_after_suspend = true;
      Serial.println("[A2DP] Suspended -> one retry START");
      delay(200);
      stream_started = false;
      start_stream_once();
    }
  }
}

static void on_discovery_state(esp_bt_gap_discovery_state_t st) {
  Serial.print("[GAP] Discovery: ");
  Serial.println(a2dp.to_str(st));
}

static bool on_found_device(const char *name, esp_bd_addr_t address, int rssi) {
  Serial.print("[SCAN] name='");
  Serial.print(name ? name : "(null)");
  Serial.print("'  mac=");
  Serial.print(a2dp.to_str(address));
  Serial.print("  rssi=");
  Serial.println(rssi);

  // Keep your exact filter to avoid behavior changes
  if (name && strstr(name, "EMBERTON II")) {
    Serial.println("[SCAN] >>> Target match! Attempting connect...");
    return true;
  }
  return false;
}

static void avrc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT) {
    avrcp_ready = param->conn_stat.connected;
    Serial.printf("[AVRCP] connected=%d\n", (int)avrcp_ready);
  }
}

// ---------- Public API ----------
void bt_init(const char* device_name, int32_t (*data_cb)(Frame*, int32_t)) {
  a2dp.set_on_connection_state_changed(on_conn_state);
  a2dp.set_on_audio_state_changed(on_audio_state);
  a2dp.set_discovery_mode_callback(on_discovery_state);
  a2dp.set_ssid_callback(on_found_device);

  a2dp.start(device_name);
  a2dp.set_data_callback_in_frames(data_cb);

  // Optional: AVRCP (kept for debugging / future use)
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrc_ct_callback);
}

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

bool bt_is_connected() {
    return is_connected;
  }
  
  bool bt_stream_started() {
    return stream_started;
  }
