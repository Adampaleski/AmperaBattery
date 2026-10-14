// Factory-bus decoder: up to four Volt BICMs on ONE CAN3 bus, distinguished by
// their factory ID straps. Layout + byte decode from tomdebree/AmperaBattery
// VoltBMS/VoltBMS.ino candecode(); channel→pack-cell map in FactoryCellMap.h.
// Monitor only: no TX here (keep-alive 0x200 lives in CanBus.cpp).

#include "BicmDecode.h"
#include "Config.h"

#if FACTORY_BUS_DECODE

#include "FactoryCellMap.h"

namespace BicmDecode {

namespace {

constexpr float    kAlarmLowV  = 0.5f;
constexpr float    kAlarmHighV = 4.3f;
constexpr uint32_t kStaleMs    = 3000;

struct Channel {
    float    v       = 0.0f;
    uint32_t lastMs  = 0;
    bool     seen    = false;
};

Channel  g_ch[kFactoryModules + 1][kFactoryMaxCh + 1];
uint32_t g_modLastMs[kFactoryModules + 1] = {};
bool     g_modSeen[kFactoryModules + 1]   = {};
uint16_t g_frameCount[kFactoryFrameCount] = {};

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

void updateStability() {
    bool ok = !anyAlarm();
    for (int c = 1; c <= FACTORY_PACK_CELLS && ok; c++) {
        if (channelState(g_cellMod[c], g_cellCh[c]) != ChState::Ok) ok = false;
    }
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
    for (int i = 0; i < kFactoryFrameCount; i++) {
        const FactoryFrameDef &f = kFactoryFrames[i];
        if (msg.id != f.id) continue;
        if (g_frameCount[i] < 0xFFFF) g_frameCount[i]++;
        const uint32_t now = millis();
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
    if (!fresh(c.lastMs, c.seen)) return ChState::Alarm;  // wired but stale/never
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

float moduleVoltage(uint8_t m) {
    const int w = moduleWired(m);
    if (w == 0 || moduleOk(m) != w) return 0.0f;
    float s = 0.0f;
    for (int ch = 1; ch <= kFactoryChannels[m]; ch++)
        if (mapOf(m, ch)) s += g_ch[m][ch].v;
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
    SERIALCONSOLE.print(F("DECODE FACTORY-BUS  modules_present="));
    SERIALCONSOLE.print(moduleCount());
    SERIALCONSOLE.print(F("/4  cells_ok="));
    SERIALCONSOLE.print(seriesCellCount());
    SERIALCONSOLE.print(F("/"));
    SERIALCONSOLE.print(FACTORY_PACK_CELLS);
    SERIALCONSOLE.print(F("  stable="));
    SERIALCONSOLE.print(g_packStable ? F("Y") : F("N"));
    SERIALCONSOLE.print(F("  ALARM="));
    SERIALCONSOLE.println(anyAlarm() ? F("YES") : F("no"));

    for (int m = 1; m <= kFactoryModules; m++) {
        const int w = moduleWired(m);
        SERIALCONSOLE.print(F("  M"));
        SERIALCONSOLE.print(m);
        SERIALCONSOLE.print(modulePresent(m) ? F(" present") : F(" ABSENT "));
        SERIALCONSOLE.print(F("  wired="));
        SERIALCONSOLE.print(w);
        SERIALCONSOLE.print(F("/"));
        SERIALCONSOLE.print(kFactoryChannels[m]);
        SERIALCONSOLE.print(F("  ok="));
        SERIALCONSOLE.print(moduleOk(m));
        SERIALCONSOLE.print(F("  V="));
        const float mv = moduleVoltage(m);
        if (w && mv > 0.0f) SERIALCONSOLE.print(mv, 2); else SERIALCONSOLE.print('-');
        SERIALCONSOLE.println(moduleAlarm(m) ? F("  ALARM") : F(""));
        SERIALCONSOLE.print(F("     frames:"));
        for (int i = 0; i < kFactoryFrameCount; i++) {
            if (kFactoryFrames[i].module != m) continue;
            SERIALCONSOLE.print(F(" "));
            SERIALCONSOLE.print(kFactoryFrames[i].id, HEX);
            SERIALCONSOLE.print('=');
            SERIALCONSOLE.print(g_frameCount[i]);
        }
        SERIALCONSOLE.println();
        if (!modulePresent(m) && w == 0) continue;
        SERIALCONSOLE.print(F("     ch:"));
        for (int ch = 1; ch <= kFactoryChannels[m]; ch++) {
            SERIALCONSOLE.print(' ');
            const ChState st = channelState(m, ch);
            if (st == ChState::NotWired) {
                SERIALCONSOLE.print('-');
            } else if (!g_ch[m][ch].seen) {
                SERIALCONSOLE.print(F("?!"));
            } else {
                SERIALCONSOLE.print(g_ch[m][ch].v, 3);
                if (st == ChState::Alarm) SERIALCONSOLE.print('!');
            }
            if (ch % 10 == 0 && ch < kFactoryChannels[m]) SERIALCONSOLE.print(F("\n        "));
        }
        SERIALCONSOLE.println();
    }
#if FACTORY_SERIES_STRING
    SERIALCONSOLE.print(F("  series string total: "));
    const float pv = packVoltage();
    if (pv > 0.0f) { SERIALCONSOLE.print(pv, 2); SERIALCONSOLE.println(F("V")); }
    else SERIALCONSOLE.println(F("- (not all cells OK)"));
#else
    SERIALCONSOLE.println(F("  (map is not one series string: no pack total)"));
#endif
    SERIALCONSOLE.println(F("  legend: - not wired   ! alarm (<0.5/>4.3 V or stale >3 s)   ?! wired, never seen"));
}

void handleSerial(char c) {
    if (c == 'd') printDecodeDetails();
}

}  // namespace BicmDecode

#endif  // FACTORY_BUS_DECODE
