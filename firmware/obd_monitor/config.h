#pragma once
// =====================================================================
//  OBD Monitor - hardware pins and limits
// =====================================================================

#define FW_VERSION "3.1"

// ---- Pins (display pins are in TFT_eSPI/User_Setup.h) ----
#define CAN_TX_PIN  GPIO_NUM_16   // CAN board TX  (labelled RX2 on many ESP32 boards)
#define CAN_RX_PIN  GPIO_NUM_5    // CAN board RX
#define BUTTON_PIN  13            // Push button -> GND. Cycles pages / silences buzzer
#define BUZZER_PIN  25            // Buzzer + (other leg to GND)

// ---- Screen orientation ----
// 1 = landscape, 3 = landscape turned 180 degrees (use 3 if the picture is upside down)
// After changing it, run "Touch cal." on the WIFI page once.
#define SCREEN_ROTATION 1

// ---- Hotspot defaults (can be changed in the web panel) ----
#define DEFAULT_AP_SSID "OBD-Monitor"
#define DEFAULT_AP_PASS "12345678"      // min 8 characters

// ---- Time zone for the clock on the screen (POSIX TZ string - change for your region) ----
#define TZ_INFO "IST-2IDT,M3.4.4/26,M10.5.0"

// ---- Limits ----
#define MAX_TRACKED   16      // PIDs saved to the log / streamed
#define MAX_FAVS      6       // PIDs on the FAV screen
#define MAX_RULES     8       // alert rules
#define MAX_POLL      40

// ---- Storage ----
#define LOG_FILE        "/log.csv"
#define ALERT_FILE      "/alerts.csv"
#define MAX_LOG_BYTES   (700UL * 1024UL)   // stop logging when this much is waiting
#define MAX_ALERT_BYTES (32UL * 1024UL)
#define UPLOAD_BATCH    40                 // records per HTTP POST

// ---- Timing ----
#define CAN_POLL_GAP_MS     40     // gap between PID requests in LIVE mode
#define LIVE_TIMEOUT_MS     3000   // no ECU reply for this long -> DEMO mode
#define VALUE_FRESH_MS      5000   // value older than this shows "--"
#define WIFI_CONNECT_TIMEOUT 20000
#define WIFI_RETRY_MS       30000
#define HTTP_TIMEOUT_MS     2500
