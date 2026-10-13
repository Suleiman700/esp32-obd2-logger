#pragma once
#include <Arduino.h>

// =====================================================================
//  Known OBD-II Mode 01 PIDs and how to decode them
// =====================================================================

enum Formula : uint8_t {
  F_A,        // A
  F_A_M40,    // A - 40           (temperatures)
  F_PCT,      // A * 100 / 255    (percent)
  F_RPM,      // (256A + B) / 4
  F_AB,       // 256A + B
  F_AB_100,   // (256A + B) / 100
  F_AB_1000,  // (256A + B) / 1000
  F_TRIM,     // (A - 128) * 100 / 128
  F_TIMING,   // A / 2 - 64
  F_A3,       // A * 3
  F_AB_20     // (256A + B) / 20
};

struct PidDef {
  uint8_t pid;
  const char* key;    // short name used in streamed JSON
  const char* name;   // display name
  const char* unit;
  Formula f;
};

static const PidDef PID_TABLE[] = {
  {0x04, "load",         "Engine Load",      "%",    F_PCT},
  {0x05, "coolant",      "Coolant Temp",     "C",    F_A_M40},
  {0x06, "stft1",        "Short Fuel Trim",  "%",    F_TRIM},
  {0x07, "ltft1",        "Long Fuel Trim",   "%",    F_TRIM},
  {0x0A, "fuel_press",   "Fuel Pressure",    "kPa",  F_A3},
  {0x0B, "map",          "Intake MAP",       "kPa",  F_A},
  {0x0C, "rpm",          "Engine RPM",       "rpm",  F_RPM},
  {0x0D, "speed",        "Speed",            "km/h", F_A},
  {0x0E, "timing",       "Timing Advance",   "deg",  F_TIMING},
  {0x0F, "intake",       "Intake Temp",      "C",    F_A_M40},
  {0x10, "maf",          "MAF Rate",         "g/s",  F_AB_100},
  {0x11, "throttle",     "Throttle",         "%",    F_PCT},
  {0x1F, "runtime",      "Run Time",         "s",    F_AB},
  {0x21, "dist_mil",     "Dist. with MIL",   "km",   F_AB},
  {0x2F, "fuel",         "Fuel Level",       "%",    F_PCT},
  {0x31, "dist_clr",     "Dist. since clear","km",   F_AB},
  {0x33, "baro",         "Baro Pressure",    "kPa",  F_A},
  {0x42, "voltage",      "Battery Voltage",  "V",    F_AB_1000},
  {0x45, "rel_throttle", "Rel. Throttle",    "%",    F_PCT},
  {0x46, "ambient",      "Ambient Temp",     "C",    F_A_M40},
  {0x49, "pedal",        "Accel Pedal",      "%",    F_PCT},
  {0x5C, "oil",          "Oil Temp",         "C",    F_A_M40},
  {0x5E, "fuel_rate",    "Fuel Rate",        "L/h",  F_AB_20},
};
static const uint8_t PID_TABLE_COUNT = sizeof(PID_TABLE) / sizeof(PID_TABLE[0]);

// ---------------------------------------------------------------------
//  Custom values (manufacturer data, Mode 21/22) found with the Scanner.
//  They use the unused ids 0xF0..0xFB so they work everywhere a normal PID does.
// ---------------------------------------------------------------------
#define CUSTOM_BASE 0xF0
#define MAX_CUSTOM  12

struct CustomPid {
  bool     used;
  uint16_t ecu;        // request CAN id, e.g. 0x7E0 (reply comes from ecu + 8)
  uint8_t  mode;       // service, e.g. 0x21 or 0x22
  uint16_t did;        // id after the service byte (e.g. 0x01 for 2101, 0x0101 for 220101)
  uint8_t  didLen;     // 0, 1 or 2 bytes of id
  uint8_t  byteIdx;    // which data byte (0 = first byte after the header)
  uint8_t  size;       // 1 or 2 bytes (2 = big-endian A*256+B)
  int8_t   bit;        // -1 = whole value, 0..7 = single bit (on/off)
  float    mul, add;   // value = raw * mul + add
  char     name[20];
  char     unit[8];
};
CustomPid customPids[MAX_CUSTOM];

