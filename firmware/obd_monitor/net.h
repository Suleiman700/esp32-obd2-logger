#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <time.h>
#include "settings.h"

// =====================================================================
//  WiFi (hotspot + car WiFi), logging to flash, streaming to URL
// =====================================================================

enum WifiState { WS_NOT_SET, WS_CONNECTING, WS_CONNECTED, WS_FAILED };
WifiState wifiState = WS_NOT_SET;
uint32_t staAttemptStart = 0, staFailAt = 0;
uint32_t apRestartAt = 0, staRestartAt = 0;   // delayed restarts after saving settings

int      lastHttpCode  = 0;
uint32_t lastUploadOk  = 0;
bool     logFull       = false;

// ---- shared between the display loop (core 1) and the storage task (core 0) ----
volatile uint32_t cachedLogPending   = 0;
volatile uint32_t cachedAlertPending = 0;
volatile bool     clearRequested     = false;
volatile uint32_t saveIconUntil      = 0;      // save icon is lit until this time
SemaphoreHandle_t cfgLock    = nullptr;          // protects cfg + prefs
QueueHandle_t     alertQueue = nullptr;          // alert lines waiting to be written
struct AlertLine { char text[112]; };

void markSaving() { saveIconUntil = millis() + 600; }

struct CfgLock {
  CfgLock()  { if (cfgLock) xSemaphoreTake(cfgLock, portMAX_DELAY); }
  ~CfgLock() { if (cfgLock) xSemaphoreGive(cfgLock); }
};

const char* wifiStateName() {
  switch (wifiState) {
    case WS_CONNECTING: return "connecting";
    case WS_CONNECTED:  return "connected";
    case WS_FAILED:     return "failed";
    default:            return "not_set";
  }
}

String jsonEsc(const String& s) {
  String o;
  o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

// ---------------- time ----------------
uint32_t nowEpoch() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;
}

// Records saved before the clock was set get their time fixed when uploading
uint32_t fixTimestamp(uint32_t t, uint32_t boot, uint32_t uptime) {
  if (t) return t;
  uint32_t now = nowEpoch();
  if (now && boot == bootCount) return now - millis() / 1000 + uptime;
  return 0;
}

