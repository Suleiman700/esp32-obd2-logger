#pragma once
#include "display.h"

// =====================================================================
//  On-screen WiFi setup: scan list + keyboard, and touch calibration
// =====================================================================

enum UiMode { UI_NORMAL, UI_SCAN, UI_KEYBOARD };
UiMode uiMode = UI_NORMAL;

#define MAX_SCAN 6
String   scanSsid[MAX_SCAN];
int      scanRssi[MAX_SCAN];
bool     scanOpen[MAX_SCAN];
int      scanCount = 0;
bool     scanning = false;

String   kbSsid, kbText;
uint8_t  kbMode = 0;            // 0 = abc, 1 = ABC, 2 = 123/symbols

enum KeySpecial : uint8_t { K_CHAR, K_SHIFT, K_MODE, K_DEL, K_SPACE, K_OK, K_CANCEL };
struct Key { int16_t x, y, w, h; char c; KeySpecial s; };
Key     keys[48];
uint8_t keyCount = 0;

#define KB_TOP 70      // landscape: 5 rows x 34 px = bottom of the screen
#define KB_ROW 34

// ---------------- touch calibration ----------------
void loadTouchCal() {
  uint16_t cal[5] = {300, 3800, 300, 3800, 1};
  if (prefs.getBytesLength("tcal_l") == sizeof(cal)) prefs.getBytes("tcal_l", cal, sizeof(cal));
  tft.setTouch(cal);
}

// true if the touch chip reports a press within timeoutMs (button press = cancel)
bool waitForAnyTouch(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    uint16_t z = tft.getTouchRawZ();
    if (z > 350) return true;
    if (digitalRead(BUTTON_PIN) == LOW && millis() - start > 500) return false;
    delay(20);
  }
  return false;
}

void runTouchCalibration() {
  uint16_t cal[5];
  tft.fillScreen(C_BG);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch calibration", SW / 2, 90, 4);
  tft.drawString("Tap anywhere on the screen to start", SW / 2, 130, 2);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString("(push button = cancel)", SW / 2, 155, 2);

  if (!waitForAnyTouch(8000)) {
    Serial.printf("Touch not detected (raw Z=%u) - check T_CS/T_CLK/T_DIN/T_DO wiring\n", tft.getTouchRawZ());
    tft.fillScreen(C_BG);
    tft.setTextColor(C_BAD, C_BG);
    tft.drawString("Touch not detected", SW / 2, 100, 4);
    tft.setTextColor(C_LABEL, C_BG);
    tft.drawString("Check touch wiring", SW / 2, 135, 2);
    delay(2500);
    needsFullRedraw = true;
    return;
  }
  while (tft.getTouchRawZ() > 350) delay(20);   // wait for finger up

  tft.fillScreen(C_BG);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Touch the arrow corners", SW / 2, 110, 2);
  tft.drawString("as they appear", SW / 2, 130, 2);
  tft.calibrateTouch(cal, C_ACCENT, C_BG, 15);
  {
    CfgLock l;
    markSaving();
    prefs.putBytes("tcal_l", cal, sizeof(cal));
  }
  tft.setTouch(cal);
  Serial.printf("Touch calibration saved: %u %u %u %u %u\n", cal[0], cal[1], cal[2], cal[3], cal[4]);
  tft.fillScreen(C_BG);
  tft.drawString("Calibration saved", SW / 2, 120, 4);
  delay(1000);
  needsFullRedraw = true;
}

// Touch that fires once per press (not repeated while held)
bool touchWasDown = false;   // set true when a screen opens so the opening tap is ignored
bool touchPressed(uint16_t& x, uint16_t& y) {
  static uint32_t lastChange = 0;
  bool down = tft.getTouch(&x, &y);
  if (down == touchWasDown || millis() - lastChange < 60) return false;
  touchWasDown = down;
  lastChange = millis();
  return down;
}

