#pragma once
#include <WebServer.h>
#include "display.h"
#include "webpage.h"

// =====================================================================
//  Web control panel (http://192.168.4.1 on the hotspot)
// =====================================================================

WebServer server(80);

void sendJson(const String& j) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

String pidHex(uint8_t p) {
  char b[4];
  snprintf(b, sizeof(b), "%02X", p);
  return b;
}

String jsonValue(uint8_t pid) {
  if (!isFresh(pid)) return "null";
  float v = pidData[pid].value;
  return String(v, (unsigned int)(pidDecimals(pid, v) + 1));
}

void appendPidInfo(String& j, uint8_t p) {
  j += "{\"pid\":\"";  j += pidHex(p);
  j += "\",\"key\":\""; j += pidKey(p);
  j += "\",\"name\":\""; j += pidName(p);
  j += "\",\"unit\":\""; j += pidUnit(p);
  j += "\"";
}

// ---------- /api/live ----------
void handleLive() {
  String j;
  j.reserve(4000);
  j += "{\"mode\":\"";   j += demoMode ? "demo" : (linkLost ? "lost" : "live");
  j += "\",\"uptime\":"; j += millis() / 1000;
  j += ",\"time\":";     j += nowEpoch();
  j += ",\"wifi\":{\"state\":\""; j += wifiStateName();
  j += "\",\"ssid\":\""; j += jsonEsc(cfg.wifiSsid);
  j += "\",\"ip\":\"";   j += wifiState == WS_CONNECTED ? WiFi.localIP().toString() : String("");
  j += "\",\"rssi\":";   j += wifiState == WS_CONNECTED ? WiFi.RSSI() : 0;
  j += "},\"log\":{\"pending\":"; j += logPendingBytes();
  j += ",\"full\":";     j += logFull ? "true" : "false";
  j += ",\"alertsPending\":"; j += alertPendingBytes();
  j += "},\"stream\":{\"url\":"; j += cfg.streamUrl.length() ? "true" : "false";
  j += ",\"code\":";     j += lastHttpCode;
  j += ",\"ago\":";      j += lastUploadOk ? (long)((millis() - lastUploadOk) / 1000) : -1L;
  j += "},\"alerts\":[";
  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    const Rule& r = cfg.rules[i];
    if (i) j += ',';
    j += "{\"i\":"; j += i;
    j += ",\"pid\":\""; j += pidHex(r.pid);
    j += "\",\"name\":\""; j += pidName(r.pid);
    j += "\",\"cond\":\""; j += r.above ? ">" : "<";
    j += "\",\"th\":";      j += fmtThreshold(r.threshold);
    j += ",\"value\":";     j += jsonValue(r.pid);
    j += ",\"active\":";    j += alertState[i].active ? "true" : "false";
    j += ",\"acked\":";     j += alertState[i].acked ? "true" : "false";
    j += "}";
  }
  j += "],\"values\":[";
  for (uint8_t i = 0; i < pollCount; i++) {
    uint8_t p = pollList[i];
    if (i) j += ',';
    appendPidInfo(j, p);
    j += ",\"value\":"; j += jsonValue(p);
    j += ",\"fav\":";   j += inList(cfg.favs, cfg.favCount, p) ? "true" : "false";
    j += ",\"log\":";   j += inList(cfg.tracked, cfg.trackedCount, p) ? "true" : "false";
    j += "}";
  }
  j += "]}";
  sendJson(j);
}

// ---------- /api/pids ----------
void appendPidRow(String& j, uint8_t p, bool& first) {
  if (!first) j += ',';
  first = false;
  appendPidInfo(j, p);
  j += ",\"supported\":"; j += pidData[p].supported ? "true" : "false";
  j += ",\"log\":";       j += inList(cfg.tracked, cfg.trackedCount, p) ? "true" : "false";
  j += ",\"fav\":";       j += inList(cfg.favs, cfg.favCount, p) ? "true" : "false";
  j += "}";
}

