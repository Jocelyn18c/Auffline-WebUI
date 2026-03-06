#pragma once
#include "general.h"

// Forward declaration
enum NavEvent : uint8_t;

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
inline int btSelectedIndex = 0;
inline int btScrollOffset = 0;
inline bool btScanning = false;
inline bool btEnabled = false;

// ===== Settings Menu Items =====
enum SettingsItem : uint8_t {
  SETTINGS_BLUETOOTH = 0,
  SETTINGS_WIFI,
  SETTINGS_DISPLAY,
  SETTINGS_AUDIO,
  SETTINGS_ABOUT,
  SETTINGS_COUNT
};

inline const char* const settingsItems[] = {
  "Bluetooth",
  "Wi-Fi",
  "Display",
  "Audio",
  "About"
};

inline int settingsSelectedIndex = 0;
inline bool inBluetoothMenu = false;

// ===== Display Constants =====
const int BT_DEVICES_PER_PAGE = 5;
const int BT_ITEM_HEIGHT = 35;

// Track last state for partial updates
inline int lastSettingsSelectedIndex = -1;
inline int lastBtSelectedIndex = -1;
inline int lastBtScrollOffset = -1;

// ===== Bluetooth Functions (TEMPLATES) =====

// Initialize Bluetooth (TEMPLATE - implement later)
inline void initBluetooth() {
  /*
   * TODO: Implement Bluetooth initialization
   * 
   * For ESP32:
   * 1. Include required libraries:
   *    - #include "BluetoothSerial.h" (for classic BT)
   *    - OR #include "BLEDevice.h" (for BLE)
   * 
   * 2. Initialize Bluetooth:
   *    - BluetoothSerial SerialBT;
   *    - SerialBT.begin("ESP32_Audio");
   * 
   * 3. For BLE scanning:
   *    - BLEDevice::init("ESP32_Audio");
   *    - BLEScan* pBLEScan = BLEDevice::getScan();
   *    - pBLEScan->setActiveScan(true);
   */
  
  btEnabled = true;  // Placeholder
}

// Start scanning for Bluetooth devices (TEMPLATE)
inline void startBluetoothScan() {
  /*
   * TODO: Implement Bluetooth scanning
   * 
   * For ESP32 Classic Bluetooth:
   * - Use ESP_BT discovery APIs
   * 
   * For BLE:
   * - BLEScanResults foundDevices = pBLEScan->start(scanTime);
   * - for (int i = 0; i < foundDevices.getCount(); i++) {
   *     BLEAdvertisedDevice device = foundDevices.getDevice(i);
   *     strcpy(btDevices[btDeviceCount].name, device.getName().c_str());
   *     strcpy(btDevices[btDeviceCount].address, device.getAddress().toString().c_str());
   *     btDevices[btDeviceCount].rssi = device.getRSSI();
   *     btDeviceCount++;
   *   }
   * 
   * Callback approach for continuous scanning:
   * - Implement BLEAdvertisedDeviceCallbacks
   * - Update btDevices array in callback
   */
  
  // Placeholder: Add some dummy devices for UI testing
  btScanning = true;
  btDeviceCount = 0;
  
  // Simulate discovered devices
  strcpy(btDevices[0].name, "AirPods Pro");
  strcpy(btDevices[0].address, "AA:BB:CC:DD:EE:01");
  btDevices[0].rssi = -45;
  btDevices[0].paired = false;
  
  strcpy(btDevices[1].name, "iPhone");
  strcpy(btDevices[1].address, "AA:BB:CC:DD:EE:02");
  btDevices[1].rssi = -60;
  btDevices[1].paired = true;
  
  strcpy(btDevices[2].name, "Car Stereo");
  strcpy(btDevices[2].address, "AA:BB:CC:DD:EE:03");
  btDevices[2].rssi = -75;
  btDevices[2].paired = false;
  
  strcpy(btDevices[3].name, "Bluetooth Speaker");
  strcpy(btDevices[3].address, "AA:BB:CC:DD:EE:04");
  btDevices[3].rssi = -55;
  btDevices[3].paired = false;
  
  btDeviceCount = 4;
  btScanning = false;
}

// Connect to a Bluetooth device (TEMPLATE)
inline void connectBluetoothDevice(int deviceIndex) {
  /*
   * TODO: Implement Bluetooth connection
   * 
   * For Classic Bluetooth:
   * - SerialBT.connect(btDevices[deviceIndex].address);
   * 
   * For BLE:
   * - BLEClient* pClient = BLEDevice::createClient();
   * - pClient->connect(BLEAddress(btDevices[deviceIndex].address));
   * 
   * Handle pairing if needed:
   * - Some devices require PIN or confirmation
   * - Implement callbacks for pairing requests
   */
  
  // Placeholder
  btDevices[deviceIndex].paired = true;
}

