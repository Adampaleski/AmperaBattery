// Factory-bus decoder: up to four Volt BICMs on ONE CAN3 bus, distinguished by
// their factory ID straps. Layout + byte decode from tomdebree/AmperaBattery
// VoltBMS/VoltBMS.ino candecode(); channel→pack-cell map in FactoryCellMap.h.
// Monitor only: no TX here (keep-alive 0x200 lives in CanBus.cpp).

#include "BicmDecode.h"
#include "Config.h"
#include <string.h>

#if FACTORY_BUS_DECODE

#include "FactoryCellMap.h"

namespace BicmDecode {

namespace {

constexpr float    kAlarmLowV  = 0.5f;
constexpr float    kAlarmHighV = 4.3f;
constexpr uint32_t kStaleMs    = kCellStaleMs;  // 3 s, Config.h

// Temperature frames 0x7E0..0x7EF: raw capture only, decode UNVERIFIED.
struct TempFrame {
    uint8_t  buf[8] = {};
    uint8_t  len    = 0;
    uint32_t lastMs = 0;
    uint32_t count  = 0;
};
TempFrame g_temp[16];

struct Channel {
    float    v       = 0.0f;
    uint32_t lastMs  = 0;
    bool     seen    = false;
};

Channel  g_ch[kFactoryModules + 1][kFactoryMaxCh + 1];
uint32_t g_modLastMs[kFactoryModules + 1] = {};
bool     g_modSeen[kFactoryModules + 1]   = {};
uint16_t g_frameCount[kFactoryFrameCount] = {};
uint32_t g_frameLastMs[kFactoryFrameCount] = {};

// Pack cell → (module, ch). 0 = unmapped.
uint8_t g_cellMod[FACTORY_PACK_CELLS + 1] = {};
uint8_t g_cellCh[FACTORY_PACK_CELLS + 1]  = {};

int      g_stableTicks = 0;
bool     g_packStable  = false;
uint32_t g_printLastMs = 0;

// Tom writes `(b0 & 0x0F) << 8 + b1`, which C parses as `<< (8 + b1)`.
// Intended value (and what our K112 path already uses): 12-bit, 1.25 mV/LSB.
float decodeCell(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = static_cast<uint16_t>(((msb & 0x0F) << 8) | lsb);
    return raw * 0.00125f;
}

bool fresh(uint32_t lastMs, bool seen) {
    return seen && (millis() - lastMs) <= kStaleMs;
}

bool validV(float v) { return v >= kAlarmLowV && v <= kAlarmHighV; }

uint8_t mapOf(uint8_t m, uint8_t ch) {
    if (m < 1 || m > kFactoryModules || ch < 1 || ch > kFactoryChannels[m]) return 0;
    return kFactoryMaps[m][ch - 1];
}

// ---- per-circuit status (circuit i = frames 2i and 2i+1) ----
enum class CircState : uint8_t { Empty, Reporting, Partial, Lost };

const FactoryFrameDef &circFrame(int i, int k) { return kFactoryFrames[2 * i + k]; }
uint8_t circModule(int i)  { return circFrame(i, 0).module; }
uint8_t circFirstCh(int i) { return circFrame(i, 0).firstCh; }
uint8_t circCells(int i)   { return circFrame(i, 0).count + circFrame(i, 1).count; }

bool frameFresh(int fi) { return g_frameCount[fi] && fresh(g_frameLastMs[fi], true); }

CircState circState(int i) {
    const bool everA = g_frameCount[2 * i] != 0, everB = g_frameCount[2 * i + 1] != 0;
    if (!everA && !everB) return CircState::Empty;
    const bool fa = frameFresh(2 * i), fb = frameFresh(2 * i + 1);
    if (fa && fb) return CircState::Reporting;
    if (fa || fb) return CircState::Partial;
    return CircState::Lost;
}

// Circuit index for a (module, ch), -1 if none.
int circOf(uint8_t m, uint8_t ch) {
    for (int i = 0; i < kFactoryCircuitCount; i++)
        if (circModule(i) == m && ch >= circFirstCh(i) && ch < circFirstCh(i) + circCells(i))
            return i;
    return -1;
}

const __FlashStringHelper *circName(CircState st) {
    switch (st) {
        case CircState::Reporting: return F("reporting");
        case CircState::Partial:   return F("PARTIAL");
        case CircState::Lost:      return F("SILENT-LOST");
        default:                   return F("silent(empty)");
    }
}

void updateStability() {
    bool ok = !anyAlarm();
    int nOk = 0;
    for (int c = 1; c <= FACTORY_PACK_CELLS && ok; c++) {
        const ChState st = channelState(g_cellMod[c], g_cellCh[c]);
        if (st == ChState::Ok) nOk++;
        else if (st != ChState::NoData && st != ChState::NotWired) ok = false;
    }
    if (nOk == 0) ok = false;
    if (ok) {
        if (g_stableTicks < 255) g_stableTicks++;
    } else {
        g_stableTicks = 0;
    }
    g_packStable = (g_stableTicks >= 3);
}

}  // namespace

void begin() {
    for (int c = 0; c <= FACTORY_PACK_CELLS; c++) { g_cellMod[c] = 0; g_cellCh[c] = 0; }
    for (int m = 1; m <= kFactoryModules; m++) {
        g_modSeen[m] = false;
        for (int ch = 1; ch <= kFactoryChannels[m]; ch++) {
            g_ch[m][ch] = Channel{};
            const uint8_t cell = mapOf(m, ch);
            if (cell) { g_cellMod[cell] = m; g_cellCh[cell] = ch; }
        }
    }
}

void onFrame(const CAN_message_t &msg) {
    if (msg.id >= 0x7E0 && msg.id < 0x7F0) {
        TempFrame &t = g_temp[msg.id - 0x7E0];
        t.len = msg.len > 8 ? 8 : msg.len;
        memcpy(t.buf, msg.buf, t.len);
        t.lastMs = millis();
        t.count++;
        return;  // read-only: no limits, no effect on stability/charge
    }
    for (int i = 0; i < kFactoryFrameCount; i++) {
        const FactoryFrameDef &f = kFactoryFrames[i];
        if (msg.id != f.id) continue;
        if (g_frameCount[i] < 0xFFFF) g_frameCount[i]++;
        const uint32_t now = millis();
        g_frameLastMs[i] = now;
        g_modSeen[f.module]   = true;
        g_modLastMs[f.module] = now;
        for (uint8_t k = 0; k < f.count; k++) {
            const uint8_t b = static_cast<uint8_t>(k * 2);
            if (b + 1 >= msg.len) break;  // short frame: leave rest untouched
            Channel &c = g_ch[f.module][f.firstCh + k];
            c.v      = decodeCell(msg.buf[b], msg.buf[b + 1]);
            c.lastMs = now;
            c.seen   = true;
        }
        return;
    }
}

void tick() {
    updateStability();
    if (millis() - g_printLastMs >= 3000 && g_packStable) {
        g_printLastMs = millis();
        printDecodeDetails();
    }
}

ChState channelState(uint8_t m, uint8_t ch) {
    if (mapOf(m, ch) == 0) return ChState::NotWired;
    const Channel &c = g_ch[m][ch];
    if (!c.seen) return ChState::NoData;                   // circuit never sent: empty, not alarm
    if (!fresh(c.lastMs, c.seen)) return ChState::Alarm;  // sent before, now stale
    return validV(c.v) ? ChState::Ok : ChState::Alarm;
}

float channelVoltage(uint8_t m, uint8_t ch) {
    if (m < 1 || m > kFactoryModules || ch < 1 || ch > kFactoryChannels[m]) return 0.0f;
    return g_ch[m][ch].v;
}

bool modulePresent(uint8_t m) {
    if (m < 1 || m > kFactoryModules) return false;
    return fresh(g_modLastMs[m], g_modSeen[m]);
}

int moduleWired(uint8_t m) {
    int n = 0;
    if (m < 1 || m > kFactoryModules) return 0;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++) if (mapOf(m, ch)) n++;
    return n;
}

