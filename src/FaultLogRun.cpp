#include "FaultLogRun.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include "Config.h"
#include "StringConfig.h"
#include "ContactorSeq.h"
#include "CurrentSense.h"
#include "FaultLog.h"

namespace FaultLogRun {

#if DUAL_STRING && ROLE_IS_A
namespace {
uint8_t eeRead(int a) { return EEPROM.read(a); }
void    eeWrite(int a, uint8_t v) { EEPROM.update(a, v); }   // skips unchanged bytes

BmsCore::FaultLog g_log;
uint32_t g_prevFaults = 0;
uint8_t  g_prevDerate = 0;
bool     g_started = false;

// One context value per record, same rule as the simulator (sim/wasm_api.cpp):
// cell mV for CELL-HIGH / CELL-LOW / CHARGE-OV, pack amps for OVERCURRENT.
int16_t valueFor(uint8_t bit, const BmsCore::Outputs &o) {
    if (bit == 2 || bit == 3 || bit == 10) return static_cast<int16_t>(o.minCellV * 1000.0f + 0.5f);
    if (bit == 6 && CurrentSense::valid()) {
        const float a = CurrentSense::amps();
        return static_cast<int16_t>(a > 32000 ? 32000 : (a < -32000 ? -32000 : a));
    }
    return 0;
}

const char *kindName(BmsCore::LogKind k) {
    switch (k) {
        case BmsCore::LogKind::FaultSet:    return "SET   ";
        case BmsCore::LogKind::FaultClear:  return "CLEAR ";
        case BmsCore::LogKind::DerateSet:   return "DERATE";
        case BmsCore::LogKind::DerateClear: return "DR-END";
        default:                            return "NOTE  ";
    }
}
}  // namespace

void begin() {
    g_log.begin({eeRead, eeWrite});
    g_prevFaults = 0; g_prevDerate = 0; g_started = true;
    SERIALCONSOLE.print(F("FAULT LOG: EEPROM "));
    SERIALCONSOLE.print(BmsCore::FaultLog::kBase); SERIALCONSOLE.print('-');
    SERIALCONSOLE.print(BmsCore::FaultLog::kEnd - 1);
    SERIALCONSOLE.print(F(", ")); SERIALCONSOLE.print(g_log.count());
    SERIALCONSOLE.print(F(" saved record(s), boot #")); SERIALCONSOLE.print(g_log.boot());
    SERIALCONSOLE.println(F("  (f = print, FCLEAR = erase)"));
}

void tick() {
    if (!g_started) return;
    const BmsCore::Outputs &o = ContactorSeq::coreOut();
    const uint32_t ms = millis();
    // Active fault word only (like the sim): set = became active, clear = went away.
    const uint32_t now = o.faults, up = now & ~g_prevFaults, down = g_prevFaults & ~now;
    for (uint8_t b = 0; b < 32; b++) {
        if (up & (1u << b))   g_log.add(BmsCore::LogKind::FaultSet, b, ms, valueFor(b, o));
        if (down & (1u << b)) g_log.add(BmsCore::LogKind::FaultClear, b, ms, valueFor(b, o));
    }
    g_prevFaults = now;
    // Derates (Adam 2026-10-21): set value = power % at the start, clear value = lowest %.
    if (o.derateCode != g_prevDerate) {
        if (g_prevDerate) g_log.add(BmsCore::LogKind::DerateClear, g_prevDerate, ms, static_cast<int16_t>(o.derateMinPct + 0.5f));
        if (o.derateCode) g_log.add(BmsCore::LogKind::DerateSet, o.derateCode, ms, static_cast<int16_t>(o.kellyPct + 0.5f));
        g_prevDerate = o.derateCode;
    }
}

void print() {
    SERIALCONSOLE.print(F("--- FAULT LOG (boot #")); SERIALCONSOLE.print(g_log.boot());
    SERIALCONSOLE.print(F(", ")); SERIALCONSOLE.print(g_log.count());
    SERIALCONSOLE.print(F("/")); SERIALCONSOLE.print(BmsCore::FaultLog::kSlots);
    SERIALCONSOLE.print(F(" records, written this boot ")); SERIALCONSOLE.print(g_log.writtenThisBoot());
    SERIALCONSOLE.print('/'); SERIALCONSOLE.print(g_log.maxPerBoot);
    if (g_log.limited()) SERIALCONSOLE.print(F(" LIMIT REACHED"));
    if (g_log.deratesDropped()) { SERIALCONSOLE.print(F(", derates dropped ")); SERIALCONSOLE.print(g_log.deratesDropped()); }
    SERIALCONSOLE.println(F(") newest first ---"));
    BmsCore::LogRec r;
    for (int i = 0; g_log.get(i, r); i++) {
        SERIALCONSOLE.print(F("  boot ")); SERIALCONSOLE.print(r.boot);
        SERIALCONSOLE.print(F("  t=")); SERIALCONSOLE.print(r.ms / 1000.0f, 1);
        SERIALCONSOLE.print(F("s  ")); SERIALCONSOLE.print(kindName(r.kind)); SERIALCONSOLE.print(' ');
        if (r.kind == BmsCore::LogKind::FaultSet || r.kind == BmsCore::LogKind::FaultClear) {
            SERIALCONSOLE.print(BmsCore::faultName(1u << r.code));
            if (r.value && (r.code == 2 || r.code == 3 || r.code == 10)) { SERIALCONSOLE.print(F("  cell ")); SERIALCONSOLE.print(r.value); SERIALCONSOLE.print(F(" mV")); }
            else if (r.value && r.code == 6) { SERIALCONSOLE.print(F("  ")); SERIALCONSOLE.print(r.value); SERIALCONSOLE.print(F(" A")); }
        } else if (r.kind == BmsCore::LogKind::DerateSet || r.kind == BmsCore::LogKind::DerateClear) {
            SERIALCONSOLE.print(BmsCore::derateName(r.code));
            SERIALCONSOLE.print(r.kind == BmsCore::LogKind::DerateSet ? F("  power ") : F("  lowest "));
            SERIALCONSOLE.print(r.value); SERIALCONSOLE.print('%');
        } else {
            SERIALCONSOLE.print(r.code == BmsCore::NOTE_LIMIT ? F("log limit reached this boot") : F("note"));
        }
        SERIALCONSOLE.println();
    }
    if (!g_log.count()) SERIALCONSOLE.println(F("  (empty)"));
}

bool handleLine(const char *line) {
    if (line[0] != 'F') return false;
    if (strcmp(line, "FCLEAR") != 0) {
        SERIALCONSOLE.println(F("FAULT LOG: type FCLEAR then Enter to erase (nothing erased)"));
        return true;
    }
    g_log.clear();
    g_log.begin({eeRead, eeWrite});
    g_prevFaults = 0; g_prevDerate = 0;   // re-log anything still active
    SERIALCONSOLE.println(F("FAULT LOG: erased"));
    return true;
}
#else
void begin() {}
void tick() {}
void print() { SERIALCONSOLE.println(F("FAULT LOG: only on dual-string role A (where BmsCore runs)")); }
bool handleLine(const char *line) { return line[0] == 'F' ? (print(), true) : false; }
#endif

}  // namespace FaultLogRun
