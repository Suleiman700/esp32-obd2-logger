#pragma once
#include <driver/twai.h>
#include "state.h"

// =====================================================================
//  CAN / OBD-II (11-bit, 500 kbps) + DEMO data
// =====================================================================

bool canInit() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  g.tx_queue_len = 5;
  g.rx_queue_len = 32;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) { Serial.println("CAN driver install failed"); return false; }
  if (twai_start() != ESP_OK) { Serial.println("CAN start failed"); return false; }
  Serial.println("CAN initialized OK");
  return true;
}

bool sendOBDRequest(uint8_t pid) {
  if (!canInitialized) return false;
  twai_message_t m = {};
  m.identifier = 0x7DF;          // broadcast to all ECUs
  m.extd = 0;
  m.rtr = 0;
  m.data_length_code = 8;
  m.data[0] = 0x02;              // 2 bytes follow
  m.data[1] = 0x01;              // Mode 01 = current data
  m.data[2] = pid;
  for (int i = 3; i < 8; i++) m.data[i] = 0x55;
  return twai_transmit(&m, 0) == ESP_OK;
}

void handleFrame(const twai_message_t& msg) {
  if (msg.extd || msg.rtr) return;
  if (msg.identifier < 0x7E8 || msg.identifier > 0x7EF) return;   // ECU replies
  if (msg.data_length_code < 4) return;
  uint8_t len = msg.data[0];
  if (len < 3 || len > 7) return;
  if (msg.data[1] != 0x41) return;                                // Mode 01 reply
  uint8_t pid = msg.data[2];
  uint8_t n = len - 2;
  const uint8_t* b = &msg.data[3];
  lastCanResponse = millis();

  // "Supported PIDs" replies: 0x00, 0x20, 0x40 ...
  if ((pid % 0x20) == 0 && pid <= 0xC0) {
    if (n >= 4) {
      uint32_t mask = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
      for (uint8_t i = 0; i < 32; i++)
        if (mask & (1UL << (31 - i))) pidData[pid + i + 1].supported = true;
      supportKnown = true;
    }
    return;
  }

  PidState& s = pidData[pid];
  s.rawLen = n > 5 ? 5 : n;
  memcpy(s.raw, b, s.rawLen);
  s.value = decodePid(findPid(pid), b, n);
  s.hasValue = true;
  s.lastSeen = millis();
}

void readCAN() {
  if (!canInitialized) return;
  twai_message_t msg;
  for (int k = 0; k < 16 && twai_receive(&msg, 0) == ESP_OK; k++) handleFrame(msg);
}

void canMaintenance() {
  static uint32_t last = 0;
  if (!canInitialized || millis() - last < 1000) return;
  last = millis();
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) return;
  if (st.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
  else if (st.state == TWAI_STATE_STOPPED) twai_start();
}

void obdLoop() {
  static uint32_t lastReq = 0, lastProbe = 0, lastDisc = 0;
  static uint8_t idx = 0, discovery = 0;

  readCAN();
  canMaintenance();

  bool live = lastCanResponse != 0 && (millis() - lastCanResponse) < LIVE_TIMEOUT_MS;
  if (live && demoMode) {                 // first time the car answers: drop all fake data
    demoMode = false;
    linkLost = false;
    for (int i = 0; i < 256; i++) { pidData[i].hasValue = false; pidData[i].supported = false; }
    supportKnown = false;
    discovery = 0;
    Serial.println(">>> ECU found - LIVE mode");
  } else if (live && linkLost) {          // car is back
    linkLost = false;
    Serial.println(">>> ECU back - LIVE mode");
  } else if (!live && !demoMode && !linkLost) {
    linkLost = true;                      // never go back to fake data once the car was seen
    lostSince = millis();
    Serial.println(">>> ECU stopped answering - CONNECTION LOST");
  }
  if (!canInitialized) return;

  if (!demoMode) {
    // Ask the car which PIDs it supports (once per LIVE session)
    if (discovery < 4) {
      if (millis() - lastDisc > 60) {
        sendOBDRequest(discovery * 0x20);
        discovery++;
        lastDisc = lastReq = millis();
      }
      return;
    }
    if (millis() - lastReq >= CAN_POLL_GAP_MS) {
      lastReq = millis();
      bool sent = false;
      for (uint8_t tries = 0; tries < pollCount && !sent; tries++) {
        uint8_t p = pollList[idx];
        idx = (idx + 1) % pollCount;
        if (!supportKnown || pidData[p].supported) sent = sendOBDRequest(p);
      }
      if (!sent) sendOBDRequest(0x00);   // keep-alive so we stay in LIVE mode
    }
  } else if (millis() - lastProbe > 1000) {
    lastProbe = millis();
    sendOBDRequest(0x0C);                // probe: is a car there?
  }
}

// ---------------- DEMO data ----------------
void setSim(uint8_t pid, float v) {
  pidData[pid].value = v;
  pidData[pid].hasValue = true;
  pidData[pid].lastSeen = millis();
  pidData[pid].rawLen = 0;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void simulateDemo() {
  static uint32_t last = 0;
  static float rpm = 800, speed = 0, thr = 0, cool = 70, oil = 75, fuel = 72;
  if (millis() - last < 100) return;
  last = millis();

  float t = millis() / 1000.0f;
  int cycle = ((int)t) % 30;
  if (cycle < 5) {                       // idle
    rpm = 800 + sinf(t * 2) * 30;
    speed = clampf(speed - 2, 0, 200);
    thr = 0;
  } else if (cycle < 12) {               // accelerate
    rpm = clampf(rpm + random(40, 120), 800, 5200);
    speed = clampf(speed + random(1, 3), 0, 130);
    thr = clampf(thr + random(2, 6), 0, 80);
  } else if (cycle < 20) {               // cruise
    rpm = 2200 + sinf(t * 3) * 150;
    speed = clampf(speed + random(-1, 2), 90, 120);
    thr = 22 + sinf(t * 2) * 6;
  } else {                               // slow down
    rpm = clampf(rpm - random(40, 120), 900, 6000);
    speed = clampf(speed - random(1, 4), 0, 200);
    thr = clampf(thr - random(2, 6), 0, 100);
  }
  cool = clampf(cool + (rpm > 2500 ? 0.06f : -0.01f), 80, 103);
  oil  = clampf(cool + 6 + sinf(t * 0.2f) * 2, 40, 140);
  fuel = clampf(fuel - 0.0005f, 5, 100);
  float load = clampf(15 + thr * 0.7f, 0, 100);
  float maf  = rpm * load / 2000.0f;

  setSim(0x04, load);
  setSim(0x05, cool);
  setSim(0x06, sinf(t * 1.3f) * 3);
  setSim(0x07, 2.3f);
  setSim(0x0A, 380);
  setSim(0x0B, 30 + load * 0.7f);
  setSim(0x0C, rpm);
  setSim(0x0D, speed);
  setSim(0x0E, 10 + sinf(t) * 5);
  setSim(0x0F, 32 + sinf(t * 0.1f) * 6);
  setSim(0x10, maf);
  setSim(0x11, thr);
  setSim(0x1F, millis() / 1000.0f);
  setSim(0x21, 0);
  setSim(0x2F, fuel);
  setSim(0x31, 1840);
  setSim(0x33, 101);
  setSim(0x42, 14.1f + sinf(t * 0.7f) * 0.2f);
  setSim(0x45, thr * 0.9f);
  setSim(0x46, 29 + sinf(t * 0.05f) * 2);
  setSim(0x49, thr);
  setSim(0x5C, oil);
  setSim(0x5E, maf * 0.33f);
}
