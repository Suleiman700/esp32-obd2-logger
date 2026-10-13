# ESP32 OBD-II Monitor

OBD-II monitor for a 2016 Hyundai Sonata Hybrid: ESP32 + SN65HVD230 CAN board + 2.4" ILI9341 touch TFT,
with a hotspot web panel, logging, alerts and streaming to an n8n webhook.

## Features
- Reads OBD-II Mode 01 PIDs over CAN (500 kbps, 11-bit) — DEMO data when no car answers
- Landscape UI with LIVE / DEMO / LOST badge, spinner, save icon
- Own WiFi hotspot `OBD-Monitor` → panel at `http://192.168.4.1`: live data, PID query, favorites, logging, alert rules, settings
- Joins your WiFi (set from the panel or on the screen) and streams logged data to your webhook (POST), with an offline backlog
- Buzzer alerts; push button cycles pages / silences alarms

## Layout
| Folder | Contents |
|---|---|
| `firmware/obd_monitor/` | Arduino sketch (v3.0, landscape) + `WIRING.txt` |
| `firmware/tft_espi_setup/User_Setup.h` | TFT_eSPI config — copy into the TFT_eSPI library folder |
| `case/v2/` | 3D-print case v2 (2026-10-13): shifted lid (tested), 4.5 cm vents box, honeycomb box, generator scripts |
| `case/mockups/` | Case renders |

## Build
- Arduino IDE, board **ESP32 Dev Module**, **esp32 core 2.0.17** (3.x gives a white screen with TFT_eSPI)
- Library: TFT_eSPI with the included `User_Setup.h`
- Partition: Default 4MB with spiffs (or Huge APP if the sketch is too big)

## Pins
| Part | ESP32 |
|---|---|
| CAN TX / RX | GPIO16 / GPIO5 |
| Button | GPIO13 → GND |
| Buzzer | GPIO25 |
| Display | CS 15, DC 2, RST 4, MOSI 23, SCLK 18, MISO 19, T_CS 22 |

Power in the car: OBD pin 16 (+12 V) → fuse → LM2596 set to 5.0 V → ESP32 VIN. CANH → OBD 6, CANL → OBD 14, GND → OBD 5.
See `firmware/obd_monitor/WIRING.txt` for the full table.

## Case
Print one box + the lid from `case/v2/`. The honeycomb box uses ~5.1 m filament, the vents box ~9.1 m.
Regenerate with `pip install manifold3d trimesh numpy` and the `make_*.py` scripts.