int moduleOk(uint8_t m) {
    int n = 0;
    if (m < 1 || m > kFactoryModules) return 0;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++)
        if (channelState(m, ch) == ChState::Ok) n++;
    return n;
}

// Sum of the OK cells in this board's reporting circuits (whole circuits only).
// 0 if any cell in a non-empty circuit is in alarm.
float moduleVoltage(uint8_t m) {
    if (m < 1 || m > kFactoryModules || moduleAlarm(m)) return 0.0f;
    float s = 0.0f;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++)
        if (channelState(m, ch) == ChState::Ok) s += g_ch[m][ch].v;
    return s;
}

bool moduleAlarm(uint8_t m) {
    if (m < 1 || m > kFactoryModules) return false;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++)
        if (channelState(m, ch) == ChState::Alarm) return true;
    return false;
}

bool anyAlarm() {
    for (int m = 1; m <= kFactoryModules; m++) if (moduleAlarm(m)) return true;
    return false;
}

int moduleLive(uint8_t m) {
    if (m < 1 || m > kFactoryModules) return 0;
    int n = 0;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++) {
        const Channel &c = g_ch[m][ch];
        if (fresh(c.lastMs, c.seen) && validV(c.v)) n++;
    }
    return n;
}

int moduleExpected(uint8_t m) {
    if (m < 1 || m > kFactoryModules) return 0;
    if (kFactoryExpectOverride[m] >= 0) return kFactoryExpectOverride[m];
    int n = 0;  // mapped channels in circuits that have ever sent a frame
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++) {
        const int i = circOf(m, static_cast<uint8_t>(ch));
        if (mapOf(m, ch) && i >= 0 && circState(i) != CircState::Empty) n++;
    }
    return n;
}

