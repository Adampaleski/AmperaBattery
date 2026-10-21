#include "Settings.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <stdlib.h>
#include "Config.h"
#include "CurrentSense.h"
#include "BmsCore.h"
#include "Thresholds.h"
#include "StringConfig.h"

namespace Settings {
namespace {
constexpr uint32_t kMagic = 0x54525832;  // "TRX2" (TRX1 had ceil_mV; old EEPROM -> defaults)
constexpr int      kAddr  = 0;
struct Blob {
    uint32_t magic;
    uint8_t  pct;
    uint16_t low_mV;
    uint8_t  db_mV;
    uint16_t off1;    // current zero, fraction of Vc x 10000 (0 = default)
    uint16_t off2;
    uint8_t  check;
};
Blob  g{};
float g_simBus = 0.0f;

uint8_t eeRead(int a) { return EEPROM.read(a); }
void    eeWrite(int a, uint8_t v) { EEPROM.update(a, v); }
const BmsCore::LogStore kThrStore{eeRead, eeWrite};
BmsCore::Thresholds g_thr(kThrStore);
void thrOut(const char *s) { SERIALCONSOLE.print(s); }

uint8_t sum(const Blob &b) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&b);
    uint8_t s = 0xA5;
    for (unsigned i = 0; i < sizeof(Blob) - 1; i++) s = static_cast<uint8_t>(s * 31 + p[i]);
    return s;
}
bool valid(const Blob &b) {
    return b.magic == kMagic && b.check == sum(b) &&
           b.pct >= kPctMin && b.pct <= kPctMax &&
           b.low_mV >= kLowMin * 1000 - 0.5f && b.low_mV <= kLowMax * 1000 + 0.5f &&
           b.db_mV >= kDbMin && b.db_mV <= kDbMax;
}
void defaults() {
    g.magic = kMagic;
    g.pct     = kPctDef;
    g.off1    = 0;
    g.off2    = 0;
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
    const bool thrOk = g_thr.load();
    SERIALCONSOLE.print(F("Thresholds (EEPROM "));
    SERIALCONSOLE.print(BmsCore::Thresholds::kBase);
    SERIALCONSOLE.print(F("): "));
    SERIALCONSOLE.print(thrOk ? g_thr.overrides() : 0);
    SERIALCONSOLE.println(thrOk ? F(" overridden (T to list)") : F("none saved, code defaults (T to list)"));
}

void applyThresholds(BmsCore::Config &c) { g_thr.apply(c); }

int   chargePct()    { return g.pct; }
float ceilingV() {
    float v = BmsCore::ceilingForPct(g.pct);
#if DUAL_STRING
    const float clamp = (KELLY_MAX_V - KELLY_MARGIN_V) / static_cast<float>(STRING_EXPECT_S);
    if (v > clamp) v = clamp;
#endif
    return v;
}
float curOff1Frac()  { return g.off1 / 10000.0f; }
float curOff2Frac()  { return g.off2 / 10000.0f; }
void  setCurOffsets(float f1, float f2) {
    g.off1 = static_cast<uint16_t>(f1 * 10000 + 0.5f);
    g.off2 = static_cast<uint16_t>(f2 * 10000 + 0.5f);
    save();
}
float chargeFaultV() { const float f = ceilingV() + 0.05f; return f > 4.20f ? 4.20f : f; }
float lowCutoffV()   { return g.low_mV / 1000.0f; }
float deadbandV()    { return g.db_mV / 1000.0f; }
float simBusV()      { return g_simBus; }

void print() {
    SERIALCONSOLE.print(F("charge_limit="));
    SERIALCONSOLE.print(g.pct);
    SERIALCONSOLE.print(F("% (P50-100) -> ceiling="));
    SERIALCONSOLE.print(ceilingV(), 3);
    SERIALCONSOLE.print(F("V/cell  low_cutoff="));
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
    if (k == 'T') {
        BmsCore::Config base;
        base.lowCutV = lowCutoffV();
        g_thr.setBase(base);
        g_thr.handleLine(line, thrOut);
        return true;
    }
    switch (k) {
        case 'P':
            if (!num) break;
            if (v < kPctMin || v > kPctMax) { refuse(F("charge_limit_%"), v, kPctMin, kPctMax, 0); return true; }
            g.pct = static_cast<uint8_t>(v + 0.5f); save();
            SERIALCONSOLE.print(F("SETTINGS saved: ")); print(); return true;
        case 'V':
            SERIALCONSOLE.println(F("V<volts> is retired: set the charge limit in percent, e.g. P80 (3.95 V) / P100 (4.10 V)"));
            return true;
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
        case 'Z':
            SERIALCONSOLE.println(CurrentSense::calibrateZero()
                ? F("CURRENT zero calibrated and saved (do this with contactors OPEN / no current)")
                : F("CURRENT zero refused: sensor not fitted/enabled or reading implausible"));
            CurrentSense::printStatus();
            return true;
        case 'I':
            // Bench dry-run only: pretend pack current is v amps (I0 = off).
            CurrentSense::setSimAmps(num ? v : 0.0f, num && v != 0.0f);
            CurrentSense::printStatus();
            return true;
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
    SERIALCONSOLE.println(F("' (use P80 / L3.30 / W10 / X / T / TocA 600 / U145.0 / I120 / Z then Enter)"));
    return true;
}
}  // namespace Settings
