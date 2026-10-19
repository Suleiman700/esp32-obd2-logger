#pragma once
// =====================================================================
//  Big value icons for the main screen. Drawn with simple shapes on a
//  48 x 48 design grid, scaled to any size. Uses the global "tft".
// =====================================================================

struct IconPen { int ox, oy; float k; uint16_t c, bg; };

static inline int icX(const IconPen& p, float v) { return p.ox + (int)roundf(v * p.k); }
static inline int icY(const IconPen& p, float v) { return p.oy + (int)roundf(v * p.k); }
static inline int icL(const IconPen& p, float v) { int r = (int)roundf(v * p.k); return r < 1 ? 1 : r; }

static void icLine(const IconPen& p, float ax, float ay, float bx, float by, float w, uint16_t col) {
  float wd = w * p.k; if (wd < 1.5f) wd = 1.5f;
  tft.drawWideLine(p.ox + ax * p.k, p.oy + ay * p.k, p.ox + bx * p.k, p.oy + by * p.k, wd, col, p.bg);
}
static void icRect(const IconPen& p, float x, float y, float w, float h, uint16_t col) {
  tft.fillRect(icX(p, x), icY(p, y), icL(p, w), icL(p, h), col);
}
static void icRRect(const IconPen& p, float x, float y, float w, float h, float r, uint16_t col) {
  tft.fillRoundRect(icX(p, x), icY(p, y), icL(p, w), icL(p, h), icL(p, r), col);
}
static void icCircle(const IconPen& p, float x, float y, float r, uint16_t col) {
  tft.fillCircle(icX(p, x), icY(p, y), icL(p, r), col);
}
static void icTri(const IconPen& p, float ax, float ay, float bx, float by, float cx, float cy, uint16_t col) {
  tft.fillTriangle(icX(p, ax), icY(p, ay), icX(p, bx), icY(p, by), icX(p, cx), icY(p, cy), col);
}
// angles: 0 = bottom (6 o'clock), clockwise
static void icArc(const IconPen& p, float cx, float cy, float r, float th, uint16_t a0, uint16_t a1, uint16_t col) {
  int ro = icL(p, r), ri = ro - icL(p, th);
  if (ri < 1) ri = 1;
  tft.drawSmoothArc(icX(p, cx), icY(p, cy), ro, ri, a0, a1, col, p.bg, true);
}

static void icThermo(const IconPen& p, bool waves) {
  float sx = waves ? 24 : 20;
  icRRect(p, sx - 6, 3, 12, 30, 6, p.c);           // tube
  icRRect(p, sx - 3, 6, 6, 24, 3, p.bg);           // glass
  icRect(p, sx - 1.5f, 15, 3, 18, p.c);            // mercury
  icCircle(p, sx, 37, 9, p.c);                     // bulb
  if (waves) {                                      // coolant = thermometer in water
    for (int row = 0; row < 2; row++) {
      float y = 38 + row * 7;
      icLine(p, 1, y, 5, y - 4, 2.5f, p.c); icLine(p, 5, y - 4, 9, y, 2.5f, p.c); icLine(p, 9, y, 13, y - 4, 2.5f, p.c);
      icLine(p, 35, y - 4, 39, y, 2.5f, p.c); icLine(p, 39, y, 43, y - 4, 2.5f, p.c); icLine(p, 43, y - 4, 47, y, 2.5f, p.c);
    }
  } else {
    icLine(p, 30, 10, 38, 10, 2.5f, p.c); icLine(p, 30, 17, 38, 17, 2.5f, p.c); icLine(p, 30, 24, 38, 24, 2.5f, p.c);
  }
}

static void icGauge(const IconPen& p, bool redZone) {
  icArc(p, 24, 27, 22, 4.5f, 45, 315, p.c);
  if (redZone) icArc(p, 24, 27, 22, 4.5f, 245, 315, 0xF800);
  for (int a = 70; a <= 290; a += 55) {             // ticks
    float r = a * 0.0174533f;
    icLine(p, 24 - 13 * sinf(r), 27 + 13 * cosf(r), 24 - 16 * sinf(r), 27 + 16 * cosf(r), 2.5f, p.c);
  }
  icLine(p, 24, 27, 35, 14, 4, p.c);               // needle
  icCircle(p, 24, 27, 4.5f, p.c);
  if (!redZone) icRect(p, 14, 41, 20, 4, p.c);      // speed: display window under the needle
}

static void icFuel(const IconPen& p) {
  icRRect(p, 5, 5, 24, 38, 3, p.c);                // pump
  icRect(p, 9, 9, 16, 11, p.bg);                   // window
  icRect(p, 2, 42, 30, 5, p.c);                    // base
  icLine(p, 29, 12, 38, 12, 3, p.c);               // hose
  icLine(p, 38, 12, 43, 18, 3, p.c);
  icLine(p, 43, 18, 43, 34, 3, p.c);
  icLine(p, 43, 34, 39, 38, 3, p.c);
  icLine(p, 39, 38, 34, 34, 3, p.c);
}

