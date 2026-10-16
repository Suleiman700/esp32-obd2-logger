/*
  =====================================================================
   ESP32 OBD-II Monitor  v3.5
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

   Touch calibration: web panel -> Settings -> Calibrate touch screen.
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

// Push button with noise protection (long wires in a car pick up noise)
//  - a press counts only if the button was released for >= 200 ms before
//    and is then held for >= 80 ms (short glitches are ignored)
//  - held / shorted for more than 3 s = stuck -> ignored until released for 1 s
//  - one action per press:  setup screen open -> cancel,  alarm -> silence,  otherwise -> next page
void handleButton() {
  static uint32_t highSince = 0, lowSince = 0;
  static bool armed = false, fired = false;
  uint32_t now = millis();
  bool low = digitalRead(BUTTON_PIN) == LOW;
  if (!low) {
    if (lowSince) { lowSince = 0; highSince = now; }       // just released
    if (!highSince) highSince = now;
    fired = false;
    if (buttonStuck && now - highSince > 1000) { buttonStuck = false; Serial.println("Button released - active again"); }
    return;
  }
  if (!lowSince) {                                         // just went low
    lowSince = now;
    armed = highSince && (now - highSince >= 200);
  }
  if (!buttonStuck && now - lowSince > 3000) {
    buttonStuck = true;
    Serial.println("Button pin LOW for 3 s - ignoring it (button stuck or wire shorted to GND?)");
  }
  if (!cfg.buttonOn || buttonStuck || fired || !armed) return;
  if (now - lowSince >= 80) {
    fired = true;
    if (uiMode != UI_NORMAL) leaveUi();
    else if (buzzerShouldSound()) ackAlerts();
    else nextPage();
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

  // (touch calibration is started only from the web panel - never automatically at power-on)
  Serial.printf("Button pin at boot: %s\n", digitalRead(BUTTON_PIN) ? "HIGH (released)" : "LOW (pressed or shorted to GND!)");
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
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
  if (otaRunning) return;              // firmware update in progress
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
