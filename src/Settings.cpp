#include "Settings.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <stdlib.h>
#include "Config.h"

namespace Settings {
namespace {
constexpr uint32_t kMagic = 0x54525831;  // "TRX1"
constexpr int      kAddr  = 0;
struct Blob {
    uint32_t magic;
    uint16_t ceil_mV;
    uint16_t low_mV;
    uint8_t  db_mV;
    uint8_t  check;
};
Blob  g{};
float g_simBus = 0.0f;

uint8_t sum(const Blob &b) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&b);
    uint8_t s = 0xA5;
    for (unsigned i = 0; i < sizeof(Blob) - 1; i++) s = static_cast<uint8_t>(s * 31 + p[i]);
    return s;
}
bool valid(const Blob &b) {
    return b.magic == kMagic && b.check == sum(b) &&
           b.ceil_mV >= kCeilMin * 1000 - 0.5f && b.ceil_mV <= kCeilMax * 1000 + 0.5f &&
           b.low_mV >= kLowMin * 1000 - 0.5f && b.low_mV <= kLowMax * 1000 + 0.5f &&
           b.db_mV >= kDbMin && b.db_mV <= kDbMax;
}
void defaults() {
    g.magic = kMagic;
    g.ceil_mV = static_cast<uint16_t>(kCeilDef * 1000 + 0.5f);
    g.low_mV  = static_cast<uint16_t>(kLowDef * 1000 + 0.5f);
    g.db_mV   = kDbDef;
}
void save() {
    g.check = sum(g);
    EEPROM.put(kAddr, g);
}
void refuse(const __FlashStringHelper *what, float v, float lo, float hi, int dp) {
    SERIALCONSOLE.print(F("SETTINGS REFUSED: "));
    SERIALCONSOLE.print(what);
    SERIALCONSOLE.print('=');
    SERIALCONSOLE.print(v, dp);
    SERIALCONSOLE.print(F(" outside "));
    SERIALCONSOLE.print(lo, dp);
    SERIALCONSOLE.print('-');
    SERIALCONSOLE.print(hi, dp);
    SERIALCONSOLE.println(F(" (unchanged)"));
}
}  // namespace

void begin() {
    EEPROM.get(kAddr, g);
    const bool ok = valid(g);
    if (!ok) { defaults(); save(); }
    SERIALCONSOLE.print(ok ? F("Settings (EEPROM): ") : F("Settings (EEPROM blank/invalid -> DEFAULTS saved): "));
    print();
}

float ceilingV()     { return g.ceil_mV / 1000.0f; }
float chargeFaultV() { const float f = ceilingV() + 0.05f; return f > 4.20f ? 4.20f : f; }
float lowCutoffV()   { return g.low_mV / 1000.0f; }
float deadbandV()    { return g.db_mV / 1000.0f; }
float simBusV()      { return g_simBus; }

void print() {
    SERIALCONSOLE.print(F("ceiling="));
    SERIALCONSOLE.print(ceilingV(), 3);
    SERIALCONSOLE.print(F("V (V3.90-4.15)  low_cutoff="));
    SERIALCONSOLE.print(lowCutoffV(), 3);
    SERIALCONSOLE.print(F("V (L3.00-3.50)  deadband="));
    SERIALCONSOLE.print(g.db_mV);
    SERIALCONSOLE.print(F("mV (W2-50)  charge_fault_latch="));
    SERIALCONSOLE.print(chargeFaultV(), 3);
    SERIALCONSOLE.print('V');
    if (g_simBus > 0.0f) { SERIALCONSOLE.print(F("  SIM_BUS=")); SERIALCONSOLE.print(g_simBus, 1); SERIALCONSOLE.print('V'); }
    SERIALCONSOLE.println();
}

bool handleLine(const char *line) {
    if (!line || !line[0]) return false;
    const char k = line[0];
    const char *arg = line + 1;
    char *end = nullptr;
    const float v = static_cast<float>(strtod(arg, &end));
    const bool num = end && end != arg;
    switch (k) {
        case 'V':
            if (!num) break;
            if (v < kCeilMin - 1e-4f || v > kCeilMax + 1e-4f) { refuse(F("ceiling"), v, kCeilMin, kCeilMax, 2); return true; }
            g.ceil_mV = static_cast<uint16_t>(v * 1000 + 0.5f); save();
            SERIALCONSOLE.print(F("SETTINGS saved: ")); print(); return true;
        case 'L':
            if (!num) break;
            if (v < kLowMin - 1e-4f || v > kLowMax + 1e-4f) { refuse(F("low_cutoff"), v, kLowMin, kLowMax, 2); return true; }
            g.low_mV = static_cast<uint16_t>(v * 1000 + 0.5f); save();
            SERIALCONSOLE.print(F("SETTINGS saved: ")); print(); return true;
        case 'W':
            if (!num) break;
            if (v < kDbMin || v > kDbMax) { refuse(F("deadband_mV"), v, kDbMin, kDbMax, 0); return true; }
            g.db_mV = static_cast<uint8_t>(v + 0.5f); save();
            SERIALCONSOLE.print(F("SETTINGS saved: ")); print(); return true;
        case 'X':
            defaults(); save();
            SERIALCONSOLE.print(F("SETTINGS defaults restored: ")); print(); return true;
        case 'U':
            // Bench dry-run only: pretend the HV bus reads v volts (0 = off). Never drives anything.
            g_simBus = (num && v > 0.0f && v < 400.0f) ? v : 0.0f;
            SERIALCONSOLE.print(F("SIM bus voltage (dry-run precharge only) = "));
            if (g_simBus > 0.0f) { SERIALCONSOLE.print(g_simBus, 1); SERIALCONSOLE.println('V'); }
            else SERIALCONSOLE.println(F("OFF"));
            return true;
        default: break;
    }
    SERIALCONSOLE.print(F("Bad settings line '"));
    SERIALCONSOLE.print(line);
    SERIALCONSOLE.println(F("' (use V4.05 / L3.30 / W10 / X / U145.0 then Enter)"));
    return true;
}
}  // namespace Settings
