#pragma once
#include "general.h"
#include "bt_a2dp.h"


// ===== Bluetooth Device Structure =====
struct BTDevice {
  char name[32];
  char address[18];  // MAC address format: XX:XX:XX:XX:XX:XX
  int rssi;          // Signal strength
  bool paired;
};


// Global Bluetooth device list
inline BTDevice btDevices[20];  // Max 20 devices
inline int btDeviceCount = 0;
inline const char* btStatus = "Idle";
inline int btSelectedIndex = 0;
inline int btScrollOffset = 0;
inline bool btScanning = false;
inline bool btEnabled = false;
inline uint32_t btScanStartMs = 0;  // tracks when scan began, for 10s timeout

inline int settingsSelectedIndex = 0;

// ===== Display Constants =====
const int BT_DEVICES_PER_PAGE = 5;
const int BT_ITEM_HEIGHT = 35;
const uint32_t BT_SCAN_DURATION_MS = 10000;

// Track last state for partial updates
inline int lastSettingsSelectedIndex = -1;
inline int lastBtSelectedIndex = -1;
inline int lastBtScrollOffset = -1;


// ---- Dummy PCM callback (connect-only phase) ----
// bt_init() requires a callback, but audio streaming is not yet implemented.
static int32_t dummy_pcm_cb(Frame* frames, int32_t frame_count) {
  static float phase = 0.0f;

  const float sample_rate = 44100.0f;
  const float freq = 440.0f;
  const float amplitude = 12000.0f;
  const float two_pi = 6.28318530718f;

  for (int i = 0; i < frame_count; i++) {
    int16_t sample = (int16_t)(amplitude * sinf(phase));
    frames[i].channel1 = sample;
    frames[i].channel2 = sample;
    phase += two_pi * freq / sample_rate;
    if (phase >= two_pi) phase -= two_pi;
  }

  return frame_count;
}

// Convert MAC bytes -> "AA:BB:CC:DD:EE:FF"
inline void bdaToString(const esp_bd_addr_t bda, char out[18]) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

// Pull backend scan results into UI array (btDevices[])
inline void syncBtDevicesFromBackend() {
  btDeviceCount = 0;

  int n = bt_scan_count();
  if (n > 20) n = 20;

  for (int i = 0; i < n; i++) {
    BtScanDevice d{};
    if (!bt_scan_get(i, d)) continue;

    if (d.name[0]) strncpy(btDevices[btDeviceCount].name, d.name, sizeof(btDevices[0].name) - 1);
    else strncpy(btDevices[btDeviceCount].name, "(no name)", sizeof(btDevices[0].name) - 1);
    btDevices[btDeviceCount].name[sizeof(btDevices[0].name) - 1] = '\0';

    bdaToString(d.bda, btDevices[btDeviceCount].address);

    btDevices[btDeviceCount].rssi = d.rssi;

    esp_bd_addr_t cbda;
    bool hasConn = bt_get_connected_bda(cbda);
    btDevices[btDeviceCount].paired = hasConn && (memcmp(cbda, d.bda, 6) == 0);

    btDeviceCount++;
  }

  if (bt_is_connected()) btStatus = "Connected";
  else if (btScanning)   btStatus = "Scanning...";
  else                   btStatus = "Idle";
}

// Initialize Bluetooth — guarded, safe to call multiple times
inline void initBluetooth() {
  if (btEnabled) return;
  bt_init("Auffline", dummy_pcm_cb);
  btEnabled = true;
}

// Start scanning for nearby Bluetooth devices
inline void startBluetoothScan() {
  bt_scan_clear();
  btDeviceCount = 0;
  btSelectedIndex = 0;
  btScrollOffset = 0;
  btScanning = true;
  btScanStartMs = millis();
  bt_scan_start(10);
}

