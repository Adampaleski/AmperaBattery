// ONE-CELL bench bleed (BENCH_BLEED_TEST=1 only). See docs/BENCH_BLEED_TEST.md.
#include "BenchBleedRun.h"
#if BENCH_BLEED_TEST && FACTORY_BUS_DECODE
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "BenchBleed.h"
#include "BicmDecode.h"
#include "CanBus.h"
#include "Config.h"
#include "FactoryCellMap.h"
#include "Settings.h"

namespace BenchBleedRun {
namespace {
using namespace FactoryBalance;
bool     g_active = false;
int      g_mod = 0, g_cell = 0;
uint8_t  g_b300[kLen300], g_b310[kLen310];
uint32_t g_startMs = 0, g_lastTxMs = 0, g_lastPrintMs = 0, g_cycle = 0;
bool     g_kaWas = true;

void hex(const uint8_t *b, int n) {
    for (int i = 0; i < n; i++) {
        if (b[i] < 0x10) SERIALCONSOLE.print('0');
        SERIALCONSOLE.print(b[i], HEX);
        if (i + 1 < n) SERIALCONSOLE.print('.');
    }
}

bool send(uint32_t id, const uint8_t *b, uint8_t len) {
    CAN_message_t m{};
    m.id = id; m.len = len; memcpy(m.buf, b, len);
    const int tx = CanBus::writeFrame(m);
    SERIALCONSOLE.print(F("TX 0x")); SERIALCONSOLE.print(id, HEX); SERIALCONSOLE.print('#');
    hex(b, len); SERIALCONSOLE.println(tx > 0 ? F("") : F("  (TX FAILED)"));
    return tx > 0;
}

bool sendTriplet(const uint8_t *b300, const uint8_t *b310) {
    bool ok = send(kId300, b300, kLen300);
    ok = send(kId310, b310, kLen310) && ok;
    ok = send(kIdTrigger, kTrigger, 3) && ok;
    return ok;
}

void sendAllOff() {
    uint8_t z300[kLen300], z310[kLen310];
    clearFrames(z300, z310);
    SERIALCONSOLE.println(F("BLEED ALL-OFF:"));
    for (int k = 0; k < 3; k++) sendTriplet(z300, z310);  // sent 3x to be sure
}

BenchBleed::GuardIn gather(int mod, int cell) {
    BenchBleed::GuardIn g;
    g.module = mod; g.cell = cell; g.active = g_active;
    g.lowCutoffV = Settings::lowCutoffV();
    if (mod >= 1 && mod <= kFactoryModules) g.moduleChannels = kFactoryChannels[mod];
    bool fresh = true; int seen = 0; float minV = 9.0f;
    for (int m = 1; m <= kFactoryModules; m++) {
        for (int ch = 1; ch <= kFactoryChannels[m]; ch++) {
            const BicmDecode::ChState st = BicmDecode::channelState(m, ch);
            if (st == BicmDecode::ChState::NotWired || st == BicmDecode::ChState::NoData) continue;
            seen++;
            if (st != BicmDecode::ChState::Ok) { fresh = false; continue; }
            const float v = BicmDecode::channelVoltage(m, ch);
            if (v < minV) minV = v;
        }
    }
    g.allFresh = fresh && seen > 0;
    g.minCellV = seen ? minV : 0.0f;
    if (g.moduleChannels && cell >= 1 && cell <= g.moduleChannels) {
        g.targetOk = BicmDecode::channelState(mod, cell) == BicmDecode::ChState::Ok;
        g.targetV  = BicmDecode::channelVoltage(mod, cell);
    }
    return g;
}

void stop(const char *why) {
    if (!g_active) return;
    g_active = false;
    sendAllOff();
    CanBus::setKeepAliveEnabled(g_kaWas);
    SERIALCONSOLE.print(F("*** BLEED STOPPED: ")); SERIALCONSOLE.print(why);
    SERIALCONSOLE.print(F("  after ")); SERIALCONSOLE.print(millis() - g_startMs);
    SERIALCONSOLE.println(F(" ms. All bleed bits OFF. Watch the cell recover (d). ***"));
}
}  // namespace

bool active() { return g_active; }
void abort(const char *why) { stop(why); }

void start(int mod, int cell) {
    const BenchBleed::GuardIn g = gather(mod, cell);
    int r = BenchBleed::check(g);
    int circ = -1, bit = 0;
    if (r == BenchBleed::kOk) {
        circ = BenchBleed::findCircuit(mod, cell, &bit);
        r = BenchBleed::buildFrames(circ, bit, g_b300, g_b310);
    }
    if (r != BenchBleed::kOk) {
        SERIALCONSOLE.print(F("BLEED REFUSED M")); SERIALCONSOLE.print(mod);
        SERIALCONSOLE.print(F(" ch")); SERIALCONSOLE.print(cell);
        SERIALCONSOLE.print(F(": ")); SERIALCONSOLE.println(BenchBleed::refuseText(r));
        SERIALCONSOLE.print(F("  target=")); SERIALCONSOLE.print(g.targetV, 3);
        SERIALCONSOLE.print(F(" V  min=")); SERIALCONSOLE.print(g.minCellV, 3);
        SERIALCONSOLE.print(F(" V  lowCut=")); SERIALCONSOLE.print(g.lowCutoffV, 2);
        SERIALCONSOLE.print(F(" V  allFresh=")); SERIALCONSOLE.println(g.allFresh ? 1 : 0);
        return;
    }
    g_mod = mod; g_cell = cell; g_active = true; g_cycle = 0;
    g_startMs = g_lastTxMs = g_lastPrintMs = millis();
    g_kaWas = CanBus::keepAliveEnabled();
    CanBus::setKeepAliveEnabled(false);  // our 0x200 every 200 ms replaces the 1 s one
    SERIALCONSOLE.print(F("*** BLEED ARMED M")); SERIALCONSOLE.print(mod);
    SERIALCONSOLE.print(F(" ch")); SERIALCONSOLE.print(cell);
    SERIALCONSOLE.print(F(" (circuit 0x")); SERIALCONSOLE.print(kCircuit[circ].idA, HEX);
    SERIALCONSOLE.print(F(" bit ")); SERIALCONSOLE.print(bit);
    SERIALCONSOLE.print(F(")  start=")); SERIALCONSOLE.print(g.targetV, 3);
    SERIALCONSOLE.println(F(" V  10 s, press x to abort ***"));
    sendTriplet(g_b300, g_b310);
}

void tick() {
    if (!g_active) return;
    const uint32_t now = millis();
    if (now - g_startMs >= BenchBleed::kDurationMs) { stop("10 s timeout"); return; }
    if (now - g_lastTxMs >= BenchBleed::kResendMs) {
        g_lastTxMs = now;
        g_cycle++;
        if (g_cycle % kSampleEvery == 0) {  // blank cycle -> clean (unloaded) reading
            uint8_t z300[kLen300], z310[kLen310];
            clearFrames(z300, z310);
            SERIALCONSOLE.println(F("(sample cycle: bits off)"));
            if (!sendTriplet(z300, z310)) { stop("CAN TX failed"); return; }
        } else if (!sendTriplet(g_b300, g_b310)) { stop("CAN TX failed"); return; }
    }
    if (now - g_lastPrintMs >= BenchBleed::kPrintMs) {
        g_lastPrintMs = now;
        const BenchBleed::GuardIn g = gather(g_mod, g_cell);
        SERIALCONSOLE.print(F("BLEED t=")); SERIALCONSOLE.print(now - g_startMs);
        SERIALCONSOLE.print(F(" ms  M")); SERIALCONSOLE.print(g_mod);
        SERIALCONSOLE.print(F(" ch")); SERIALCONSOLE.print(g_cell);
        SERIALCONSOLE.print(F(" = ")); SERIALCONSOLE.print(g.targetV, 3);
        SERIALCONSOLE.print(F(" V  (min ")); SERIALCONSOLE.print(g.minCellV, 3);
        SERIALCONSOLE.println(F(" V)"));
        // Stop if data goes stale or any cell falls below the low cutoff.
        if (!g.allFresh || !g.targetOk) { stop("cell data went stale"); return; }
        if (g.minCellV < g.lowCutoffV)  { stop("cell below low cutoff"); return; }
    }
}

bool handleLine(const char *line) {
    if (strncmp(line, "bleed", 5) != 0) return false;
    int m = 0, c = 0;
    if (sscanf(line + 5, "%d %d", &m, &c) != 2) {
        SERIALCONSOLE.println(F("Usage: bleed <module 1-4> <channel>   e.g. bleed 2 5   (x = abort)"));
        return true;
    }
    start(m, c);
    return true;
}

}  // namespace BenchBleedRun
#else
namespace BenchBleedRun {
bool active() { return false; }
void start(int, int) {}
void abort(const char *) {}
void tick() {}
bool handleLine(const char *) { return false; }
}
#endif
