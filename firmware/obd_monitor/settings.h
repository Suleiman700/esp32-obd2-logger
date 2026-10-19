#pragma once
#include <Preferences.h>
#include "state.h"

// =====================================================================
//  Settings saved in flash (Preferences / NVS)
// =====================================================================

Preferences prefs;
uint32_t logOffset   = 0;   // bytes of LOG_FILE already uploaded
uint32_t alertOffset = 0;   // bytes of ALERT_FILE already uploaded

String hexList(const uint8_t* a, uint8_t n) {
  String s;
  char b[4];
  for (uint8_t i = 0; i < n; i++) {
    if (i) s += ',';
    snprintf(b, sizeof(b), "%02X", a[i]);
    s += b;
  }
  return s;
}

// "0C,0D, 0x05" -> {0x0C,0x0D,0x05}
uint8_t parseHexList(const String& in, uint8_t* out, uint8_t maxN) {
  uint8_t n = 0;
  int start = 0;
  String s = in + ",";
  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    if (c != ',' && c != ' ' && c != ';') continue;
    String tok = s.substring(start, i);
    start = i + 1;
    tok.trim();
    if (tok.startsWith("0x") || tok.startsWith("0X")) tok = tok.substring(2);
    if (!tok.length()) continue;
    long v = strtol(tok.c_str(), nullptr, 16);
    if (v < 1 || v > 255) continue;
    if (!inList(out, n, (uint8_t)v) && n < maxN) out[n++] = (uint8_t)v;
  }
  return n;
}

// Main screen slots keep their position: "05,0D,00,2F" (00 = empty slot)
String slotsToString() {
  String s;
  char b[4];
  for (uint8_t i = 0; i < MAX_DASH; i++) {
    if (i) s += ',';
    snprintf(b, sizeof(b), "%02X", cfg.dash[i]);
    s += b;
  }
  return s;
}

void parseSlots(const String& in) {
  for (uint8_t i = 0; i < MAX_DASH; i++) cfg.dash[i] = 0;
  uint8_t n = 0;
  int start = 0;
  String s = in + ",";
  for (int i = 0; i < (int)s.length() && n < MAX_DASH; i++) {
    if (s[i] != ',') continue;
    String tok = s.substring(start, i);
    start = i + 1;
    tok.trim();
    long v = tok.length() ? strtol(tok.c_str(), nullptr, 16) : 0;
    cfg.dash[n++] = (v < 0 || v > 255) ? 0 : (uint8_t)v;
  }
}

// "05>105;42<11.5"
String rulesToString() {
  String s;
  char b[8];
  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    if (i) s += ';';
    snprintf(b, sizeof(b), "%02X%c", cfg.rules[i].pid, cfg.rules[i].above ? '>' : '<');
    s += b;
    s += fmtThreshold(cfg.rules[i].threshold);
  }
  return s;
}

uint8_t parseRules(const String& in, Rule* out, uint8_t maxN) {
  uint8_t n = 0;
  int start = 0;
  String s = in + ";";
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] != ';') continue;
    String tok = s.substring(start, i);
    start = i + 1;
    tok.trim();
    int pos = tok.indexOf('>');
    bool above = true;
    if (pos < 0) { pos = tok.indexOf('<'); above = false; }
    if (pos < 1) continue;
    long pid = strtol(tok.substring(0, pos).c_str(), nullptr, 16);
    if (pid < 1 || pid > 255) continue;
    String th = tok.substring(pos + 1);
    th.trim();
    if (!th.length() || n >= maxN) continue;
    out[n].pid = (uint8_t)pid;
    out[n].above = above;
    out[n].threshold = th.toFloat();
    n++;
  }
  return n;
}

void resetAlertStates() {
  for (uint8_t i = 0; i < MAX_RULES; i++) alertState[i] = AlertState{false, false, 0, 0};
}

void loadSettings() {
  prefs.begin("obd", false);
  cfg.wifiSsid       = prefs.getString("wssid", "");
  cfg.wifiPass       = prefs.getString("wpass", "");
  cfg.apSsid         = prefs.getString("assid", DEFAULT_AP_SSID);
  cfg.apPass         = prefs.getString("apass", DEFAULT_AP_PASS);
  cfg.streamUrl      = prefs.getString("surl", "");
  cfg.alertUrl       = prefs.getString("aurl", "");
  cfg.streamInterval = prefs.getUShort("sint", 10);
  cfg.logInterval    = prefs.getUShort("lint", 5);
  cfg.logDemo        = prefs.getBool("ldemo", true);
  cfg.touchOn        = prefs.getBool("touch", true);
  cfg.buttonOn       = prefs.getBool("btn", true);
  cfg.repaintSec     = prefs.getUShort("rpnt", 0);
  cfg.trackedCount   = parseHexList(prefs.getString("track", "0C,0D,05,11,04,0F,42"), cfg.tracked, MAX_TRACKED);
  cfg.favCount       = parseHexList(prefs.getString("favs", "0C,0D,05,42"), cfg.favs, MAX_FAVS);
  cfg.ruleCount      = parseRules(prefs.getString("rules", "05>105"), cfg.rules, MAX_RULES);
  // default main screen: big Coolant + Speed, small RPM / Fuel / Battery / Throttle
  parseSlots(prefs.getString("dash", "05,0D,0C,2F,42,11,04,0F"));
  cfg.dashLayout     = prefs.getUChar("dlay", 0);
  logOffset          = prefs.getUInt("logoff", 0);
  alertOffset        = prefs.getUInt("aoff", 0);
  bootCount          = prefs.getUInt("boot", 0) + 1;
  prefs.putUInt("boot", bootCount);
  if (cfg.streamInterval < 1) cfg.streamInterval = 1;
  if (cfg.logInterval < 1) cfg.logInterval = 1;
  resetAlertStates();
}

void markSaving();   // net.h
struct CfgLock;      // net.h

void saveSettingsUnlocked() {
  markSaving();
  prefs.putString("wssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);
  prefs.putString("assid", cfg.apSsid);
  prefs.putString("apass", cfg.apPass);
  prefs.putString("surl", cfg.streamUrl);
  prefs.putString("aurl", cfg.alertUrl);
  prefs.putUShort("sint", cfg.streamInterval);
  prefs.putUShort("lint", cfg.logInterval);
  prefs.putBool("ldemo", cfg.logDemo);
  prefs.putBool("touch", cfg.touchOn);
  prefs.putBool("btn", cfg.buttonOn);
  prefs.putUShort("rpnt", cfg.repaintSec);
  prefs.putString("track", hexList(cfg.tracked, cfg.trackedCount));
  prefs.putString("favs", hexList(cfg.favs, cfg.favCount));
  prefs.putString("rules", rulesToString());
  prefs.putString("dash", slotsToString());
  prefs.putUChar("dlay", cfg.dashLayout);
}

