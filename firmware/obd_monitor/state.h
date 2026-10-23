#pragma once
#include <Arduino.h>
#include "config.h"
#include "pids.h"

// =====================================================================
//  Shared state
// =====================================================================

struct PidState {
  float    value;
  uint32_t lastSeen;
  uint8_t  raw[5];
  uint8_t  rawLen;
  bool     hasValue;
  bool     supported;   // reported by the car (LIVE mode)
};
PidState pidData[256];

struct Rule {
  uint8_t pid;
  bool    above;        // true: alert when value > threshold, false: when value < threshold
  float   threshold;
};

struct AlertState {
  bool     active;
  bool     acked;       // silenced with the button / web
  float    value;
  uint32_t since;
};

struct Settings {
  String   wifiSsid, wifiPass;
  String   apSsid, apPass;
  String   streamUrl, alertUrl;
  uint16_t streamInterval;   // seconds
  uint16_t logInterval;      // seconds
  bool     logDemo;
  bool     touchOn;          // false = never talk to the touch chip
  bool     buttonOn;         // false = ignore the push button
  uint16_t repaintSec;       // 0 = off, else re-start + redraw the display every N seconds
  uint16_t cycleSec;         // 0 = off, else switch to the next page every N seconds
  bool     cycleDash;        // auto-cycle: stay on DASH while the car is moving
  uint8_t  tracked[MAX_TRACKED]; uint8_t trackedCount;
  uint8_t  favs[MAX_FAVS];       uint8_t favCount;
  uint8_t  dash[MAX_DASH];       // PID per main-screen slot (0 = empty)
  uint8_t  dashLayout;           // index into DASH_LAYOUTS (display.h)
  uint8_t  dashIcons;            // main screen labels: 0 text, 1 icon, 2 icon + text
  uint32_t labelRgb;             // main screen label / icon colour, 0xRRGGBB
  Rule     rules[MAX_RULES];     uint8_t ruleCount;
};
Settings   cfg;
AlertState alertState[MAX_RULES];

bool     buttonStuck     = false;    // button pin LOW for > 3 s -> ignored
volatile bool otaRunning = false;   // firmware update in progress -> storage task pauses
uint32_t restartAt       = 0;        // reboot after a firmware update
uint32_t buzzerTestUntil = 0;        // web panel "Test buzzer" (millis)
bool     demoMode        = true;    // no car seen since power-on -> fake data
bool     linkLost        = false;   // car was LIVE, then stopped answering -> show last values in grey
uint32_t lostSince       = 0;
bool     canInitialized  = false;
bool     supportKnown    = false;
uint32_t lastCanResponse = 0;
uint32_t bootCount       = 0;

uint8_t  pollList[MAX_POLL];
uint8_t  pollCount = 0;

bool isFresh(uint8_t pid) {
  return pidData[pid].hasValue && (millis() - pidData[pid].lastSeen) < VALUE_FRESH_MS;
}

bool inList(const uint8_t* list, uint8_t n, uint8_t pid) {
  for (uint8_t i = 0; i < n; i++) if (list[i] == pid) return true;
  return false;
}

void addToPoll(uint8_t p) {
  if (inList(pollList, pollCount, p)) return;
  if (pollCount < MAX_POLL) pollList[pollCount++] = p;
}

// PIDs requested from the car = dashboard + logged + favorites + alert PIDs
void rebuildPollList() {
  pollCount = 0;
  const uint8_t core[] = {0x0C, 0x0D, 0x05, 0x11, 0x04, 0x42};
  for (uint8_t p : core) addToPoll(p);
  for (uint8_t i = 0; i < cfg.trackedCount; i++) addToPoll(cfg.tracked[i]);
  for (uint8_t i = 0; i < cfg.favCount; i++)     addToPoll(cfg.favs[i]);
  for (uint8_t i = 0; i < MAX_DASH; i++)         if (cfg.dash[i]) addToPoll(cfg.dash[i]);
  for (uint8_t i = 0; i < cfg.ruleCount; i++)    addToPoll(cfg.rules[i].pid);
}

String fmtValue(uint8_t pid) {
  if (!isFresh(pid)) return "--";
  float v = pidData[pid].value;
  return String(v, (unsigned int)pidDecimals(pid, v));
}

String fmtThreshold(float t) {
  bool whole = fabsf(t - roundf(t)) < 0.001f;
  return String(t, whole ? 0U : 2U);
}
