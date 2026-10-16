ESP32 OBD-II MONITOR v3.1 - SETUP & USE
========================================

INSTALL
  1. Put the whole "obd_monitor" folder in Documents\Arduino\
     (all .h files must stay next to obd_monitor.ino)
  2. User_Setup.h -> Documents\Arduino\libraries\TFT_eSPI\User_Setup.h (same as before)
  3. Arduino IDE: Board "ESP32 Dev Module", esp32 core 2.0.17, Partition "Default 4MB with spiffs"
  4. Open obd_monitor.ino and upload. No extra libraries needed besides TFT_eSPI.

WIRING (new part = buzzer)
  Display      same as before
  Push button  GPIO 13 + GND
  BUZZER       + leg -> GPIO 25 (D25),  - leg -> GND
               (3-pin buzzer module: VCC->3.3V, I/O->GPIO 25, GND->GND)
  CAN board    TX -> GPIO 16 (RX2), RX -> GPIO 5, 3.3V, GND
  OBD port     CANH -> pin 6, CANL -> pin 14, ESP32 GND -> pin 5

FIRST USE
  - Phone: join WiFi "OBD-Monitor", password 12345678
  - Browser: http://192.168.4.1
  - Set your internet WiFi either in Settings (web) or on the screen:
    WIFI page -> "Change WiFi" -> tap network -> type password -> Connect
  - Touch not working / taps in the wrong place? Calibrate (press 3 crosses):
      * web panel: Settings -> "Calibrate touch screen", or
      * hold the push button 2 s while powering on, or
      * WIFI page -> "Touch cal."
    Live touch values: http://192.168.4.1/api/touch (press the screen and refresh)
    Serial Monitor also prints every touch: raw x/y/z and screen position.

SCREEN
  Header: DEMO/LIVE + WiFi icon (green check = connected, yellow spinner = connecting, red X = not connected)
  Red bar under header = alert (blinks). Push button or tap the bar = silence buzzer.
  Pages: DASH | FAV (your 6 favorites) | ALERTS (rules + status) | WIFI (network, stream, log status)
  Push button: next page (or silences the alarm when it is beeping)

WEB PANEL TABS
  Live              all values being read
  Query PID         read any Mode 01 PID by hex number, add it to the log list
  Favorites & Log   tick what to log/stream (max 16) and what to show on FAV screen (max 6)
  Alerts            rules like "Coolant Temp above 105" (max 8)
  Settings          WiFi, hotspot, stream URL + interval, sample interval, alerts URL

LOGGING / STREAMING
  - Every "Save a sample every" seconds the logged PIDs are saved to flash.
  - Every "Send every" seconds, waiting samples are POSTed to the Data URL (40 per request).
  - No WiFi? Samples keep collecting (up to ~700 KB, about 10+ hours at 5 s).
    When WiFi comes back everything is sent quickly in a row, then deleted.
  - Samples taken before the clock was synced get the correct time added when sent.

DATA URL - JSON the ESP sends:
  {"device":"OBD-Monitor","type":"log","records":[
     {"ts":1760270000,"boot":7,"uptime":512,"mode":"live",
      "values":{"rpm":2210.00,"speed":54.00,"coolant":91.00,"voltage":14.12}}
  ]}
  ts = unix time (UTC) or null if unknown. mode "demo" = fake data.

ALERTS URL - JSON the ESP sends (right away when triggered and when cleared):
  {"device":"OBD-Monitor","type":"alert","records":[
     {"ts":1760270012,"event":"triggered","pid":"0x05","key":"coolant","name":"Coolant Temp",
      "condition":">","threshold":105,"value":106.00,"unit":"C","mode":"live"}
  ]}

  Your endpoint must answer HTTP 200-299, otherwise the ESP keeps the data and retries.
  http:// and https:// both work (https without certificate check).

NODE-RED QUICK TEST
  [http in POST /obd] -> [debug] -> [http response 200]
  Data URL: http://<home-assistant-ip>:1880/endpoint/obd  (path depends on your Node-RED setup)