bool moduleLowLive(uint8_t m) {
    const int e = moduleExpected(m);
    return e > 0 && moduleLive(m) < e;
}

// Report-only. Names the frames that are missing/stale so the cause
// (loose sense plug vs. dropped CAN frame) can be chased at the bench.
void printLowLiveWarnings() {
    for (uint8_t m = 1; m <= kFactoryModules; m++) {
        if (!moduleLowLive(m)) continue;
        SERIALCONSOLE.print(F("  M"));
        SERIALCONSOLE.print(m);
        SERIALCONSOLE.print(' ');
        SERIALCONSOLE.print(kFactorySeries[m]);
        SERIALCONSOLE.print(F("S LOW-LIVE: "));
        SERIALCONSOLE.print(moduleLive(m));
        SERIALCONSOLE.print('/');
        SERIALCONSOLE.print(moduleExpected(m));
        SERIALCONSOLE.print(F(" live cells expected; frames"));
        int missing = 0;
        for (int i = 0; i < kFactoryFrameCount; i++) {
            if (kFactoryFrames[i].module != m) continue;
            if (frameFresh(i) || !g_frameCount[i]) continue;  // never-sent = empty, not missing
            SERIALCONSOLE.print(' ');
            SERIALCONSOLE.print(F("0x"));
            SERIALCONSOLE.print(kFactoryFrames[i].id, HEX);
            missing++;
        }
        if (missing) {
            SERIALCONSOLE.println(F(" missing - check plug/CAN (report only)"));
        } else {
            SERIALCONSOLE.println(F(" all arriving but some cells out of range - check sense plug (report only)"));
        }
    }
}

bool cellPresent(uint8_t cell) {
    if (cell < 1 || cell > FACTORY_PACK_CELLS || !g_cellMod[cell]) return false;
    const Channel &c = g_ch[g_cellMod[cell]][g_cellCh[cell]];
    return fresh(c.lastMs, c.seen);
}

float cellRawVoltage(uint8_t cell) {
    if (!cellPresent(cell)) return 0.0f;
    return g_ch[g_cellMod[cell]][g_cellCh[cell]].v;
}

int seriesCellCount() {
    int n = 0;
    for (int c = 1; c <= FACTORY_PACK_CELLS; c++)
        if (channelState(g_cellMod[c], g_cellCh[c]) == ChState::Ok) n++;
    return n;
}

int moduleCount() {
    int n = 0;
    for (int m = 1; m <= kFactoryModules; m++) if (modulePresent(m)) n++;
    return n;
}

bool packStable() { return g_packStable; }

bool allCellsFresh() {
    if (FACTORY_PACK_CELLS < 1) return false;
    for (int c = 1; c <= FACTORY_PACK_CELLS; c++)
        if (channelState(g_cellMod[c], g_cellCh[c]) != ChState::Ok) return false;
    return true;
}

bool cellAlarmActive() {
    if (anyAlarm()) return true;
    for (int c = 1; c <= FACTORY_PACK_CELLS; c++) {
        const float v = cellVoltage(static_cast<uint8_t>(c));  // 0 if stale
        if (v < kCellFaultMinV || v > kCellFaultMaxV) return true;
    }
    return false;
}