// Disconnect Bluetooth device (TEMPLATE)
inline void disconnectBluetoothDevice(int deviceIndex) {
  /*
   * TODO: Implement Bluetooth disconnection
   * 
   * - SerialBT.disconnect();
   * - OR pClient->disconnect();
   */
  
  // Placeholder
  btDevices[deviceIndex].paired = false;
}

// ===== Drawing Helper Functions =====

// Draw a single settings menu item
inline void drawSettingsMenuItem(int index, bool selected) {
  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int itemH = 40;
  int y = startY + index * itemH;
  
  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 6, 300, 32, 10, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 8, 6, COLOR_ORANGE);
    tft.setTextColor(COLOR_TEXT);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 6, 300, 32, 10, COLOR_BG);
    tft.setTextColor(COLOR_GREY);
  }
  
  tft.setTextSize(2);
  tft.setCursor(PADDING * 5, y);
  tft.print(settingsItems[index]);
  
  // Draw status indicators
  if (index == SETTINGS_BLUETOOTH) {
    tft.setTextSize(1);
    tft.setCursor(240, y + 5);
    tft.setTextColor(btEnabled ? COLOR_ORANGE : COLOR_GREY);
    tft.print(btEnabled ? "ON" : "OFF");
  }
}

// Partial update for settings menu
inline void updateSettingsMenuSelection() {
  if (settingsSelectedIndex == lastSettingsSelectedIndex) return;
  
  // Deselect old item
  if (lastSettingsSelectedIndex >= 0 && lastSettingsSelectedIndex < SETTINGS_COUNT) {
    drawSettingsMenuItem(lastSettingsSelectedIndex, false);
  }
  
  // Select new item
  if (settingsSelectedIndex >= 0 && settingsSelectedIndex < SETTINGS_COUNT) {
    drawSettingsMenuItem(settingsSelectedIndex, true);
  }
  
  lastSettingsSelectedIndex = settingsSelectedIndex;
}

// ===== Drawing Functions =====

// Draw main settings menu
inline void drawSettingsMenu() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();
  
  // Title
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Settings");
  
  const int startY = SMALL_HEADER_HEIGHT + 50;
  const int itemH = 40;
  
  for (int i = 0; i < SETTINGS_COUNT; i++) {
    drawSettingsMenuItem(i, i == settingsSelectedIndex);
  }
  
  lastSettingsSelectedIndex = settingsSelectedIndex;
}

// Draw a single Bluetooth device item
inline void drawBluetoothDeviceItem(int displayIndex, int devIndex, bool selected) {
  const int listStartY = SMALL_HEADER_HEIGHT + 55;
  int y = listStartY + (displayIndex * BT_ITEM_HEIGHT);
  
  // Draw/clear selection highlight
  if (selected) {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, BT_ITEM_HEIGHT - 5, 8, COLOR_PANEL);
    tft.fillCircle(PADDING * 3 + 5, y + 10, 4, COLOR_ORANGE);
  } else {
    tft.fillRoundRect(PADDING * 2, y - 3, 300, BT_ITEM_HEIGHT - 5, 8, COLOR_BG);
  }
  
  // Draw device name
  tft.setTextSize(2);
  tft.setTextColor(selected ? COLOR_TEXT : COLOR_GREY);
  tft.setCursor(PADDING * 5, y);
  
  char displayName[18];
  strncpy(displayName, btDevices[devIndex].name, 17);
  displayName[17] = '\0';
  tft.print(displayName);
  
  // Draw status and signal strength
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 5, y + 16);
  
  // Paired status
  if (btDevices[devIndex].paired) {
    tft.setTextColor(COLOR_ORANGE);
    tft.print("Connected");
  } else {
    // Signal strength
    int rssi = btDevices[devIndex].rssi;
    if (rssi > -50) {
      tft.print("Strong");
    } else if (rssi > -70) {
      tft.print("Medium");
    } else {
      tft.print("Weak");
    }
  }
  
  // Draw signal icon
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
  int scrollBarY = listStartY;
  int scrollBarWidth = 4;
  int scrollBarX = 310;
  
  tft.fillRoundRect(scrollBarX, scrollBarY, scrollBarWidth, scrollBarHeight, 2, COLOR_GREY);
  
  float thumbRatio = (float)BT_DEVICES_PER_PAGE / (float)btDeviceCount;
  int thumbHeight = (int)(scrollBarHeight * thumbRatio);
  if (thumbHeight < 10) thumbHeight = 10;
  
  float scrollRatio = (float)btScrollOffset / (float)(btDeviceCount - BT_DEVICES_PER_PAGE);
  int thumbY = scrollBarY + (int)((scrollBarHeight - thumbHeight) * scrollRatio);
  
  tft.fillRoundRect(scrollBarX, thumbY, scrollBarWidth, thumbHeight, 2, COLOR_ORANGE);
}

