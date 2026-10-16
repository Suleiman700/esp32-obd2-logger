/*
  =====================================================================
   ESP32 OBD-II Monitor  v3.2
  =====================================================================
   - Reads OBD-II over CAN (SN65HVD230), DEMO data when no car answers
   - Own WiFi hotspot with a web panel:  http://192.168.4.1
       Live values, query any PID, favorites, logging, alert rules, settings
   - Joins your WiFi (set on the web panel OR on the screen: WIFI page)
   - Logs chosen PIDs to flash, streams them to your URL every X seconds.
     Offline? Keeps logging and sends everything when WiFi is back.
   - Alert rules (e.g. Coolant > 105): buzzer + red bar on screen +
     POST to your alerts URL. Push button silences the buzzer.

   Arduino IDE:
     Board:            ESP32 Dev Module   (esp32 core 2.0.17)
     Partition Scheme: Default 4MB with spiffs (if "Sketch too big": Huge APP)
     Library:          TFT_eSPI (with your User_Setup.h)

   Wiring:
     Display  -> same as before (see WIRING.txt)
     Button   -> GPIO 13 + GND
     Buzzer   -> GPIO 25 (+) and GND (-)
     CAN TX   -> GPIO 16 (RX2), CAN RX -> GPIO 5
     CANH/CANL-> OBD pin 6 / pin 14, ESP32 GND -> OBD pin 5

   Tip: hold the push button while powering on to calibrate the touch screen.
*/

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "pids.h"
#include "state.h"
#include "settings.h"
#include "obd.h"
#include "net.h"
#include "alerts.h"
#include "display.h"
#include "touchui.h"
#include "web.h"

// Push button: debounced, one action per press
//  - setup screen open -> cancel it
//  - alarm sounding    -> silence
//  - otherwise         -> next page
void handleButton() {
  static bool stableState = HIGH, lastReading = HIGH;
  static uint32_t lastChange = 0;
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) { lastChange = millis(); lastReading = reading; }
  if (millis() - lastChange > 50 && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      if (uiMode != UI_NORMAL) leaveUi();
      else if (buzzerShouldSound()) ackAlerts();
      else nextPage();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nOBD-II Monitor " FW_VERSION " starting...");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tft.init();
  tft.setRotation(SCREEN_ROTATION);   // landscape
  pinMode(BUTTON_PIN, INPUT_PULLUP);    // after tft.init()
  splash("Starting...");

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  loadSettings();
  rebuildPollList();
  loadTouchCal();

  // Button held for 2 seconds at power-on -> touch calibration
  delay(50);
  if (digitalRead(BUTTON_PIN) == LOW) {
    splash("Keep holding for touch calibration...");
    uint32_t t0 = millis();
    while (digitalRead(BUTTON_PIN) == LOW && millis() - t0 < 2000) delay(10);
    if (millis() - t0 >= 2000) {
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);   // wait for release
      runTouchCalibration();
    }
  }
  Serial.printf("Touch raw Z at boot: %u (0 = not pressed or no reply; press the screen to see values)\n", tft.getTouchRawZ());

  splash("Starting CAN bus...");
  canInitialized = canInit();

  splash("Starting WiFi...");
  wifiBegin();
  storageBegin();          // logging + uploads run on the other CPU core
  webBegin();

  splash(canInitialized ? "Ready - waiting for car" : "CAN failed - DEMO only");
  delay(800);
  tft.fillScreen(C_BG);
  needsFullRedraw = true;
  Serial.printf("Hotspot: %s  Panel: http://192.168.4.1\n", cfg.apSsid.c_str());
}

void loop() {
  server.handleClient();
  if (touchCalRequest) { touchCalRequest = false; if (uiMode != UI_NORMAL) leaveUi(); runTouchCalibration(); }

  obdLoop();
  if (demoMode) simulateDemo();

  alertsLoop();
  buzzerLoop();
  handleButton();

  wifiLoop();              // (logging + uploads run in storageTask on core 0)

  if (!uiLoop()) {        // WiFi setup screens take over the display while open
    handleTouch();
    displayLoop();
  }
}