// Live refresh — call every loop iteration.
// Syncs backend scan results into UI and triggers redraws as new devices appear.
inline void btLiveRefresh() {
  if (currentScreen != SCREEN_BLUETOOTH) return;

  // Auto-stop scanning after 10 s to match bt_scan_start(10)
  if (btScanning && (millis() - btScanStartMs >= BT_SCAN_DURATION_MS)) {
    btScanning = false;
    needsRedraw = true;
    return;
  }

  static uint32_t lastSync = 0;
  if (millis() - lastSync < 300) return;
  lastSync = millis();

  int oldCount = btDeviceCount;
  syncBtDevicesFromBackend();

  if (btDeviceCount != oldCount) {
    // New devices appeared — reset selection if list was empty before
    if (oldCount == 0) {
      btSelectedIndex = 0;
      btScrollOffset = 0;
    }
    needsRedraw = true;
  }
}


// Draw a single Bluetooth device item
inline void drawBluetoothDeviceItem(int displayIndex, int devIndex, bool selected) {
  const int listStartY = SMALL_HEADER_HEIGHT + 55;
  int y = listStartY + (displayIndex * BT_ITEM_HEIGHT);

  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, BT_ITEM_HEIGHT - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, BT_ITEM_HEIGHT - 5, 8, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);

  char displayName[18];
  strncpy(displayName, btDevices[devIndex].name, 17);
  displayName[17] = '\0';
  tft.print(displayName);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 5, y + 16);

  if (btDevices[devIndex].paired) {
    tft.setTextColor(COLOR_ORANGE);
    tft.print("Connected");
  } else {
    int rssi = btDevices[devIndex].rssi;
    if (rssi > -50)      tft.print("Strong");
    else if (rssi > -70) tft.print("Medium");
    else                 tft.print("Weak");
  }

  // Signal bars
  int signalX = 270;
  int signalY = y + 5;
  int devRssi = btDevices[devIndex].rssi;
  int bars = (devRssi > -50) ? 3 : (devRssi > -70) ? 2 : 1;

  for (int b = 0; b < 3; b++) {
    int barHeight = 4 + (b * 4);
    uint16_t color = (b < bars) ? COLOR_ORANGE : COLOR_GREY;
    tft.fillRect(signalX + (b * 6), signalY + (12 - barHeight), 4, barHeight, color);
  }
}

// Update Bluetooth scroll indicator
inline void updateBluetoothScrollIndicator() {
  if (btDeviceCount <= BT_DEVICES_PER_PAGE) return;

  const int listStartY = SMALL_HEADER_HEIGHT + 55;
  int scrollBarHeight = 100;
  int scrollBarX = 310;
  int scrollBarWidth = 4;

  tft.fillRoundRect(scrollBarX, listStartY, scrollBarWidth, scrollBarHeight, 2, COLOR_GREY);

  float thumbRatio = (float)BT_DEVICES_PER_PAGE / (float)btDeviceCount;
  int thumbHeight = (int)(scrollBarHeight * thumbRatio);
  if (thumbHeight < 10) thumbHeight = 10;

  float scrollRatio = (float)btScrollOffset / (float)(btDeviceCount - BT_DEVICES_PER_PAGE);
  int thumbY = listStartY + (int)((scrollBarHeight - thumbHeight) * scrollRatio);

  tft.fillRoundRect(scrollBarX, thumbY, scrollBarWidth, thumbHeight, 2, COLOR_ORANGE);
}

// Partial update for Bluetooth device list
inline void updateBluetoothSelection() {
  bool scrolled = (btScrollOffset != lastBtScrollOffset);

  if (scrolled) {
    const int listStartY = SMALL_HEADER_HEIGHT + 55;
    tft.fillRect(0, listStartY, 320, 185, COLOR_BG);

    for (int i = 0; i < BT_DEVICES_PER_PAGE && (btScrollOffset + i) < btDeviceCount; i++) {
      int devIndex = btScrollOffset + i;
      drawBluetoothDeviceItem(i, devIndex, devIndex == btSelectedIndex);
    }

    updateBluetoothScrollIndicator();
  } else {
    int oldDisplayIndex = lastBtSelectedIndex - lastBtScrollOffset;
    int newDisplayIndex = btSelectedIndex - btScrollOffset;

    if (oldDisplayIndex >= 0 && oldDisplayIndex < BT_DEVICES_PER_PAGE &&
        lastBtSelectedIndex >= 0 && lastBtSelectedIndex < btDeviceCount) {
      drawBluetoothDeviceItem(oldDisplayIndex, lastBtSelectedIndex, false);
    }

    if (newDisplayIndex >= 0 && newDisplayIndex < BT_DEVICES_PER_PAGE &&
        btSelectedIndex >= 0 && btSelectedIndex < btDeviceCount) {
      drawBluetoothDeviceItem(newDisplayIndex, btSelectedIndex, true);
    }
  }

  lastBtSelectedIndex = btSelectedIndex;
  lastBtScrollOffset = btScrollOffset;
}