bool isCustomId(uint8_t id) {
  return id >= CUSTOM_BASE && id < CUSTOM_BASE + MAX_CUSTOM && customPids[id - CUSTOM_BASE].used;
}

bool decodeCustom(const CustomPid& c, const uint8_t* data, uint16_t n, float& out) {
  if (c.byteIdx + c.size > n) return false;
  uint32_t raw = c.size == 2 ? ((uint32_t)data[c.byteIdx] << 8) | data[c.byteIdx + 1] : data[c.byteIdx];
  if (c.bit >= 0) raw = (raw >> c.bit) & 1;
  out = raw * c.mul + c.add;
  return true;
}

const PidDef* findPid(uint8_t pid) {
  for (uint8_t i = 0; i < PID_TABLE_COUNT; i++)
    if (PID_TABLE[i].pid == pid) return &PID_TABLE[i];
  return nullptr;
}

float decodePid(const PidDef* d, const uint8_t* b, uint8_t n) {
  float A = n > 0 ? b[0] : 0;
  float B = n > 1 ? b[1] : 0;
  if (!d) return n >= 2 ? A * 256.0f + B : A;   // unknown PID: raw number
  switch (d->f) {
    case F_A:       return A;
    case F_A_M40:   return A - 40.0f;
    case F_PCT:     return A * 100.0f / 255.0f;
    case F_RPM:     return (A * 256.0f + B) / 4.0f;
    case F_AB:      return A * 256.0f + B;
    case F_AB_100:  return (A * 256.0f + B) / 100.0f;
    case F_AB_1000: return (A * 256.0f + B) / 1000.0f;
    case F_TRIM:    return (A - 128.0f) * 100.0f / 128.0f;
    case F_TIMING:  return A / 2.0f - 64.0f;
    case F_A3:      return A * 3.0f;
    case F_AB_20:   return (A * 256.0f + B) / 20.0f;
  }
  return A;
}

String pidKey(uint8_t pid) {
  if (isCustomId(pid)) {
    String k = "c_";
    for (const char* p = customPids[pid - CUSTOM_BASE].name; *p; p++) {
      char c = *p;
      if (isalnum((unsigned char)c)) k += (char)tolower((unsigned char)c);
      else if (k[k.length() - 1] != '_') k += '_';
    }
    return k;
  }
  const PidDef* d = findPid(pid);
  if (d) return d->key;
  char buf[10];
  snprintf(buf, sizeof(buf), "pid_%02X", pid);
  return buf;
}

String pidName(uint8_t pid) {
  if (isCustomId(pid)) return customPids[pid - CUSTOM_BASE].name;
  const PidDef* d = findPid(pid);
  if (d) return d->name;
  char buf[12];
  snprintf(buf, sizeof(buf), "PID 0x%02X", pid);
  return buf;
}

const char* pidUnit(uint8_t pid) {
  if (isCustomId(pid)) return customPids[pid - CUSTOM_BASE].unit;
  const PidDef* d = findPid(pid);
  return d ? d->unit : "";
}

// How many decimals to show for a PID value
uint8_t pidDecimals(uint8_t pid, float v) {
  if (isCustomId(pid)) {
    const CustomPid& c = customPids[pid - CUSTOM_BASE];
    if (c.bit >= 0 || (c.mul == roundf(c.mul) && c.add == roundf(c.add))) return 0;
    return fabsf(v) >= 100.0f ? 0 : 1;
  }
  const PidDef* d = findPid(pid);
  if (!d) return 0;
  switch (d->f) {
    case F_A: case F_A_M40: case F_RPM: case F_AB: case F_A3: return 0;
    case F_AB_1000: return 1;
    default: return fabsf(v) >= 100.0f ? 0 : 1;
  }
}