static void icBattery(const IconPen& p) {
  icRect(p, 9, 9, 8, 6, p.c);                      // terminals
  icRect(p, 31, 9, 8, 6, p.c);
  icRRect(p, 3, 14, 42, 30, 3, p.c);               // body
  icRect(p, 8, 27, 11, 4, p.bg);                   // -
  icRect(p, 29, 27, 11, 4, p.bg);                  // +
  icRect(p, 32.5f, 23.5f, 4, 11, p.bg);
}

static void icPedal(const IconPen& p) {
  icTri(p, 16, 3, 36, 3, 31, 37, p.c);             // pad (slanted)
  icTri(p, 16, 3, 31, 37, 11, 37, p.c);
  icLine(p, 20, 11, 30, 11, 2.5f, p.bg);           // grip lines
  icLine(p, 19, 19, 29, 19, 2.5f, p.bg);
  icLine(p, 18, 27, 28, 27, 2.5f, p.bg);
  icLine(p, 21, 37, 24, 44, 4, p.c);               // hinge
  icLine(p, 6, 45, 42, 45, 4, p.c);                // floor
}

static void icOil(const IconPen& p) {
  icRRect(p, 6, 22, 26, 19, 3, p.c);               // can
  icRect(p, 14, 16, 8, 7, p.c);                    // neck
  icRect(p, 11, 13, 14, 4, p.c);                   // cap
  icLine(p, 30, 27, 45, 17, 4, p.c);               // spout
  icLine(p, 6, 26, 2, 22, 3, p.c);                 // handle
  icLine(p, 2, 22, 2, 34, 3, p.c);
  icLine(p, 2, 34, 6, 36, 3, p.c);
  icTri(p, 44, 24, 41, 31, 47, 31, p.c);           // drop
  icCircle(p, 44, 32, 3, p.c);
}

static void icEngine(const IconPen& p) {
  icRect(p, 12, 16, 24, 22, p.c);                  // block
  icRect(p, 16, 10, 16, 6, p.c);                   // valve cover
  icRect(p, 20, 6, 8, 4, p.c);
  icRect(p, 4, 22, 8, 10, p.c);                    // left
  icRect(p, 36, 20, 6, 4, p.c);                    // right pipe
  icRect(p, 41, 16, 4, 20, p.c);
  icRect(p, 36, 32, 6, 4, p.c);
  icRect(p, 16, 38, 16, 5, p.c);                   // oil pan
}

static void icClock(const IconPen& p) {
  icArc(p, 24, 24, 20, 4, 0, 360, p.c);
  icLine(p, 24, 24, 24, 11, 3.5f, p.c);
  icLine(p, 24, 24, 33, 29, 3.5f, p.c);
  icCircle(p, 24, 24, 3, p.c);
}

static void icDrop(const IconPen& p) {
  icTri(p, 24, 3, 12, 27, 36, 27, p.c);
  icCircle(p, 24, 31, 13, p.c);
  icLine(p, 17, 30, 19, 37, 2.5f, p.bg);           // shine
}

static void icWind(const IconPen& p) {
  icLine(p, 3, 16, 33, 16, 3.5f, p.c); icArc(p, 33, 10, 7.5f, 3.5f, 180, 360, p.c);   // curl up
  icLine(p, 3, 26, 42, 26, 3.5f, p.c);
  icLine(p, 3, 36, 29, 36, 3.5f, p.c); icArc(p, 29, 42, 7.5f, 3.5f, 180, 360, p.c);   // curl down
}

// Which icon a PID uses: 0 = none
uint8_t pidIconId(uint8_t pid) {
  switch (pid) {
    case 0x05: return 1;                           // coolant
    case 0x0F: case 0x46: return 2;                // air temps
    case 0x0D: return 3;                           // speed
    case 0x0C: return 4;                           // rpm
    case 0x0A: case 0x0B: case 0x33: return 5;     // pressures
    case 0x2F: return 6;                           // fuel level
    case 0x42: return 7;                           // battery
    case 0x11: case 0x45: case 0x49: return 8;     // throttle / pedal
    case 0x5C: return 9;                           // oil
    case 0x04: return 10;                          // engine load
    case 0x1F: return 11;                          // run time
    case 0x5E: return 12;                          // fuel rate
    case 0x10: return 13;                          // air flow
    default:   return 0;
  }
}

// Draws the icon in an S x S square at (x, y). Returns false if the PID has no icon.
bool drawPidIcon(uint8_t pid, int x, int y, int S, uint16_t col, uint16_t bg) {
  IconPen p{x, y, S / 48.0f, col, bg};
  switch (pidIconId(pid)) {
    case 1:  icThermo(p, true); break;
    case 2:  icThermo(p, false); break;
    case 3:  icGauge(p, false); break;
    case 4:  icGauge(p, true); break;
    case 5:  icGauge(p, false); break;
    case 6:  icFuel(p); break;
    case 7:  icBattery(p); break;
    case 8:  icPedal(p); break;
    case 9:  icOil(p); break;
    case 10: icEngine(p); break;
    case 11: icClock(p); break;
    case 12: icDrop(p); break;
    case 13: icWind(p); break;
    default: return false;
  }
  return true;
}
