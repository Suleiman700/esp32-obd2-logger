#pragma once
#include <WebServer.h>
#include <Update.h>
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
  j += "{\"fw\":\"" FW_VERSION "\",\"wifi_ssid\":\""; j += jsonEsc(cfg.wifiSsid);
  j += "\",\"wifi_pass_set\":"; j += cfg.wifiPass.length() ? "true" : "false";
  j += ",\"ap_ssid\":\"";   j += jsonEsc(cfg.apSsid);
  j += "\",\"ap_pass\":\""; j += jsonEsc(cfg.apPass);
  j += "\",\"stream_url\":\""; j += jsonEsc(cfg.streamUrl);
  j += "\",\"alert_url\":\"";  j += jsonEsc(cfg.alertUrl);
  j += "\",\"stream_int\":";   j += cfg.streamInterval;
  j += ",\"log_int\":";        j += cfg.logInterval;
  j += ",\"log_demo\":";       j += cfg.logDemo ? "true" : "false";
  j += ",\"touch_on\":";       j += cfg.touchOn ? "true" : "false";
  j += ",\"button_on\":";      j += cfg.buttonOn ? "true" : "false";
  j += ",\"repaint\":";        j += cfg.repaintSec;
  j += ",\"tracked\":\"";      j += hexList(cfg.tracked, cfg.trackedCount);
  j += "\",\"favs\":\"";       j += hexList(cfg.favs, cfg.favCount);
  j += "\",\"rules\":\"";      j += rulesToString();
  j += "\",\"dash\":\"";       j += slotsToString();
  j += "\",\"dash_layout\":";  j += cfg.dashLayout;
  j += "}";
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
  if (server.hasArg("button_on")) cfg.buttonOn = server.arg("button_on") == "1";
  if (server.hasArg("repaint")) {
    long v = server.arg("repaint").toInt();
    cfg.repaintSec = (uint16_t)(v < 0 ? 0 : (v > 3600 ? 3600 : v));
  }
  if (server.hasArg("tracked")) cfg.trackedCount = parseHexList(server.arg("tracked"), cfg.tracked, MAX_TRACKED);
  if (server.hasArg("favs"))    cfg.favCount = parseHexList(server.arg("favs"), cfg.favs, MAX_FAVS);
  if (server.hasArg("dash")) parseSlots(server.arg("dash"));
  if (server.hasArg("dash_layout")) {
    long v = server.arg("dash_layout").toInt();
    cfg.dashLayout = (uint8_t)((v < 0 || v >= DASH_LAYOUT_COUNT) ? 0 : v);
  }
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
  j += ",\"button\":\""; j += digitalRead(BUTTON_PIN) ? "released" : "PRESSED/LOW";
  j += "\",\"button_stuck\":"; j += buttonStuck ? "true" : "false";
  j += "}";
  sendJson(j);
}

// ---------- firmware update over WiFi (OTA) ----------
// Upload obd_monitor.ino.bin from the web panel (Settings -> Firmware update).
// Protected with the hotspot password (?key=...).
bool   otaAuthOk = false;
String otaError;

void otaScreen(const char* msg, int pct) {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Firmware update", SW / 2, 80, 4);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString(msg, SW / 2, 115, 2);
  if (pct >= 0) {
    tft.drawRect(40, 140, SW - 80, 16, C_LABEL);
    tft.fillRect(42, 142, (SW - 84) * pct / 100, 12, C_ACCENT);
  }
}

void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  static size_t total = 0;
  static int lastPct = -1;
  if (up.status == UPLOAD_FILE_START) {
    otaAuthOk = cfg.apPass.length() < 8 || server.arg("key") == cfg.apPass;
    otaError = "";
    total = 0; lastPct = -1;
    if (!otaAuthOk) { otaError = "Wrong password (use the hotspot password)"; return; }
    size_t len = server.hasArg("len") ? (size_t)server.arg("len").toInt() : 0;
    otaRunning = true;
    Serial.printf("OTA start: %s (%u bytes)\n", up.filename.c_str(), (unsigned)len);
    otaScreen("Receiving new firmware...", 0);
    if (!Update.begin(len ? len : UPDATE_SIZE_UNKNOWN, U_FLASH)) otaError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!otaAuthOk || otaError.length()) return;
    if (Update.write(up.buf, up.currentSize) != up.currentSize) { otaError = Update.errorString(); return; }
    total += up.currentSize;
    size_t len = server.hasArg("len") ? (size_t)server.arg("len").toInt() : 0;
    int pct = len ? (int)(total * 100 / len) : 0;
    if (pct != lastPct && pct % 5 == 0) { lastPct = pct; otaScreen("Receiving new firmware...", pct); }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!otaAuthOk || otaError.length()) return;
    if (!Update.end(true)) otaError = Update.errorString();
    Serial.printf("OTA end: %u bytes, %s\n", (unsigned)total, otaError.length() ? otaError.c_str() : "OK");
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaError = "Upload aborted";
  }
}

