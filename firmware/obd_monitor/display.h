#pragma once
#include <TFT_eSPI.h>
#include "alerts.h"

// =====================================================================
//  TFT screen: header (mode + WiFi icon), alert strip, 4 pages, tab bar
// =====================================================================

TFT_eSPI tft = TFT_eSPI();

#define C_BG      0x0841
#define C_HEADER  0x10A2
#define C_PANEL   0x18E3
#define C_TEXT    0xFFFF
#define C_LABEL   0x9CD3
#define C_ACCENT  0x05FF
#define C_OK      0x07E0
#define C_WARN    0xFD20
#define C_BAD     0xF800
#define C_DARKRED 0x8000
#define C_DIM     0x39E7
#define C_TAB_BG  0x2104
#define C_TAB_SEL 0x041F
#define C_DEMO    0xA01F   // purple = fake demo data
#define C_STALE   0x6B4D   // grey   = old value (connection lost)

// LANDSCAPE 320 x 240
#define SW        320
#define SH        240
#define Y_ALERT   28
#define H_ALERT   20
#define Y_CONTENT 48
#define Y_TABS    212
#define H_TABS    28
#define H_CONTENT (Y_TABS - Y_CONTENT)

enum Page { PG_DASH, PG_FAV, PG_ALERTS, PG_INFO, PAGE_COUNT };
const char* pageNames[] = {"DASH", "FAV", "ALERTS", "WIFI"};
Page currentPage = PG_DASH;
bool needsFullRedraw = true;

String trunc(const String& s, unsigned int n) { return s.length() > n ? s.substring(0, n) : s; }

// Value text for the screen: when the connection is lost, keep showing the last real value
String dispValue(uint8_t pid) {
  if (linkLost && pidData[pid].hasValue) {
    float v = pidData[pid].value;
    return String(v, (unsigned int)pidDecimals(pid, v));
  }
  return fmtValue(pid);
}
bool hasDispValue(uint8_t pid) { return isFresh(pid) || (linkLost && pidData[pid].hasValue); }

// Colour for numbers: purple in DEMO, grey when the connection is lost
uint16_t vc(uint16_t normal) {
  if (demoMode) return C_DEMO;
  if (linkLost) return C_STALE;
  return normal;
}

// defined in touchui.h
void infoStatic();
bool infoPageTouch(uint16_t x, uint16_t y);

void splash(const char* msg) {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("OBD-II Monitor", SW / 2, 100, 4);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString(msg, SW / 2, 140, 2);
}

// ---------------- header ----------------
int screenMode() { return demoMode ? 0 : (linkLost ? 2 : 1); }   // 0 demo, 1 live, 2 lost