// Partial update for Bluetooth device list
inline void updateBluetoothSelection() {
  // Check if we scrolled
  bool scrolled = (btScrollOffset != lastBtScrollOffset);
  
  if (scrolled) {
    // Full redraw of list area only
    const int listStartY = SMALL_HEADER_HEIGHT + 55;
    tft.fillRect(0, listStartY, 320, 185, COLOR_BG);
    
    // Redraw all visible devices
    for (int i = 0; i < BT_DEVICES_PER_PAGE && (btScrollOffset + i) < btDeviceCount; i++) {
      int devIndex = btScrollOffset + i;
      drawBluetoothDeviceItem(i, devIndex, devIndex == btSelectedIndex);
    }
    
    updateBluetoothScrollIndicator();
  } else {
    // Just selection changed - update only 2 items
    int oldDisplayIndex = lastBtSelectedIndex - lastBtScrollOffset;
    int newDisplayIndex = btSelectedIndex - btScrollOffset;
    
    // Deselect old if visible
    if (oldDisplayIndex >= 0 && oldDisplayIndex < BT_DEVICES_PER_PAGE && 
        lastBtSelectedIndex >= 0 && lastBtSelectedIndex < btDeviceCount) {
      drawBluetoothDeviceItem(oldDisplayIndex, lastBtSelectedIndex, false);
    }
    
    // Select new if visible
    if (newDisplayIndex >= 0 && newDisplayIndex < BT_DEVICES_PER_PAGE && 
        btSelectedIndex >= 0 && btSelectedIndex < btDeviceCount) {
      drawBluetoothDeviceItem(newDisplayIndex, btSelectedIndex, true);
    }
  }
  
  lastBtSelectedIndex = btSelectedIndex;
  lastBtScrollOffset = btScrollOffset;
}

// Draw Bluetooth device list
inline void drawBluetoothScreen() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();
  
  // Title
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 10);
  tft.print("Bluetooth");
  
  // Status
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY);
  tft.setCursor(PADDING * 3, SMALL_HEADER_HEIGHT + 30);
  if (btScanning) {
    tft.print("Scanning...");
  } else {
    tft.printf("%d devices found", btDeviceCount);
  }
  
  // Scan button hint
  tft.setCursor(200, SMALL_HEADER_HEIGHT + 30);
  tft.setTextColor(COLOR_ORANGE);
  tft.print("Press SELECT to scan");
  
  if (btDeviceCount == 0 && !btScanning) {
    // No devices found
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(2);
    tft.setCursor(60, 120);
    tft.print("No devices");
    tft.setTextSize(1);
    tft.setCursor(50, 145);
    tft.print("Press SELECT to scan");
    return;
  }
  
  // Draw device list
  for (int i = 0; i < BT_DEVICES_PER_PAGE && (btScrollOffset + i) < btDeviceCount; i++) {
    int devIndex = btScrollOffset + i;
    drawBluetoothDeviceItem(i, devIndex, devIndex == btSelectedIndex);
  }
  
  // Draw scroll indicator
  updateBluetoothScrollIndicator();
  
  // Update tracking
  lastBtSelectedIndex = btSelectedIndex;
  lastBtScrollOffset = btScrollOffset;
}

// ===== Navigation Handlers =====

// Handle navigation in settings menu
inline void handleSettingsNav(NavEvent e) {
  if (e == NAV_UP || e == NAV_LEFT) {
    settingsSelectedIndex--;
    if (settingsSelectedIndex < 0) settingsSelectedIndex = SETTINGS_COUNT - 1;
  }
  else if (e == NAV_DOWN || e == NAV_RIGHT) {
    settingsSelectedIndex++;
    if (settingsSelectedIndex >= SETTINGS_COUNT) settingsSelectedIndex = 0;
  }
}

// Handle navigation in Bluetooth menu
inline void handleBluetoothNav(NavEvent e) {
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
  }
}

// Handle selecting a Bluetooth device
inline void selectBluetoothDevice() {
  if (btDeviceCount == 0 || btSelectedIndex >= btDeviceCount) return;
  
  if (btDevices[btSelectedIndex].paired) {
    disconnectBluetoothDevice(btSelectedIndex);
  } else {
    connectBluetoothDevice(btSelectedIndex);
  }
}