// ---------------- buttons ----------------
void drawButton(int x, int y, int w, int h, const char* label, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(x, y, w, h, 6, bg);
  tft.setTextColor(fg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}

bool inRect(uint16_t x, uint16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// Buttons on the WIFI page (drawn by infoStatic)
#define BTN_X      226
#define BTN_W      90
#define BTN_WIFI_Y 52
#define BTN_WIFI_H 46
#define BTN_CAL_Y  104
#define BTN_CAL_H  38

void infoStatic() {
  drawButton(BTN_X, BTN_WIFI_Y, BTN_W, BTN_WIFI_H, "Change WiFi", C_TAB_SEL, C_TEXT);
  drawButton(BTN_X, BTN_CAL_Y, BTN_W, BTN_CAL_H, "Touch cal.", C_PANEL, C_TEXT);
}

// ---------------- scan screen ----------------
void drawScanScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, SW, 32, C_HEADER);
  tft.setTextColor(C_TEXT, C_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Choose WiFi", 8, 16, 4);

  if (scanning) {
    tft.setTextColor(C_LABEL, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Scanning...", SW / 2, 120, 4);
  } else if (scanCount == 0) {
    tft.setTextColor(C_LABEL, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No networks found", SW / 2, 120, 2);
  } else {
    for (int i = 0; i < scanCount; i++) {
      int y = 36 + i * 28;
      bool current = scanSsid[i] == cfg.wifiSsid;
      tft.fillRoundRect(4, y, SW - 8, 26, 5, current ? C_TAB_SEL : C_PANEL);
      tft.setTextColor(C_TEXT);
      tft.setTextDatum(ML_DATUM);
      tft.drawString(trunc(scanSsid[i], 28), 12, y + 13, 2);
      int bars = scanRssi[i] > -60 ? 3 : (scanRssi[i] > -75 ? 2 : 1);
      for (int b = 0; b < 3; b++) {
        int h = 4 + b * 4;
        tft.fillRect(SW - 36 + b * 6, y + 20 - h, 4, h, b < bars ? C_OK : C_DIM);
      }
      if (!scanOpen[i]) tft.drawString("*", SW - 52, y + 13, 2);   // has password
    }
  }
  drawButton(4, 206, 154, 30, "Rescan", C_PANEL, C_TEXT);
  drawButton(162, 206, 154, 30, "Cancel", C_PANEL, C_TEXT);
}

void startScan() {
  uiMode = UI_SCAN;
  touchWasDown = true;
  scanning = true;
  scanCount = 0;
  WiFi.scanDelete();
  WiFi.scanNetworks(true);          // async, doesn't block the rest
  drawScanScreen();
}

void pollScan() {
  if (!scanning) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanning = false;
  scanCount = 0;
  if (n > 0) {
    // keep the strongest unique networks
    for (int k = 0; k < n && scanCount < MAX_SCAN; k++) {
      int best = -1;
      for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        if (!s.length()) continue;
        bool used = false;
        for (int j = 0; j < scanCount; j++) if (scanSsid[j] == s) used = true;
        if (used) continue;
        if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
      }
      if (best < 0) break;
      scanSsid[scanCount] = WiFi.SSID(best);
      scanRssi[scanCount] = WiFi.RSSI(best);
      scanOpen[scanCount] = WiFi.encryptionType(best) == WIFI_AUTH_OPEN;
      scanCount++;
    }
  }
  WiFi.scanDelete();
  drawScanScreen();
}

// ---------------- keyboard ----------------
void addKey(int x, int y, int w, char c, KeySpecial s = K_CHAR) {
  if (keyCount < sizeof(keys) / sizeof(keys[0])) keys[keyCount++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)KB_ROW, c, s};
}

void addRow(const char* chars, int y, int xStart, int w) {
  for (int i = 0; chars[i]; i++) addKey(xStart + i * w, y, w, chars[i]);
}

void buildKeys() {
  keyCount = 0;
  const char *r0, *r1, *r2, *r3;
  if (kbMode == 2) { r0 = "1234567890"; r1 = "!@#$%^&*()"; r2 = "-_=+[]{};"; r3 = ":'\",.?/"; }
  else if (kbMode == 1) { r0 = "1234567890"; r1 = "QWERTYUIOP"; r2 = "ASDFGHJKL"; r3 = "ZXCVBNM"; }
  else { r0 = "1234567890"; r1 = "qwertyuiop"; r2 = "asdfghjkl"; r3 = "zxcvbnm"; }
  addRow(r0, KB_TOP, 0, 32);
  addRow(r1, KB_TOP + KB_ROW, 0, 32);
  addRow(r2, KB_TOP + 2 * KB_ROW, 16, 32);
  addKey(0, KB_TOP + 3 * KB_ROW, 48, 0, K_SHIFT);
  addRow(r3, KB_TOP + 3 * KB_ROW, 48, 32);
  addKey(272, KB_TOP + 3 * KB_ROW, 48, 0, K_DEL);
  addKey(0, KB_TOP + 4 * KB_ROW, 80, 0, K_MODE);
  addKey(80, KB_TOP + 4 * KB_ROW, 140, ' ', K_SPACE);
  addKey(220, KB_TOP + 4 * KB_ROW, 100, 0, K_OK);
}

void drawKey(const Key& k, bool pressed) {
  uint16_t bg = pressed ? C_ACCENT : (k.s == K_OK ? C_TAB_SEL : (k.s == K_CHAR ? C_PANEL : C_TAB_BG));
  tft.fillRoundRect(k.x + 1, k.y + 1, k.w - 2, k.h - 2, 4, bg);
  tft.setTextColor(C_TEXT);
  tft.setTextDatum(MC_DATUM);
  const char* label = nullptr;
  char ch[2] = {k.c, 0};
  switch (k.s) {
    case K_SHIFT: label = kbMode == 1 ? "abc" : "Aa"; break;
    case K_MODE:  label = kbMode == 2 ? "ABC" : "#+="; break;
    case K_DEL:   label = "DEL"; break;
    case K_SPACE: label = "space"; break;
    case K_OK:    label = "Connect"; break;
    default:      label = ch; break;
  }
  tft.drawString(label, k.x + k.w / 2, k.y + k.h / 2, 2);
}

void drawKbField() {
  tft.fillRoundRect(4, 28, SW - 8, 36, 5, C_PANEL);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextDatum(ML_DATUM);
  String shown = kbText.length() > 32 ? kbText.substring(kbText.length() - 32) : kbText;
  tft.drawString(shown + "_", 10, 46, 2);
  tft.setTextColor(C_LABEL, C_PANEL);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(kbText.length()), SW - 10, 46, 2);   // character count
}