void handlePids() {
  String j;
  j.reserve(4000);
  j += "{\"supportKnown\":"; j += (supportKnown && !demoMode) ? "true" : "false";
  j += ",\"pids\":[";
  bool first = true;
  for (uint8_t i = 0; i < PID_TABLE_COUNT; i++) appendPidRow(j, PID_TABLE[i].pid, first);
  for (uint8_t i = 0; i < cfg.trackedCount; i++) if (!findPid(cfg.tracked[i])) appendPidRow(j, cfg.tracked[i], first);
  for (uint8_t i = 0; i < cfg.favCount; i++)
    if (!findPid(cfg.favs[i]) && !inList(cfg.tracked, cfg.trackedCount, cfg.favs[i])) appendPidRow(j, cfg.favs[i], first);
  j += "]}";
  sendJson(j);
}

// ---------- /api/config ----------
void handleGetConfig() {
  String j;
  j += "{\"wifi_ssid\":\""; j += jsonEsc(cfg.wifiSsid);
  j += "\",\"wifi_pass_set\":"; j += cfg.wifiPass.length() ? "true" : "false";
  j += ",\"ap_ssid\":\"";   j += jsonEsc(cfg.apSsid);
  j += "\",\"ap_pass\":\""; j += jsonEsc(cfg.apPass);
  j += "\",\"stream_url\":\""; j += jsonEsc(cfg.streamUrl);
  j += "\",\"alert_url\":\"";  j += jsonEsc(cfg.alertUrl);
  j += "\",\"stream_int\":";   j += cfg.streamInterval;
  j += ",\"log_int\":";        j += cfg.logInterval;
  j += ",\"log_demo\":";       j += cfg.logDemo ? "true" : "false";
  j += ",\"touch_on\":";       j += cfg.touchOn ? "true" : "false";
  j += ",\"repaint\":";        j += cfg.repaintSec;
  j += ",\"tracked\":\"";      j += hexList(cfg.tracked, cfg.trackedCount);
  j += "\",\"favs\":\"";       j += hexList(cfg.favs, cfg.favCount);
  j += "\",\"rules\":\"";      j += rulesToString();
  j += "\"}";
  sendJson(j);
}

void sendError(const String& msg) {
  sendJson("{\"ok\":false,\"error\":\"" + jsonEsc(msg) + "\"}");
}

void handlePostConfig() {
  bool wifiChanged = false, apChanged = false;
  CfgLock lock;   // storage task must not read cfg while we change it

  if (server.hasArg("ap_pass")) {
    String p = server.arg("ap_pass");
    if (p.length() > 0 && p.length() < 8) { sendError("Hotspot password must be at least 8 characters"); return; }
    if (p != cfg.apPass) { cfg.apPass = p; apChanged = true; }
  }
  if (server.hasArg("ap_ssid")) {
    String s = server.arg("ap_ssid");
    s.trim();
    if (s.length() && s != cfg.apSsid) { cfg.apSsid = s; apChanged = true; }
  }
  if (server.hasArg("wifi_ssid")) {
    String s = server.arg("wifi_ssid");
    s.trim();
    if (s != cfg.wifiSsid) { cfg.wifiSsid = s; wifiChanged = true; }
  }
  if (server.hasArg("wifi_pass")) {
    String p = server.arg("wifi_pass");
    if (p != cfg.wifiPass) { cfg.wifiPass = p; wifiChanged = true; }
  }
  if (server.hasArg("stream_url")) { cfg.streamUrl = server.arg("stream_url"); cfg.streamUrl.trim(); lastHttpCode = 0; }
  if (server.hasArg("alert_url"))  { cfg.alertUrl = server.arg("alert_url");   cfg.alertUrl.trim(); }
  if (server.hasArg("stream_int")) {
    long v = server.arg("stream_int").toInt();
    cfg.streamInterval = (uint16_t)(v < 1 ? 1 : (v > 3600 ? 3600 : v));
  }
  if (server.hasArg("log_int")) {
    long v = server.arg("log_int").toInt();
    cfg.logInterval = (uint16_t)(v < 1 ? 1 : (v > 3600 ? 3600 : v));
  }
  if (server.hasArg("log_demo")) cfg.logDemo = server.arg("log_demo") == "1";
  if (server.hasArg("touch_on")) cfg.touchOn = server.arg("touch_on") == "1";
  if (server.hasArg("repaint")) {
    long v = server.arg("repaint").toInt();
    cfg.repaintSec = (uint16_t)(v < 0 ? 0 : (v > 3600 ? 3600 : v));
  }
  if (server.hasArg("tracked")) cfg.trackedCount = parseHexList(server.arg("tracked"), cfg.tracked, MAX_TRACKED);
  if (server.hasArg("favs"))    cfg.favCount = parseHexList(server.arg("favs"), cfg.favs, MAX_FAVS);
  if (server.hasArg("rules")) {
    cfg.ruleCount = parseRules(server.arg("rules"), cfg.rules, MAX_RULES);
    resetAlertStates();
  }

  saveSettingsUnlocked();
  rebuildPollList();
  needsFullRedraw = true;
  sendJson("{\"ok\":true}");

  // apply network changes after the reply has been sent
  if (wifiChanged) staRestartAt = millis() + 500;
  if (apChanged)   apRestartAt  = millis() + 1500;
}