void drawModeBadge() {
  int m = screenMode();
  uint16_t bg = m == 0 ? C_DEMO : (m == 1 ? C_OK : C_BAD);
  tft.fillRoundRect(84, 6, 46, 17, 4, bg);
  tft.setTextColor(m == 1 ? TFT_BLACK : C_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(m == 0 ? "DEMO" : (m == 1 ? "LIVE" : "LOST"), 107, 15, 2);
}

// Purple side frame while showing fake data
void drawDemoFrame() {
  uint16_t c = demoMode ? C_DEMO : C_BG;
  tft.fillRect(0, Y_CONTENT, 3, H_CONTENT, c);
  tft.fillRect(SW - 3, Y_CONTENT, 3, H_CONTENT, c);
}

// Rotating dots: keeps turning as long as the program is running
#define SPIN_X 145
#define SPIN_Y 14
void drawSpinner(bool force) {
  static int lastStep = -1;
  int step = (millis() / 90) % 8;
  if (step == lastStep && !force) return;
  lastStep = step;
  for (int i = 0; i < 8; i++) {
    int age = (step - i + 8) % 8;                 // 0 = newest
    uint16_t c = age == 0 ? C_ACCENT : (age == 1 ? 0x03EF : (age == 2 ? 0x022A : C_DIM));
    float a = i * 0.7854f - 1.5708f;
    tft.fillCircle(SPIN_X + (int)roundf(7 * cosf(a)), SPIN_Y + (int)roundf(7 * sinf(a)), 1, c);
  }
}

// Floppy-disk icon: lit while saving to flash, grey when idle
#define SAVE_X 158
void drawSaveIcon(bool force) {
  static int last = -1;
  int on = (int32_t)(saveIconUntil - millis()) > 0 ? 1 : 0;
  if (on == last && !force) return;
  last = on;
  uint16_t c = on ? C_WARN : C_DIM;
  int x = SAVE_X, y = 6;
  tft.fillRect(x - 1, y - 1, 18, 18, C_HEADER);
  tft.fillRoundRect(x, y, 16, 16, 2, c);              // body
  tft.fillRect(x + 4, y + 1, 7, 5, C_HEADER);         // metal shutter (cut-out)
  tft.fillRect(x + 8, y + 2, 2, 3, c);
  tft.fillRect(x + 3, y + 9, 10, 6, C_HEADER);        // label (cut-out)
}

void thickLine(int x0, int y0, int x1, int y1, uint16_t c) {
  tft.drawLine(x0, y0, x1, y1, c);
  tft.drawLine(x0, y0 + 1, x1, y1 + 1, c);
  tft.drawLine(x0 + 1, y0, x1 + 1, y1, c);
}

void drawWifiIcon(bool force) {
  static int lastKey = -1;
  int frame = (millis() / 300) % 4;
  int key = (int)wifiState * 10 + (wifiState == WS_CONNECTING ? frame : 0);
  if (key == lastKey && !force) return;
  lastKey = key;

  const int o = SW - 240;                         // keep the icon at the right edge
  tft.fillRect(188 + o, 2, 50, 24, C_HEADER);
  uint16_t col = C_DIM;
  int lit = 0;
  if (wifiState == WS_CONNECTED)  { col = C_OK;   lit = 3; }
  if (wifiState == WS_CONNECTING) { col = C_WARN; lit = frame; }
  for (int i = 0; i < 3; i++) {                   // signal bars
    int h = 6 + i * 5;
    tft.fillRect(192 + o + i * 7, 22 - h, 5, h, i < lit ? col : C_DIM);
  }
  if (wifiState == WS_CONNECTED) {                // green check
    thickLine(218 + o, 14, 222 + o, 19, C_OK);
    thickLine(222 + o, 19, 231 + o, 8, C_OK);
  } else if (wifiState == WS_CONNECTING) {        // spinner
    tft.drawCircle(225 + o, 14, 7, C_WARN);
    float a = frame * 1.5708f;
    tft.drawLine(225 + o, 14, 225 + o + (int)(6 * cosf(a)), 14 + (int)(6 * sinf(a)), C_WARN);
  } else {                                        // red X
    thickLine(218 + o, 8, 230 + o, 20, C_BAD);
    thickLine(230 + o, 8, 218 + o, 20, C_BAD);
  }
}

void drawHeader() {
  tft.fillRect(0, 0, SW, 28, C_HEADER);
  tft.setTextColor(C_TEXT, C_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("OBD-II", 6, 15, 4);
  drawModeBadge();
  drawSpinner(true);
  drawSaveIcon(true);
  drawWifiIcon(true);
}

// ---------------- alert strip ----------------
void drawAlertStrip(bool force) {
  static int lastKey = -2;
  static uint32_t lastText = 0;
  int idx = -1;
  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    if (!alertState[i].active) continue;
    if (idx < 0 || (alertState[idx].acked && !alertState[i].acked)) idx = i;
  }
  bool blink = (millis() / 500) % 2;
  int key = idx >= 0 ? idx * 4 + (alertState[idx].acked ? 2 : 0) + (alertState[idx].acked ? 0 : (blink ? 1 : 0))
          : (linkLost ? -3 : (demoMode ? -4 : -1));
  bool textDue = (idx >= 0 || linkLost) && millis() - lastText > 1000;
  if (key == lastKey && !force && !textDue) return;
  lastKey = key;
  lastText = millis();

  if (idx < 0) {
    tft.setTextDatum(MC_DATUM);
    if (linkLost) {                                   // car stopped answering
      tft.fillRect(0, Y_ALERT, SW, H_ALERT, C_BAD);
      tft.setTextColor(C_TEXT);
      uint32_t s = (millis() - lostSince) / 1000;
      String t = s < 60 ? String(s) + "s" : (s < 3600 ? String(s / 60) + "m" : String(s / 3600) + "h");
      tft.drawString("CONNECTION LOST - " + t + " ago", SW / 2, Y_ALERT + H_ALERT / 2, 2);
    } else if (demoMode) {                            // fake data
      tft.fillRect(0, Y_ALERT, SW, H_ALERT, C_DEMO);
      tft.setTextColor(C_TEXT);
      tft.drawString("DEMO DATA - NOT REAL", SW / 2, Y_ALERT + H_ALERT / 2, 2);
    } else {
      tft.fillRect(0, Y_ALERT, SW, H_ALERT, C_BG);
    }
    return;
  }
  const Rule& r = cfg.rules[idx];
  bool acked = alertState[idx].acked;
  uint16_t bg = acked ? C_WARN : (blink ? C_BAD : C_DARKRED);
  tft.fillRect(0, Y_ALERT, SW, H_ALERT, bg);
  String s = "! " + trunc(pidName(r.pid), 20) + " " + dispValue(r.pid) + (r.above ? ">" : "<") + fmtThreshold(r.threshold);
  int more = activeAlertCount() - 1;
  if (more > 0) s += " +" + String(more);
  if (acked) s += " MUTED";
  tft.setTextColor(acked ? TFT_BLACK : C_TEXT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(s, 4, Y_ALERT + H_ALERT / 2, 2);
}

// ---------------- tab bar ----------------
void drawTabBar() {
  int w = SW / PAGE_COUNT;
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < PAGE_COUNT; i++) {
    bool sel = i == currentPage;
    tft.fillRect(i * w, Y_TABS, w, H_TABS, sel ? C_TAB_SEL : C_TAB_BG);
    tft.drawFastVLine(i * w, Y_TABS, H_TABS, C_BG);
    tft.setTextColor(sel ? C_TEXT : C_LABEL);
    tft.drawString(pageNames[i], i * w + w / 2, Y_TABS + H_TABS / 2, 2);
  }
}

// ---------------- helpers ----------------
void boxFrame(int x, int y, int w, int h, const String& label, const char* unit) {
  tft.fillRoundRect(x, y, w, h, 6, C_PANEL);
  tft.setTextColor(C_LABEL, C_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, x + 6, y + 4, 2);
  if (unit && *unit) {
    tft.setTextDatum(TR_DATUM);
    tft.drawString(unit, x + w - 6, y + 4, 2);
  }
}

void boxValue(int x, int y, int w, int h, const String& v, uint16_t col) {
  tft.setTextColor(col, C_PANEL);
  tft.setTextDatum(BR_DATUM);
  tft.setTextPadding(w - 12);
  tft.drawString(v, x + w - 6, y + h - 4, 4);
  tft.setTextPadding(0);
}

void drawBar(int x, int y, int w, int h, float frac, uint16_t col) {
  frac = clampf(frac, 0, 1);
  int fw = (int)(frac * w);
  if (fw > 0) tft.fillRect(x, y, fw, h, col);
  if (fw < w) tft.fillRect(x + fw, y, w - fw, h, C_PANEL);
}

uint16_t tempColor(uint8_t pid) {
  if (demoMode) return C_DEMO;
  if (linkLost) return C_STALE;
  if (!isFresh(pid)) return C_LABEL;
  float v = pidData[pid].value;
  if (v >= 105) return C_BAD;
  if (v >= 98)  return C_WARN;
  if (v < 60)   return C_ACCENT;
  return C_OK;
}

// ---------------- DASH page ----------------
void dashStatic() {
  tft.setTextColor(C_LABEL, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("RPM", 4, 54, 1);      // small font: a 4-digit RPM in font 7 starts at x=28
  tft.drawString("km/h", 4, 152, 1);
  boxFrame(164, 52,  74, 78, "COOL", "C");
  boxFrame(242, 52,  74, 78, "THR", "%");
  boxFrame(164, 132, 74, 78, "LOAD", "%");
  boxFrame(242, 132, 74, 78, "BATT", "V");
}

void dashDynamic() {
  tft.setTextColor(vc(C_TEXT), C_BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(128);
  tft.drawString(hasDispValue(0x0C) ? String((int)pidData[0x0C].value) : "--", 156, 52, 7);
  tft.drawString(hasDispValue(0x0D) ? String((int)pidData[0x0D].value) : "--", 156, 150, 7);
  tft.setTextPadding(0);

  float rpm = hasDispValue(0x0C) ? pidData[0x0C].value : 0;
  drawBar(6, 108, 150, 12, rpm / 7000.0f, vc(rpm > 5500 ? C_BAD : (rpm > 4000 ? C_WARN : C_ACCENT)));

  boxValue(164, 52,  74, 78, dispValue(0x05), tempColor(0x05));
  boxValue(242, 52,  74, 78, dispValue(0x11), vc(C_TEXT));
  boxValue(164, 132, 74, 78, dispValue(0x04), vc(C_TEXT));
  float volt = hasDispValue(0x42) ? pidData[0x42].value : 0;
  boxValue(242, 132, 74, 78, dispValue(0x42), vc((volt > 0 && volt < 12.0f) ? C_WARN : C_TEXT));
}

// ---------------- FAV page ----------------
#define FAV_W 100
#define FAV_H 78
void favBox(uint8_t i, int& x, int& y) { x = 6 + (i % 3) * 104; y = 52 + (i / 3) * 80; }

void favStatic() {
  if (cfg.favCount == 0) {
    tft.setTextColor(C_LABEL, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No favorites yet", SW / 2, 100, 4);
    tft.drawString("Connect to the hotspot and", SW / 2, 140, 2);
    tft.drawString("open http://192.168.4.1", SW / 2, 160, 2);
    return;
  }
  for (uint8_t i = 0; i < cfg.favCount; i++) {
    int x, y;
    favBox(i, x, y);
    boxFrame(x, y, FAV_W, FAV_H, trunc(pidName(cfg.favs[i]), 9), pidUnit(cfg.favs[i]));
  }
}

void favDynamic() {
  for (uint8_t i = 0; i < cfg.favCount; i++) {
    int x, y;
    favBox(i, x, y);
    uint8_t p = cfg.favs[i];
    bool isTemp = findPid(p) && findPid(p)->f == F_A_M40;
    boxValue(x, y, FAV_W, FAV_H, dispValue(p), isTemp ? tempColor(p) : vc(C_TEXT));
  }
}

// ---------------- ALERTS page ----------------
void alertsStatic() {
  if (cfg.ruleCount == 0) {
    tft.setTextColor(C_LABEL, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No alert rules", SW / 2, 110, 4);
    tft.drawString("Add them in the web panel", SW / 2, 150, 2);
    return;
  }
  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    int y = 50 + i * 20;
    const Rule& r = cfg.rules[i];
    tft.fillRoundRect(5, y, SW - 10, 19, 3, C_PANEL);
    tft.setTextColor(C_TEXT, C_PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(trunc(pidName(r.pid), 18), 24, y + 10, 2);
    tft.setTextColor(C_LABEL, C_PANEL);
    tft.drawString(String(r.above ? "> " : "< ") + fmtThreshold(r.threshold), 170, y + 10, 2);
  }
}

void alertsDynamic() {
  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    int y = 50 + i * 20;
    const AlertState& a = alertState[i];
    uint16_t c = a.active ? (a.acked ? C_WARN : C_BAD) : (isFresh(cfg.rules[i].pid) ? C_OK : C_DIM);
    tft.fillCircle(14, y + 9, 5, c);
    tft.setTextColor(a.active ? c : vc(C_TEXT), C_PANEL);
    tft.setTextDatum(MR_DATUM);
    tft.setTextPadding(60);
    tft.drawString(dispValue(cfg.rules[i].pid), SW - 10, y + 10, 2);
    tft.setTextPadding(0);
  }
}

// ---------------- WIFI / INFO page ----------------
void infoLine(int i, const String& s, uint16_t col) {
  tft.setTextColor(col, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(214);
  tft.drawString(trunc(s, 29), 6, 50 + i * 18, 2);
  tft.setTextPadding(0);
}

void infoDynamic() {
  infoLine(0, "Hotspot: " + cfg.apSsid, C_TEXT);
  infoLine(1, "Password: " + (cfg.apPass.length() >= 8 ? cfg.apPass : String("(open)")), C_LABEL);
  infoLine(2, "Panel: http://192.168.4.1", C_ACCENT);

  String ws = "WiFi: " + (cfg.wifiSsid.length() ? cfg.wifiSsid : String("(not set)"));
  infoLine(3, ws, C_TEXT);
  switch (wifiState) {
    case WS_CONNECTED:  infoLine(4, "Connected  " + String(WiFi.RSSI()) + " dBm", C_OK); break;
    case WS_CONNECTING: infoLine(4, "Connecting...", C_WARN); break;
    case WS_FAILED:     infoLine(4, "Failed - retrying soon", C_BAD); break;
    default:            infoLine(4, "Set WiFi in the web panel", C_LABEL); break;
  }
  infoLine(5, wifiState == WS_CONNECTED ? "IP: " + WiFi.localIP().toString() : String("IP: -"), C_LABEL);

  uint32_t ep = nowEpoch();
  if (ep) {
    time_t t = ep;
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[24];
    strftime(buf, sizeof(buf), "%H:%M:%S  %d/%m/%Y", &tmv);
    infoLine(6, String("Time: ") + buf, C_LABEL);
  } else {
    infoLine(6, "Time: not synced", C_LABEL);
  }

  if (!cfg.streamUrl.length()) infoLine(7, "Stream: no URL set", C_LABEL);
  else if (lastHttpCode == 0) infoLine(7, "Stream: waiting", C_LABEL);
  else if (lastHttpCode >= 200 && lastHttpCode < 300)
    infoLine(7, "Stream: OK " + String((millis() - lastUploadOk) / 1000) + "s ago", C_OK);
  else infoLine(7, "Stream: error " + String(lastHttpCode), C_BAD);

  uint32_t pend = logPendingBytes();
  infoLine(8, logFull ? String("Log: FULL - not saving") : "Log waiting: " + String(pend / 1024.0f, 1) + " KB",
           logFull ? C_BAD : C_LABEL);
}

// ---------------- page control ----------------
void drawPageStatic() {
  switch (currentPage) {
    case PG_DASH:   dashStatic(); break;
    case PG_FAV:    favStatic(); break;
    case PG_ALERTS: alertsStatic(); break;
    case PG_INFO:   infoStatic(); break;
    default: break;
  }
}

void drawPageDynamic() {
  switch (currentPage) {
    case PG_DASH:   dashDynamic(); break;
    case PG_FAV:    favDynamic(); break;
    case PG_ALERTS: alertsDynamic(); break;
    case PG_INFO:   infoDynamic(); break;
    default: break;
  }
}

void nextPage() {
  currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
  needsFullRedraw = true;
  Serial.printf("Page -> %s\n", pageNames[currentPage]);
}

// Re-start the display controller and redraw everything.
// Recovers a white / garbled screen caused by electrical noise without restarting the ESP32.
volatile bool repaintRequest = false;
void repaintDisplay() {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(C_BG);
  needsFullRedraw = true;
}

void displayLoop() {
  static uint32_t lastRepaint = 0;
  if (repaintRequest || (cfg.repaintSec && millis() - lastRepaint >= (uint32_t)cfg.repaintSec * 1000UL)) {
    if (repaintRequest) Serial.println("Display repaint (web panel)");
    repaintRequest = false;
    lastRepaint = millis();
    repaintDisplay();
  }
  static uint32_t lastUpd = 0;
  static int lastMode = -1;
  if (needsFullRedraw) {
    needsFullRedraw = false;
    drawHeader();
    tft.fillRect(0, Y_CONTENT, SW, H_CONTENT, C_BG);
    drawTabBar();
    drawPageStatic();
    drawAlertStrip(true);
    drawDemoFrame();
    lastUpd = 0;
    lastMode = screenMode();
  }
  if (screenMode() != lastMode) {
    lastMode = screenMode();
    drawModeBadge();
    drawDemoFrame();
    drawAlertStrip(true);
    lastUpd = 0;
  }
  drawSpinner(false);
  drawSaveIcon(false);
  drawWifiIcon(false);
  drawAlertStrip(false);

  uint32_t interval = currentPage == PG_INFO ? 1000 : 200;
  if (lastUpd == 0 || millis() - lastUpd >= interval) {
    lastUpd = millis();
    drawPageDynamic();
  }
}

// ---------------- touch input (own mapping, works in any screen rotation) ----------------
// Reads the XPT2046 raw values and maps them with our own 3-point calibration.
// Lower pressure threshold than the TFT_eSPI default (600), so light presses work too.
#define TOUCH_Z_MIN 300
#define CAL_INSET   45     // calibration crosses this far from the screen edges (clear of the case frame)

struct TouchCal { uint8_t valid, xAxis; uint16_t xa, xb, ya, yb; };   // raw at the calibration crosses
TouchCal tcal = {0, 1, 300, 3800, 300, 3800};                         // guess until calibrated
volatile bool touchCalRequest = false;                                 // set by the web panel
uint16_t lastRawX = 0, lastRawY = 0, lastRawZ = 0;
int16_t  lastTouchX = -1, lastTouchY = -1;

void loadTouchCal() {
  TouchCal c;
  if (prefs.getBytesLength("tcal4") == sizeof(c)) {
    prefs.getBytes("tcal4", &c, sizeof(c));
    if (c.valid == 1) tcal = c;
  }
  Serial.printf("Touch calibration: %s (axis %u, x %u..%u, y %u..%u)\n",
                tcal.valid ? "saved" : "NOT calibrated - using a guess", tcal.xAxis, tcal.xa, tcal.xb, tcal.ya, tcal.yb);
}

// Averaged raw reading. false = not pressed
bool touchRaw(uint16_t& rx, uint16_t& ry, uint16_t& z) {
  z = tft.getTouchRawZ();
  lastRawZ = z;
  if (z < TOUCH_Z_MIN) return false;
  uint32_t sx = 0, sy = 0;
  uint8_t n = 0;
  for (uint8_t i = 0; i < 4; i++) {
    uint16_t a, b;
    tft.getTouchRaw(&a, &b);
    if (a > 50 && a < 4050 && b > 50 && b < 4050) { sx += a; sy += b; n++; }
  }
  if (!n || tft.getTouchRawZ() < TOUCH_Z_MIN) return false;   // released while reading
  rx = sx / n; ry = sy / n;
  lastRawX = rx; lastRawY = ry;
  return true;
}

long mapAxis(uint16_t raw, uint16_t r0, uint16_t r1, int s0, int s1) {
  if (r1 == r0) return s0;
  return s0 + ((long)raw - r0) * (s1 - s0) / ((long)r1 - r0);
}

// Screen position of the current press. false = not pressed
bool touchStuck = false;      // pressed non-stop (case lid pressing the screen?) -> ignored
bool readTouch(uint16_t& x, uint16_t& y) {
  if (!cfg.touchOn) return false;                  // touch switched off in the web panel
  static uint32_t pressStart = 0, releaseAt = 0;
  uint16_t rx, ry, z;
  bool down = touchRaw(rx, ry, z);
  if (!down) {
    pressStart = 0;
    if (touchStuck && !releaseAt) releaseAt = millis();
    if (touchStuck && millis() - releaseAt > 1000) { touchStuck = false; Serial.println("Touch released - touch active again"); }
    return false;
  }
  releaseAt = 0;
  if (!pressStart) pressStart = millis();
  if (!touchStuck && millis() - pressStart > 8000) {
    touchStuck = true;
    Serial.println("Touch pressed non-stop for 8 s - ignoring it (is the case lid pressing the screen?)");
  }
  if (touchStuck) return false;
  uint16_t ax = tcal.xAxis ? ry : rx, ay = tcal.xAxis ? rx : ry;
  long sx = constrain(mapAxis(ax, tcal.xa, tcal.xb, CAL_INSET, SW - CAL_INSET), 0, SW - 1);
  long sy = constrain(mapAxis(ay, tcal.ya, tcal.yb, CAL_INSET, SH - CAL_INSET), 0, SH - 1);
  x = sx; y = sy;
  lastTouchX = sx; lastTouchY = sy;
  static uint32_t lastLog = 0;
  if (millis() - lastLog > 250) {
    lastLog = millis();
    Serial.printf("Touch raw x=%u y=%u z=%u -> screen %ld,%ld\n", rx, ry, z, sx, sy);
  }
  return true;
}

// ---------------- touch ----------------
void handleTouch() {
  uint16_t tx, ty;
  if (!readTouch(tx, ty)) return;
  static uint32_t lastTouch = 0;
  if (millis() - lastTouch < 300) return;
  lastTouch = millis();

  if (infoPageTouch(tx, ty)) return;
  if (ty >= Y_TABS) {
    int p = tx / (SW / PAGE_COUNT);
    if (p >= 0 && p < PAGE_COUNT && p != currentPage) {
      currentPage = (Page)p;
      needsFullRedraw = true;
    }
  } else if (ty >= Y_ALERT && ty < Y_ALERT + H_ALERT + 10 && activeAlertCount() > 0) {
    ackAlerts();                       // tap the red strip to silence
  }
}