void printTempsJson(Print &out) {
    out.print('[');
    bool first = true;
    for (int i = 0; i < 16; i++) {
        const TempFrame &t = g_temp[i];
        if (!t.count) continue;
        if (!first) out.print(',');
        first = false;
        out.print(F("{\"id\":"));
        out.print(0x7E0 + i);
        out.print(F(",\"hex\":\""));
        for (uint8_t b = 0; b < t.len; b++) {
            if (t.buf[b] < 0x10) out.print('0');
            out.print(t.buf[b], HEX);
        }
        out.print(F("\",\"ageMs\":"));
        out.print(millis() - t.lastMs);
        out.print(F(",\"guessA_C\":"));
        if (t.len >= 2) {
            const uint16_t r = static_cast<uint16_t>(((t.buf[0] & 0x03) << 8) | t.buf[1]);
            out.print(r * 0.0556f - 27.778f, 1);
        } else {
            out.print(F("null"));
        }
        out.print(F(",\"guessB_C\":"));
        if (t.len >= 8) out.print(((t.buf[6] << 8) + t.buf[7]) * -0.0324f + 150.0f, 1);
        else out.print(F("null"));
        out.print('}');
    }
    out.print(']');
}

void printTemps() {
    SERIALCONSOLE.println(F("  TEMPS 0x7E0-0x7EF  *** UNVERIFIED decode, no limits enforced, TEMPS_VERIFIED=0 ***"));
    bool any = false;
    for (int i = 0; i < 16; i++) {
        const TempFrame &t = g_temp[i];
        if (!t.count) continue;
        any = true;
        SERIALCONSOLE.print(F("    "));
        SERIALCONSOLE.print(0x7E0 + i, HEX);
        SERIALCONSOLE.print('#');
        for (uint8_t b = 0; b < t.len; b++) {
            if (t.buf[b] < 0x10) SERIALCONSOLE.print('0');
            SERIALCONSOLE.print(t.buf[b], HEX);
            if (b + 1 < t.len) SERIALCONSOLE.print('.');
        }
        SERIALCONSOLE.print(F("  n="));
        SERIALCONSOLE.print(t.count);
        SERIALCONSOLE.print(F(" age="));
        SERIALCONSOLE.print(millis() - t.lastMs);
        // Guess A: DBC 10-bit b0[1:0]:b1, 0.0556 C/LSB - 27.778
        if (t.len >= 2) {
            const uint16_t r = static_cast<uint16_t>(((t.buf[0] & 0x03) << 8) | t.buf[1]);
            SERIALCONSOLE.print(F("  guessA(dbc)="));
            SERIALCONSOLE.print(r * 0.0556f - 27.778f, 1);
        }
        // Guess B: Tom VoltBMSV2 BMSModule.cpp, b6:b7 * -0.0324 + 150
        if (t.len >= 8) {
            SERIALCONSOLE.print(F("C  guessB(tom)="));
            SERIALCONSOLE.print(((t.buf[6] << 8) + t.buf[7]) * -0.0324f + 150.0f, 1);
        }
        SERIALCONSOLE.println(F("C  UNVERIFIED"));
    }
    if (!any) SERIALCONSOLE.println(F("    (no 0x7E0-0x7EF frames seen)"));
}

// 0 when unmapped, stale or out of range (callers treat 0 as "no value").
float cellVoltage(uint8_t cell) {
    if (cell < 1 || cell > FACTORY_PACK_CELLS) return 0.0f;
    if (channelState(g_cellMod[cell], g_cellCh[cell]) != ChState::Ok) return 0.0f;
    return g_ch[g_cellMod[cell]][g_cellCh[cell]].v;
}

float rangeVoltage(int first, int count) {
    float s = 0.0f;
    for (int c = first; c < first + count; c++) {
        const float v = cellVoltage(static_cast<uint8_t>(c));
        if (v <= 0.0f) return 0.0f;
        s += v;
    }
    return s;
}

// Pack total only when the map declares one series string.
float packVoltage() {
#if FACTORY_SERIES_STRING
    return rangeVoltage(1, FACTORY_PACK_CELLS);
#else
    return 0.0f;
#endif
}

float minCellVoltage() {
    float mn = 99.0f; int n = 0;
    for (int c = 1; c <= FACTORY_PACK_CELLS; c++) {
        const float v = cellVoltage(static_cast<uint8_t>(c));
        if (v > 0.0f) { n++; if (v < mn) mn = v; }
    }
    return n ? mn : 0.0f;
}

float maxCellVoltage() {
    float mx = 0.0f;
    for (int c = 1; c <= FACTORY_PACK_CELLS; c++) {
        const float v = cellVoltage(static_cast<uint8_t>(c));
        if (v > mx) mx = v;
    }
    return mx;
}