// Draw Bluetooth screen — inits BT stack on first call, auto-starts scan
inline void drawBluetoothScreen() {
  initBluetooth();

  // Auto-start scan on every fresh entry (not already scanning, not connected)
  if (!btScanning && !bt_is_connected()) {
    startBluetoothScan();
  }

  tft.fillScreen(COLOR_BG);
  drawSmallHeader();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Bluetooth");

  // Status line
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  if (btScanning) {
    tft.print("Scanning...");
  } else if (bt_is_connected()) {
    tft.setTextColor(COLOR_ORANGE);
    tft.print("Connected");
  } else {
    tft.printf("%d devices found  SELECT to rescan", btDeviceCount);
  }

  // Hint when devices are visible
  if (btDeviceCount > 0) {
    tft.setTextColor(COLOR_ORANGE);
    tft.setCursor(220, SMALL_HEADER_HEIGHT + 30);
    tft.print("SELECT=connect");
  }

  if (btDeviceCount == 0 && !btScanning) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(40, 110);
    tft.print("No devices found");
    tft.setTextSize(1);
    tft.setCursor(80, 138);
    tft.print("Press SELECT to rescan");
    return;
  }

  if (btDeviceCount == 0 && btScanning) {
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(70, 110);
    tft.print("Searching...");
    return;
  }

  for (int i = 0; i < BT_DEVICES_PER_PAGE && (btScrollOffset + i) < btDeviceCount; i++) {
    int devIndex = btScrollOffset + i;
    drawBluetoothDeviceItem(i, devIndex, devIndex == btSelectedIndex);
  }

  updateBluetoothScrollIndicator();

  lastBtSelectedIndex = btSelectedIndex;
  lastBtScrollOffset = btScrollOffset;
}

// Handle selecting/connecting to the highlighted Bluetooth device
inline void selectBluetoothDevice() {
  if (btDeviceCount == 0) return;

  if (strcmp(btStatus, "Connecting...") == 0 || strcmp(btStatus, "Connected") == 0) return;

  btStatus = "Connecting...";
  bt_connect_index(btSelectedIndex);
}

// Handle navigation in Bluetooth menu
inline void handleBluetoothNav(NavEvent e) {
  if (e == NAV_SELECT) {
    if (btDeviceCount == 0) {
      if (!btScanning) {
        startBluetoothScan();
        needsRedraw = true;
      }
    } else {
      selectBluetoothDevice();
    }
    return;
  }

  if (e == NAV_BACK) {
    if (btScanning) {
      bt_scan_stop();
      btScanning = false;
    }
    navigateTo(SCREEN_SETTINGS);
    return;
  }

  if (btDeviceCount == 0) return;

  if (e == NAV_UP || e == NAV_LEFT) {
    btSelectedIndex--;
    if (btSelectedIndex < 0) {
      btSelectedIndex = btDeviceCount - 1;
      btScrollOffset = max(0, btDeviceCount - BT_DEVICES_PER_PAGE);
    } else {
      if (btSelectedIndex < btScrollOffset) {
        btScrollOffset = btSelectedIndex;
      }
    }
    updateBluetoothSelection();
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    btSelectedIndex++;
    if (btSelectedIndex >= btDeviceCount) {
      btSelectedIndex = 0;
      btScrollOffset = 0;
    } else {
      if (btSelectedIndex >= btScrollOffset + BT_DEVICES_PER_PAGE) {
        btScrollOffset = btSelectedIndex - BT_DEVICES_PER_PAGE + 1;
      }
    }
    updateBluetoothSelection();
  }
}