void drawKeyboardScreen() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_LABEL, C_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Password for:", 6, 12, 2);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString(trunc(kbSsid, 20), 96, 12, 2);
  drawKbField();
  drawButton(SW - 70, 2, 66, 22, "Cancel", C_TAB_BG, C_TEXT);
  buildKeys();
  for (uint8_t i = 0; i < keyCount; i++) drawKey(keys[i], false);
}

void connectTo(const String& ssid, const String& pass) {
  {
    CfgLock l;
    cfg.wifiSsid = ssid;
    cfg.wifiPass = pass;
    saveSettingsUnlocked();
  }
  startSta();
  uiMode = UI_NORMAL;
  currentPage = PG_INFO;
  needsFullRedraw = true;
}

void openKeyboard(const String& ssid) {
  kbSsid = ssid;
  kbText = ssid == cfg.wifiSsid ? cfg.wifiPass : "";
  kbMode = 0;
  uiMode = UI_KEYBOARD;
  drawKeyboardScreen();
}

void leaveUi() {
  uiMode = UI_NORMAL;
  scanning = false;
  needsFullRedraw = true;
}

// ---------------- touch handling for the overlays ----------------
void scanTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 4, 206, 154, 30)) { startScan(); return; }
  if (inRect(x, y, 162, 206, 154, 30)) { leaveUi(); return; }
  if (scanning) return;
  for (int i = 0; i < scanCount; i++) {
    if (inRect(x, y, 4, 36 + i * 28, SW - 8, 26)) {
      if (scanOpen[i]) connectTo(scanSsid[i], "");
      else openKeyboard(scanSsid[i]);
      return;
    }
  }
}

void keyboardTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, SW - 70, 2, 66, 22)) { leaveUi(); return; }
  for (uint8_t i = 0; i < keyCount; i++) {
    const Key& k = keys[i];
    if (!inRect(x, y, k.x, k.y, k.w, k.h)) continue;
    drawKey(k, true);
    delay(60);
    switch (k.s) {
      case K_CHAR:
      case K_SPACE: if (kbText.length() < 63) kbText += k.c; break;
      case K_DEL:   if (kbText.length()) kbText.remove(kbText.length() - 1); break;
      case K_SHIFT: kbMode = kbMode == 1 ? 0 : 1; drawKeyboardScreen(); return;
      case K_MODE:  kbMode = kbMode == 2 ? 0 : 2; drawKeyboardScreen(); return;
      case K_OK:    connectTo(kbSsid, kbText); return;
      default: break;
    }
    drawKey(k, false);
    drawKbField();
    return;
  }
}

// Called every loop. Returns true while a setup screen is open
// (then the normal display + touch handling is skipped).
bool uiLoop() {
  if (uiMode == UI_NORMAL) return false;
  if (uiMode == UI_SCAN) pollScan();
  uint16_t x, y;
  if (touchPressed(x, y)) {
    if (uiMode == UI_SCAN) scanTouch(x, y);
    else keyboardTouch(x, y);
  }
  return uiMode != UI_NORMAL;
}

// Tap handling for the WIFI page buttons; returns true if a button was hit
bool infoPageTouch(uint16_t x, uint16_t y) {
  if (currentPage != PG_INFO) return false;
  if (inRect(x, y, BTN_X, BTN_WIFI_Y, BTN_W, BTN_WIFI_H)) { startScan(); return true; }
  if (inRect(x, y, BTN_X, BTN_CAL_Y, BTN_W, BTN_CAL_H)) { runTouchCalibration(); return true; }
  return false;
}