void printDecodeDetails() {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("DECODE FACTORY-BUS  boards_present="));
    SERIALCONSOLE.print(moduleCount());
    SERIALCONSOLE.print(F("/4  cells_ok="));
    SERIALCONSOLE.print(seriesCellCount());
    SERIALCONSOLE.print(F("  stable="));
    SERIALCONSOLE.print(g_packStable ? F("Y") : F("N"));
    SERIALCONSOLE.print(F("  ALARM="));
    SERIALCONSOLE.println(anyAlarm() ? F("YES") : F("no"));

    for (int m = 1; m <= kFactoryModules; m++) {
        int okCells = 0;
        for (int ch = 1; ch <= kFactoryChannels[m]; ch++)
            if (channelState(m, ch) == ChState::Ok) okCells++;
        SERIALCONSOLE.print(F("  M"));
        SERIALCONSOLE.print(m);
        SERIALCONSOLE.print(' ');
        SERIALCONSOLE.print(kFactorySeries[m]);
        SERIALCONSOLE.print(modulePresent(m) ? F("S present  ") : F("S ABSENT   "));
        SERIALCONSOLE.print(okCells);
        SERIALCONSOLE.print(F(" cells reporting  V="));
        const float mv = moduleVoltage(m);
        if (mv > 0.0f) { SERIALCONSOLE.print(mv, 2); SERIALCONSOLE.print('V'); }
        else SERIALCONSOLE.print('-');
        SERIALCONSOLE.println(moduleAlarm(m) ? F("  ALARM") : F(""));

        for (int i = 0; i < kFactoryCircuitCount; i++) {
            if (circModule(i) != m) continue;
            const CircState st = circState(i);
            const uint8_t first = circFirstCh(i), n = circCells(i);
            SERIALCONSOLE.print(F("     c"));
            SERIALCONSOLE.print(first);
            SERIALCONSOLE.print('-');
            SERIALCONSOLE.print(first + n - 1);
            SERIALCONSOLE.print(F(" ("));
            SERIALCONSOLE.print(n);
            SERIALCONSOLE.print(F("S) "));
            SERIALCONSOLE.print(circFrame(i, 0).id, HEX);
            SERIALCONSOLE.print('/');
            SERIALCONSOLE.print(circFrame(i, 1).id, HEX);
            SERIALCONSOLE.print(F(" n="));
            SERIALCONSOLE.print(g_frameCount[2 * i]);
            SERIALCONSOLE.print('/');
            SERIALCONSOLE.print(g_frameCount[2 * i + 1]);
            SERIALCONSOLE.print(F("  "));
            SERIALCONSOLE.print(circName(st));
            if (st != CircState::Empty) {
                SERIALCONSOLE.print(':');
                float sum = 0.0f; bool all = true;
                for (uint8_t ch = first; ch < first + n; ch++) {
                    SERIALCONSOLE.print(' ');
                    const ChState cs = channelState(m, ch);
                    if (cs == ChState::NotWired) { SERIALCONSOLE.print('-'); all = false; continue; }
                    if (!g_ch[m][ch].seen)       { SERIALCONSOLE.print(F("?!")); all = false; continue; }
                    SERIALCONSOLE.print(g_ch[m][ch].v, 3);
                    if (cs == ChState::Alarm) { SERIALCONSOLE.print('!'); all = false; }
                    else sum += g_ch[m][ch].v;
                }
                if (all) { SERIALCONSOLE.print(F("  = ")); SERIALCONSOLE.print(sum, 2); SERIALCONSOLE.print('V'); }
            }
            SERIALCONSOLE.println();
        }
    }
#if FACTORY_SERIES_STRING
    SERIALCONSOLE.print(F("  series string total: "));
    const float pv = packVoltage();
    if (pv > 0.0f) { SERIALCONSOLE.print(pv, 2); SERIALCONSOLE.println(F("V")); }
    else SERIALCONSOLE.println(F("- (not all cells OK)"));
#else
    SERIALCONSOLE.println(F("  (boards are separate strings: no pack total; charge refused)"));
#endif
    printLowLiveWarnings();
    printTemps();
    SERIALCONSOLE.println(F("  legend: silent(empty) = never sent, no alarm   SILENT-LOST/PARTIAL/! = alarm (stale >3 s or <0.5/>4.3 V)"));
}

void handleSerial(char c) {
    if (c == 'd') printDecodeDetails();
}

}  // namespace BicmDecode

#endif  // FACTORY_BUS_DECODE
