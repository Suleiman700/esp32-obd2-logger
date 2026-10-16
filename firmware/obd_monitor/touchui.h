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

// ---------------- touch calibration (3 taps) ----------------
void drawCross(int x, int y, uint16_t c) {
  tft.drawLine(x - 12, y, x + 12, y, c);
  tft.drawLine(x, y - 12, x, y + 12, c);
  tft.drawCircle(x, y, 6, c);
}

// Show a cross, wait for a firm tap on it, return the averaged raw position.
// 0 = ok, 1 = no touch (timeout), 2 = cancelled with the push button
int waitTap(int px, int py, uint16_t& rx, uint16_t& ry, uint32_t timeoutMs) {
  uint16_t x, y, z;
  uint32_t t0 = millis();
  while (touchRaw(x, y, z) && millis() - t0 < 3000) delay(10);      // finger up first
  drawCross(px, py, C_ACCENT);
  t0 = millis();
  while (!touchRaw(x, y, z)) {
    if (millis() - t0 > timeoutMs) { drawCross(px, py, C_BG); return 1; }
    if (digitalRead(BUTTON_PIN) == LOW && millis() - t0 > 500) { drawCross(px, py, C_BG); return 2; }
    delay(10);
  }
  uint32_t sx = 0, sy = 0, n = 0;
  t0 = millis();
  while (millis() - t0 < 250) {                                        // average while held
    if (touchRaw(x, y, z)) { sx += x; sy += y; n++; }
    delay(10);
  }
  if (!n) { sx = x; sy = y; n = 1; }
  rx = sx / n; ry = sy / n;
  drawCross(px, py, C_OK);
  t0 = millis();
  while (touchRaw(x, y, z) && millis() - t0 < 3000) delay(10);      // wait for release
  delay(150);
  drawCross(px, py, C_BG);
  return 0;
}

void runTouchCalibration() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch calibration", SW / 2, 95, 4);
  tft.drawString("Press each cross firmly (3 crosses)", SW / 2, 130, 2);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString("push button = cancel", SW / 2, 152, 2);
  Serial.printf("Touch calibration started (raw Z now %u)\n", tft.getTouchRawZ());

  const int px[3] = {20, SW - 20, 20}, py[3] = {20, 20, SH - 20};
  uint16_t rx[3], ry[3];
  for (int i = 0; i < 3; i++) {
    int r = waitTap(px[i], py[i], rx[i], ry[i], 20000);
    if (r) {
      Serial.printf("Touch calibration %s (raw Z=%u)\n", r == 1 ? "timeout - no touch detected" : "cancelled", tft.getTouchRawZ());
      tft.fillScreen(C_BG);
      tft.setTextColor(r == 1 ? C_BAD : C_LABEL, C_BG);
      tft.drawString(r == 1 ? "No touch detected" : "Calibration cancelled", SW / 2, 105, 4);
      if (r == 1) {
        tft.setTextColor(C_LABEL, C_BG);
        tft.drawString("Check T_CS 22, T_CLK 18, T_DIN 23, T_DO 19", SW / 2, 140, 2);
      }
      delay(2500);
      needsFullRedraw = true;
      return;
    }
    Serial.printf("  cross %d at %d,%d -> raw %u,%u\n", i + 1, px[i], py[i], rx[i], ry[i]);
  }

  TouchCal c;
  int dxr = (int)rx[1] - rx[0], dyr = (int)ry[1] - ry[0];          // cross 1 -> 2 moves along screen X
  c.xAxis = abs(dyr) > abs(dxr) ? 1 : 0;
  c.xa = c.xAxis ? ry[0] : rx[0];  c.xb = c.xAxis ? ry[1] : rx[1];
  c.ya = c.xAxis ? rx[0] : ry[0];  c.yb = c.xAxis ? rx[2] : ry[2];
  c.valid = 1;

  tft.fillScreen(C_BG);
  if (abs((int)c.xb - c.xa) < 400 || abs((int)c.yb - c.ya) < 300) {
    Serial.println("Touch calibration failed - crosses too close in raw values, try again");
    tft.setTextColor(C_BAD, C_BG);
    tft.drawString("Calibration failed - try again", SW / 2, 120, 2);
    delay(2000);
    needsFullRedraw = true;
    return;
  }
  tcal = c;
  {
    CfgLock l;
    markSaving();
    prefs.putBytes("tcal3", &c, sizeof(c));
  }
  Serial.printf("Touch calibration saved: axis %u, x %u..%u, y %u..%u\n", c.xAxis, c.xa, c.xb, c.ya, c.yb);
  tft.setTextColor(C_OK, C_BG);
  tft.drawString("Calibration saved", SW / 2, 110, 4);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString("Tap anywhere to test - dot follows your finger", SW / 2, 145, 2);
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {                                       // short test
    uint16_t x, y;
    if (readTouch(x, y)) { tft.fillCircle(x, y, 3, C_ACCENT); t0 = millis() - 2000; }
    delay(15);
  }
  needsFullRedraw = true;
}

// Touch that fires once per press (not repeated while held)
bool touchWasDown = false;   // set true when a screen opens so the opening tap is ignored
bool touchPressed(uint16_t& x, uint16_t& y) {
  static uint32_t lastChange = 0;
  bool down = readTouch(x, y);
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