void handleUpdateDone() {
  if (otaAuthOk && !otaError.length() && !Update.hasError()) {
    otaScreen("Done - restarting...", 100);
    sendJson("{\"ok\":true}");
    restartAt = millis() + 1500;
  } else {
    otaRunning = false;
    if (!otaError.length()) otaError = "Update failed";
    Serial.printf("OTA failed: %s\n", otaError.c_str());
    sendError(otaError);
    needsFullRedraw = true;
  }
}

// ---------- test buttons (Settings page) ----------
void handleBuzzer() {
  if (server.arg("on") == "1") buzzerTestUntil = millis() + 10000;   // beeps for max 10 s
  else { buzzerTestUntil = 0; ackAlerts(); }                         // Stop = also silence alerts
  sendJson("{\"ok\":true}");
}

// Own client so it never collides with the background uploader
int testPost(const String& url, const String& body) {
  WiFiClient plain;
  WiFiClientSecure secure;
  HTTPClient http;
  bool ok;
  if (url.startsWith("https")) { secure.setInsecure(); ok = http.begin(secure, url); }
  else ok = http.begin(plain, url);
  if (!ok) return -1;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code;
}

// Same JSON format as the real uploads, with "test":true
void handleTest() {
  bool alert = server.arg("what") == "alert";
  String url, device;
  { CfgLock l; url = alert ? cfg.alertUrl : cfg.streamUrl; device = cfg.apSsid; }
  if (!url.length()) { sendError(alert ? "Set the Alerts URL first (and Save)" : "Set the Data URL first (and Save)"); return; }
  if (wifiState != WS_CONNECTED) { sendError("Car WiFi is not connected to the internet WiFi"); return; }

  uint32_t t = nowEpoch();
  String b = "{\"device\":\"" + jsonEsc(device) + "\",\"type\":\"" + (alert ? "alert" : "log") + "\",\"test\":true,\"records\":[{\"ts\":";
  b += t ? String(t) : String("null");
  if (alert) {
    uint8_t p = cfg.ruleCount ? cfg.rules[0].pid : 0x05;
    bool above = cfg.ruleCount ? cfg.rules[0].above : true;
    float th = cfg.ruleCount ? cfg.rules[0].threshold : 105;
    b += ",\"event\":\"triggered\",\"test\":true,\"pid\":\"0x" + pidHex(p);
    b += "\",\"key\":\"" + pidKey(p) + "\",\"name\":\"TEST " + jsonEsc(pidName(p));
    b += "\",\"condition\":\""; b += above ? ">" : "<";
    b += "\",\"threshold\":" + fmtThreshold(th);
    b += ",\"value\":" + jsonValue(p);
    b += ",\"unit\":\"" + String(pidUnit(p));
  } else {
    b += ",\"boot\":" + String(bootCount) + ",\"uptime\":" + String(millis() / 1000);
    b += ",\"test\":true,\"values\":{";
    bool first = true;
    for (uint8_t i = 0; i < cfg.trackedCount; i++) {
      uint8_t p = cfg.tracked[i];
      if (!isFresh(p)) continue;
      if (!first) b += ',';
      first = false;
      b += "\"" + pidKey(p) + "\":" + jsonValue(p);
    }
    b += "}";
  }
  b += ",\"mode\":\""; b += demoMode ? "demo" : "live";
  b += "\"}]}";

  int code = testPost(url, b);
  Serial.printf("Test %s -> %d\n", alert ? "alert" : "stream", code);
  if (code >= 200 && code < 300) sendJson("{\"ok\":true,\"code\":" + String(code) + "}");
  else sendError(code < 0 ? "No answer from the server (" + String(code) + ")" : "Server answered HTTP " + String(code));
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
  server.on("/api/buzzer", HTTP_POST, handleBuzzer);
  server.on("/api/test", HTTP_POST, handleTest);
  server.on("/api/clearlog", HTTP_POST, handleClearLog);
  server.on("/api/touchcal", HTTP_POST, handleTouchCal);
  server.on("/api/repaint", HTTP_POST, handleRepaint);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/api/touch", HTTP_GET, handleTouchInfo);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
  Serial.println("Web panel ready");
}
