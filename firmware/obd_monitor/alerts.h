#pragma once
#include "net.h"

// =====================================================================
//  Alert rules, buzzer, alert events
// =====================================================================

bool buzzerShouldSound() {
  for (uint8_t i = 0; i < cfg.ruleCount; i++)
    if (alertState[i].active && !alertState[i].acked) return true;
  return false;
}

int activeAlertCount() {
  int n = 0;
  for (uint8_t i = 0; i < cfg.ruleCount; i++) if (alertState[i].active) n++;
  return n;
}

void ackAlerts() {
  for (uint8_t i = 0; i < cfg.ruleCount; i++)
    if (alertState[i].active) alertState[i].acked = true;
  Serial.println("Alerts silenced");
}

void recordAlertEvent(uint8_t i, bool triggered, float value) {
  const Rule& r = cfg.rules[i];
  Serial.printf("ALERT %s: %s = %.2f (%c %.2f)\n", triggered ? "TRIGGERED" : "cleared",
                pidName(r.pid).c_str(), value, r.above ? '>' : '<', r.threshold);
  if (!cfg.alertUrl.length() || !alertQueue) return;       // nowhere to send
  AlertLine a;                                              // written to flash by the storage task
  snprintf(a.text, sizeof(a.text), "%lu,%lu,%lu,%02X,%d,%.2f,%.2f,%d,%d\n",
           (unsigned long)nowEpoch(), (unsigned long)bootCount, (unsigned long)(millis() / 1000),
           r.pid, r.above ? 1 : 0, r.threshold, value, triggered ? 1 : 0, demoMode ? 0 : 1);
  xQueueSend(alertQueue, &a, 0);
}

void alertsLoop() {
  static uint32_t last = 0;
  if (millis() - last < 200) return;
  last = millis();

  for (uint8_t i = 0; i < cfg.ruleCount; i++) {
    const Rule& r = cfg.rules[i];
    AlertState& a = alertState[i];
    if (!isFresh(r.pid)) continue;
    float v = pidData[r.pid].value;
    float hyst = fabsf(r.threshold) * 0.02f;    // small gap so it doesn't flicker on/off
    if (hyst < 0.5f) hyst = 0.5f;
    bool bad = r.above ? (v > r.threshold) : (v < r.threshold);
    bool ok  = r.above ? (v < r.threshold - hyst) : (v > r.threshold + hyst);
    a.value = v;
    if (!a.active && bad) {
      a.active = true;
      a.acked = false;
      a.since = millis();
      recordAlertEvent(i, true, v);
    } else if (a.active && ok) {
      a.active = false;
      a.acked = false;
      recordAlertEvent(i, false, v);
    }
  }
}

void buzzerLoop() {
  static bool on = false;
  static uint32_t last = 0;
  bool test = buzzerTestUntil && (int32_t)(buzzerTestUntil - millis()) > 0;
  if (!test) buzzerTestUntil = 0;
  if (buzzerShouldSound() || test) {
    if (millis() - last >= 250) {          // beep-beep pattern
      last = millis();
      on = !on;
      if (on) tone(BUZZER_PIN, 2700);
      else noTone(BUZZER_PIN);
    }
  } else if (on) {
    noTone(BUZZER_PIN);
    on = false;
  }
}