// ---------------- WiFi ----------------
void startAP() {
  const char* pass = cfg.apPass.length() >= 8 ? cfg.apPass.c_str() : nullptr;
  WiFi.softAP(cfg.apSsid.c_str(), pass);
  Serial.printf("Hotspot \"%s\" at %s\n", cfg.apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

void startSta() {
  if (!cfg.wifiSsid.length()) {
    WiFi.disconnect();
    wifiState = WS_NOT_SET;
    return;
  }
  WiFi.disconnect();
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  wifiState = WS_CONNECTING;
  staAttemptStart = millis();
  Serial.printf("Connecting to WiFi \"%s\"...\n", cfg.wifiSsid.c_str());
}

void wifiBegin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  startAP();
  startSta();
}

void wifiLoop() {
  uint32_t now = millis();
  if (apRestartAt && (int32_t)(now - apRestartAt) >= 0)  { apRestartAt = 0; startAP(); }
  if (staRestartAt && (int32_t)(now - staRestartAt) >= 0) { staRestartAt = 0; startSta(); return; }

  if (!cfg.wifiSsid.length()) { wifiState = WS_NOT_SET; return; }

  if (WiFi.status() == WL_CONNECTED) {
    if (wifiState != WS_CONNECTED) {
      wifiState = WS_CONNECTED;
      configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
      Serial.printf("WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }
  switch (wifiState) {
    case WS_CONNECTED:            // lost connection -> auto reconnect
      wifiState = WS_CONNECTING;
      staAttemptStart = now;
      break;
    case WS_CONNECTING:
      if (now - staAttemptStart > WIFI_CONNECT_TIMEOUT) {
        wifiState = WS_FAILED;
        staFailAt = now;
        WiFi.disconnect();
        Serial.println("WiFi connect failed, will retry");
      }
      break;
    case WS_FAILED:
      if (now - staFailAt > WIFI_RETRY_MS) startSta();
      break;
    case WS_NOT_SET:
      startSta();
      break;
  }
}

// ---------------- files ----------------
uint32_t fileSize(const char* path) {
  if (!LittleFS.exists(path)) return 0;
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  uint32_t s = f.size();
  f.close();
  return s;
}

uint32_t pendingBytes(const char* path, uint32_t offset) {
  uint32_t s = fileSize(path);
  return s > offset ? s - offset : 0;
}

// fast, cached values for the display / web panel
uint32_t logPendingBytes()   { return cachedLogPending; }
uint32_t alertPendingBytes() { return cachedAlertPending; }

// real values (storage task only)
uint32_t calcLogPending()   { return pendingBytes(LOG_FILE, logOffset); }
uint32_t calcAlertPending() { return pendingBytes(ALERT_FILE, alertOffset); }

void clearStoredData() { clearRequested = true; }   // done by the storage task

void doClearStoredData() {
  markSaving();
  LittleFS.remove(LOG_FILE);
  LittleFS.remove(ALERT_FILE);
  logOffset = alertOffset = 0;
  {
    CfgLock l;
    prefs.putUInt("logoff", 0);
    prefs.putUInt("aoff", 0);
  }
  logFull = false;
}

// ---------------- logger ----------------
// line: epoch,boot,uptime,mode,PID:value,PID:value...
void loggerLoop() {
  static uint32_t lastLog = 0;
  uint8_t tracked[MAX_TRACKED], trackedCount;
  uint16_t interval;
  bool logDemo;
  {
    CfgLock l;
    memcpy(tracked, cfg.tracked, sizeof(tracked));
    trackedCount = cfg.trackedCount;
    interval = cfg.logInterval;
    logDemo = cfg.logDemo;
  }
  if (millis() - lastLog < (uint32_t)interval * 1000UL) return;
  lastLog = millis();
  if (demoMode && !logDemo) return;
  if (trackedCount == 0) return;

  if (calcLogPending() > MAX_LOG_BYTES) { logFull = true; return; }
  logFull = false;

  char line[320];
  int n = snprintf(line, sizeof(line), "%lu,%lu,%lu,%d",
                   (unsigned long)nowEpoch(), (unsigned long)bootCount,
                   (unsigned long)(millis() / 1000), demoMode ? 0 : 1);
  int values = 0;
  for (uint8_t i = 0; i < trackedCount && n < (int)sizeof(line) - 20; i++) {
    uint8_t p = tracked[i];
    if (!isFresh(p)) continue;
    n += snprintf(line + n, sizeof(line) - n, ",%02X:%.2f", p, pidData[p].value);
    values++;
  }
  if (!values) return;

  markSaving();
  File f = LittleFS.open(LOG_FILE, "a");
  if (!f) return;
  f.println(line);
  f.close();
}

// ---------------- HTTP ----------------
int httpPost(const String& url, const String& body) {
  static WiFiClient plain;
  static WiFiClientSecure secure;
  HTTPClient http;
  bool ok;
  if (url.startsWith("https")) {
    secure.setInsecure();      // no certificate check (simpler for home servers)
    ok = http.begin(secure, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return -1;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code;
}

bool logLineToJson(char* line, String& out) {
  char* save;
  char* tok = strtok_r(line, ",", &save);     if (!tok) return false;
  uint32_t t = strtoul(tok, nullptr, 10);
  tok = strtok_r(nullptr, ",", &save);        if (!tok) return false;
  uint32_t b = strtoul(tok, nullptr, 10);
  tok = strtok_r(nullptr, ",", &save);        if (!tok) return false;
  uint32_t u = strtoul(tok, nullptr, 10);
  tok = strtok_r(nullptr, ",", &save);        if (!tok) return false;
  bool live = atoi(tok) == 1;
  t = fixTimestamp(t, b, u);

  out += "{\"ts\":";
  if (t) out += t; else out += "null";
  out += ",\"boot\":";   out += b;
  out += ",\"uptime\":"; out += u;
  out += ",\"mode\":\""; out += live ? "live" : "demo";
  out += "\",\"values\":{";
  bool first = true;
  while ((tok = strtok_r(nullptr, ",", &save))) {
    char* c = strchr(tok, ':');
    if (!c) continue;
    *c = 0;
    uint8_t pid = (uint8_t)strtol(tok, nullptr, 16);
    if (!first) out += ',';
    first = false;
    out += '"'; out += pidKey(pid); out += "\":"; out += (c + 1);
  }
  out += "}}";
  return true;
}

// line: epoch,boot,uptime,pid,above,threshold,value,triggered,mode
bool alertLineToJson(char* line, String& out) {
  char* f[9];
  char* save;
  int n = 0;
  for (char* tok = strtok_r(line, ",", &save); tok && n < 9; tok = strtok_r(nullptr, ",", &save)) f[n++] = tok;
  if (n < 9) return false;
  uint32_t t = fixTimestamp(strtoul(f[0], nullptr, 10), strtoul(f[1], nullptr, 10), strtoul(f[2], nullptr, 10));
  uint8_t pid = (uint8_t)strtol(f[3], nullptr, 16);
  char pidHex[8];
  snprintf(pidHex, sizeof(pidHex), "0x%02X", pid);

  out += "{\"ts\":";
  if (t) out += t; else out += "null";
  out += ",\"event\":\"";     out += atoi(f[7]) ? "triggered" : "cleared";
  out += "\",\"pid\":\"";     out += pidHex;
  out += "\",\"key\":\"";     out += pidKey(pid);
  out += "\",\"name\":\"";    out += pidName(pid);
  out += "\",\"condition\":\""; out += atoi(f[4]) ? ">" : "<";
  out += "\",\"threshold\":"; out += f[5];
  out += ",\"value\":";       out += f[6];
  out += ",\"unit\":\"";      out += pidUnit(pid);
  out += "\",\"mode\":\"";    out += atoi(f[8]) ? "live" : "demo";
  out += "\"}";
  return true;
}

typedef bool (*LineConverter)(char*, String&);

// Sends the next batch of a file. Returns HTTP code (0 = nothing to send).
void saveOffset(const char* prefKey, uint32_t v) {
  markSaving();
  CfgLock l;
  prefs.putUInt(prefKey, v);
}

int uploadFile(const char* path, uint32_t& offset, const char* prefKey, const String& device,
               const String& url, const char* type, LineConverter conv, bool& more) {
  more = false;
  if (!LittleFS.exists(path)) { offset = 0; return 0; }
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  uint32_t size = f.size();
  if (offset >= size) {
    f.close();
    LittleFS.remove(path);
    offset = 0;
    saveOffset(prefKey, 0);
    return 0;
  }
  f.seek(offset);

  String body;
  body.reserve(UPLOAD_BATCH * 200 + 100);
  body = "{\"device\":\"";
  body += jsonEsc(device);
  body += "\",\"type\":\"";
  body += type;
  body += "\",\"records\":[";
  int count = 0;
  char line[340];
  while (count < UPLOAD_BATCH && f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    if (n && line[n - 1] == '\r') line[n - 1] = 0;
    if (!line[0]) continue;
    String rec;
    if (!conv(line, rec)) continue;
    if (count) body += ',';
    body += rec;
    count++;
  }
  uint32_t newOffset = f.position();
  f.close();
  body += "]}";

  int code = 200;
  if (count > 0) code = httpPost(url, body);
  if (code >= 200 && code < 300) {
    offset = newOffset;
    if (offset >= size) {
      LittleFS.remove(path);
      offset = 0;
    } else {
      more = true;
    }
    saveOffset(prefKey, offset);
  }
  return code;
}

void uploaderLoop() {
  static uint32_t lastStream = 0, lastAlertTry = 0;
  static bool catchingUp = false;
  if (wifiState != WS_CONNECTED) { catchingUp = false; return; }
  bool more;

  String streamUrl, alertUrl, device;
  uint16_t streamInterval;
  {
    CfgLock l;
    streamUrl = cfg.streamUrl;
    alertUrl = cfg.alertUrl;
    device = cfg.apSsid;
    streamInterval = cfg.streamInterval;
  }

  // Alerts go out right away
  if (alertUrl.length() && calcAlertPending() > 0 && millis() - lastAlertTry > 2000) {
    lastAlertTry = millis();
    int code = uploadFile(ALERT_FILE, alertOffset, "aoff", device, alertUrl, "alert", alertLineToJson, more);
    Serial.printf("Alert upload -> %d\n", code);
  }

  if (!streamUrl.length()) return;
  uint32_t gap = catchingUp ? 300UL : (uint32_t)streamInterval * 1000UL;
  if (millis() - lastStream < gap) return;
  lastStream = millis();
  if (calcLogPending() == 0) { catchingUp = false; return; }

  int code = uploadFile(LOG_FILE, logOffset, "logoff", device, streamUrl, "log", logLineToJson, more);
  lastHttpCode = code;
  if (code >= 200 && code < 300) lastUploadOk = millis();
  catchingUp = more;   // backlog (e.g. after being offline) -> keep sending quickly
  Serial.printf("Stream upload -> %d%s\n", code, more ? " (more waiting)" : "");
}

// =====================================================================
//  Storage task: logging, alert file, uploads.  Runs on core 0 so the
//  screen (core 1) never freezes while saving or waiting for the server.
// =====================================================================
void drainAlertQueue() {
  AlertLine a;
  while (alertQueue && xQueueReceive(alertQueue, &a, 0) == pdTRUE) {
    if (fileSize(ALERT_FILE) > MAX_ALERT_BYTES) continue;
    markSaving();
    File f = LittleFS.open(ALERT_FILE, "a");
    if (!f) continue;
    f.print(a.text);
    f.close();
  }
}

void storageTask(void*) {
  uint32_t lastCache = 0;
  for (;;) {
    if (clearRequested) { doClearStoredData(); clearRequested = false; }
    loggerLoop();
    drainAlertQueue();
    uploaderLoop();
    if (millis() - lastCache > 500) {
      lastCache = millis();
      cachedLogPending = calcLogPending();
      cachedAlertPending = calcAlertPending();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void storageBegin() {
  cfgLock = xSemaphoreCreateMutex();
  alertQueue = xQueueCreate(16, sizeof(AlertLine));
  cachedLogPending = calcLogPending();
  cachedAlertPending = calcAlertPending();
  xTaskCreatePinnedToCore(storageTask, "storage", 12288, nullptr, 1, nullptr, 0);
}