// ---------- /api/query?pid=5C ----------
void handleQuery() {
  String a = server.arg("pid");
  a.trim();
  if (a.startsWith("0x") || a.startsWith("0X")) a = a.substring(2);
  long p = a.length() ? strtol(a.c_str(), nullptr, 16) : -1;
  if (p < 1 || p > 255) { sendError("Enter a PID in hex, e.g. 0C or 5C"); return; }
  uint8_t pid = (uint8_t)p;

  bool ok = false;
  if (!demoMode && canInitialized) {
    uint32_t before = pidData[pid].lastSeen;
    sendOBDRequest(pid);
    uint32_t start = millis();
    while (millis() - start < 600) {
      readCAN();
      if (pidData[pid].hasValue && pidData[pid].lastSeen != before) { ok = true; break; }
      delay(5);
    }
  } else {
    ok = isFresh(pid);
  }

  if (!ok) {
    sendError(demoMode ? "DEMO mode: no car connected. Only built-in PIDs have fake values."
                       : "No reply from the car. This PID is probably not supported.");
    return;
  }
  String raw;
  char b[4];
  for (uint8_t i = 0; i < pidData[pid].rawLen; i++) {
    snprintf(b, sizeof(b), "%02X ", pidData[pid].raw[i]);
    raw += b;
  }
  if (!raw.length()) raw = "(simulated)";
  String j = "{\"ok\":true,";
  j += "\"mode\":\""; j += demoMode ? "demo" : "live"; j += "\",";
  j += "\"pid\":\"";  j += pidHex(pid);
  j += "\",\"name\":\""; j += pidName(pid);
  j += "\",\"unit\":\""; j += pidUnit(pid);
  j += "\",\"value\":";  j += String(pidData[pid].value, 3U);
  j += ",\"raw\":\"";    j += raw;
  j += "\",\"known\":";  j += findPid(pid) ? "true" : "false";
  j += "}";
  sendJson(j);
}

void handleRepaint() {
  repaintRequest = true;
  sendJson("{\"ok\":true}");
}

void handleTouchCal() {
  touchCalRequest = true;
  sendJson("{\"ok\":true}");
}

// Live touch status - open http://192.168.4.1/api/touch and press the screen
void handleTouchInfo() {
  uint16_t x, y;
  bool down = readTouch(x, y);
  String j = "{\"pressed\":"; j += down ? "true" : "false";
  j += ",\"z\":";    j += lastRawZ;
  j += ",\"rawX\":"; j += lastRawX;
  j += ",\"rawY\":"; j += lastRawY;
  j += ",\"x\":";    j += lastTouchX;
  j += ",\"y\":";    j += lastTouchY;
  j += ",\"calibrated\":"; j += tcal.valid ? "true" : "false";
  j += ",\"enabled\":"; j += cfg.touchOn ? "true" : "false";
  j += ",\"stuck\":"; j += touchStuck ? "true" : "false";
  j += "}";
  sendJson(j);
}

void handleAck() {
  ackAlerts();
  sendJson("{\"ok\":true}");
}

void handleClearLog() {
  clearStoredData();
  sendJson("{\"ok\":true}");
}

void webBegin() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/live", HTTP_GET, handleLive);
  server.on("/api/pids", HTTP_GET, handlePids);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/query", HTTP_GET, handleQuery);
  server.on("/api/ack", HTTP_POST, handleAck);
  server.on("/api/clearlog", HTTP_POST, handleClearLog);
  server.on("/api/touchcal", HTTP_POST, handleTouchCal);
  server.on("/api/repaint", HTTP_POST, handleRepaint);
  server.on("/api/touch", HTTP_GET, handleTouchInfo);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
  Serial.println("Web panel ready");
}
