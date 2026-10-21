#include "FlexCanCompat.h"
#include "StringConfig.h"
#if DUAL_STRING
#include "StringMon.h"
#include "Config.h"
#include "FactoryCellMap.h"
#include "FactoryBalancePlan.h"
#include "Settings.h"
#include "StringConfig.h"

namespace StringMon {
namespace {

constexpr uint32_t kMasks[kStrings] = {STRING1_CIRCUITS, STRING2_CIRCUITS};
constexpr const char *kBusName[kStrings] = {BICM1_NAME, BICM2_NAME};

constexpr int circCells(int i) { return kFactoryFrames[2 * i].count + kFactoryFrames[2 * i + 1].count; }
constexpr int maskCells(uint32_t m) {
    int n = 0;
    for (int i = 0; i < kFactoryCircuitCount; i++) if (m & (1UL << i)) n += circCells(i);
    return n;
}
static_assert(kFactoryCircuitCount == 13, "circuit table changed");
static_assert((STRING1_CIRCUITS >> 13) == 0 && (STRING2_CIRCUITS >> 13) == 0, "circuit index > 12");
static_assert(maskCells(STRING1_CIRCUITS) == STRING_EXPECT_S, "STRING1_CIRCUITS does not total STRING_EXPECT_S cells");
static_assert(maskCells(STRING2_CIRCUITS) == STRING_EXPECT_S, "STRING2_CIRCUITS does not total STRING_EXPECT_S cells");

struct Bus {
    float    v[kFactoryFrameCount][4] = {};
    uint32_t last[kFactoryFrameCount] = {};
    uint32_t count[kFactoryFrameCount] = {};
    uint32_t maxGap[kFactoryFrameCount] = {};
};
Bus      g_bus[kStrings];
Summary  g_sum[kStrings];
uint8_t  g_prevFaults[kStrings] = {};
// Print-only bleed cycle counter: advances every FactoryBalance::kCycleMs
// (200 ms) like the OEM BECM frame set would. Nothing is transmitted.
uint32_t g_balCycle   = 0;
uint32_t g_balCycleMs = 0;

float decodeCell(uint8_t msb, uint8_t lsb) {
    return static_cast<uint16_t>(((msb & 0x0F) << 8) | lsb) * 0.00125f;
}

// 0 never, 1 fresh, 2 stale
int frameState(const Bus &b, int f, uint32_t now) {
    if (!b.count[f]) return 0;
    return (now - b.last[f]) <= STRING_SILENT_MS ? 1 : 2;
}

const char *modName(uint8_t m) {
    switch (m) { case 1: return "M1 30S"; case 2: return "M2 24S"; case 3: return "M3 24S"; default: return "M4 18S"; }
}

// Walk string cells in order: expected circuits ascending, frame A then frame B.
template <typename F>
void forEachCell(int s, F fn) {
    const Bus &b = g_bus[s];
    int idx = 0;
    for (int i = 0; i < kFactoryCircuitCount; i++) {
        if (!(kMasks[s] & (1UL << i))) continue;
        for (int k = 0; k < 2; k++) {
            const int f = 2 * i + k;
            for (int c = 0; c < kFactoryFrames[f].count; c++) fn(idx++, i, f, b.v[f][c]);
        }
    }
}

void evaluate(int s) {
    const Bus &b = g_bus[s];
    const uint32_t now = millis();
    Summary r;
    r.expected = static_cast<uint8_t>(maskCells(kMasks[s]));
    float sum = 0, mn = 99, mx = 0;
    int okCells = 0;
    bool cellBad = false, low = false, over = false;
    for (int i = 0; i < kFactoryCircuitCount; i++) {
        const bool exp = kMasks[s] & (1UL << i);
        const int sa = frameState(b, 2 * i, now), sb = frameState(b, 2 * i + 1, now);
        if (!exp) {
            // Empty-by-design (M4 top 0x46E): silent is normal. Any other extra = info only.
            if (sa == 1 || sb == 1) r.faults |= kFExtra;
            continue;
        }
        if (sa == 0 || sb == 0) r.faults |= kFNoData;
        else if (sa == 2 || sb == 2) r.faults |= kFSilent;
        if (sa != 1 || sb != 1) continue;
        for (int k = 0; k < 2; k++) {
            const int f = 2 * i + k;
            for (int c = 0; c < kFactoryFrames[f].count; c++) {
                const float v = b.v[f][c];
                if (v < 0.5f || v > 4.3f) { cellBad = true; continue; }
                okCells++;
                sum += v;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                if (v < Settings::lowCutoffV()) low = true;
                if (v > kCellFaultMaxV) over = true;
            }
        }
    }
    if (cellBad) r.faults |= kFCellBad;
    if (low) r.faults |= kFLowCut;
    if (over) r.faults |= kFOverMax;
    r.cells = static_cast<uint8_t>(okCells);
    if (okCells != r.expected) r.faults |= kFCount;
    if (okCells) { r.v = sum; r.minV = mn; r.maxV = mx; }
    g_sum[s] = r;
}

}  // namespace

void begin() {
    for (int s = 0; s < kStrings; s++) { g_bus[s] = Bus{}; g_sum[s] = Summary{}; }
}

void onFrame(uint8_t s, const CAN_message_t &msg) {
    if (s >= kStrings || msg.flags.extended) return;
    for (int f = 0; f < kFactoryFrameCount; f++) {
        if (kFactoryFrames[f].id != msg.id) continue;
        Bus &b = g_bus[s];
        const uint32_t now = millis();
        if (b.count[f]) { const uint32_t gap = now - b.last[f]; if (gap > b.maxGap[f]) b.maxGap[f] = gap; }
        for (int c = 0; c < kFactoryFrames[f].count; c++) {
            if (2 * c + 1 >= msg.len) break;
            b.v[f][c] = decodeCell(msg.buf[2 * c], msg.buf[2 * c + 1]);
        }
        b.last[f] = now;
        b.count[f]++;
        return;
    }
}

void tick() {
    if (millis() - g_balCycleMs >= FactoryBalance::kCycleMs) {
        g_balCycleMs = millis();
        g_balCycle++;
    }
    for (int s = 0; s < kStrings; s++) {
        evaluate(s);
        const uint8_t nowF = g_sum[s].faults & kFaultMask;
        if (nowF != g_prevFaults[s]) {
            SERIALCONSOLE.print(F("STRING "));
            SERIALCONSOLE.print(s + 1);
            SERIALCONSOLE.print(nowF ? F(" FAULT ") : F(" fault cleared "));
            printFaults(nowF);
            SERIALCONSOLE.println();
            g_prevFaults[s] = nowF;
        }
    }
}

Summary summary(int s) { return (s >= 0 && s < kStrings) ? g_sum[s] : Summary{}; }

float cellV(int s, int idx) {
    float out = 0;
    if (s < 0 || s >= kStrings) return 0;
    forEachCell(s, [&](int i, int, int, float v) { if (i == idx) out = v; });
    return out;
}

void printFaults(uint8_t b) {
    SERIALCONSOLE.print(F("0x"));
    if (b < 0x10) SERIALCONSOLE.print('0');
    SERIALCONSOLE.print(b, HEX);
    if (b & kFSilent)    SERIALCONSOLE.print(F(" SILENT"));
    if (b & kFNoData)    SERIALCONSOLE.print(F(" NO-DATA"));
    if (b & kFCellBad)   SERIALCONSOLE.print(F(" CELL-BAD"));
    if (b & kFLowCut)    SERIALCONSOLE.print(F(" LOW-CUTOFF"));
    if (b & kFOverMax)   SERIALCONSOLE.print(F(" OVER-MAX"));
    if (b & kFLinkStale) SERIALCONSOLE.print(F(" LINK-STALE"));
    if (b & kFCount)     SERIALCONSOLE.print(F(" COUNT"));
    if (b & kFExtra)     SERIALCONSOLE.print(F(" (info: extra circuit)"));
}

void print() {
    const uint32_t now = millis();
    SERIALCONSOLE.print(F("STRINGS role="));
    SERIALCONSOLE.print(ROLE_IS_A ? 'A' : 'B');
    SERIALCONSOLE.print(F("  "));
    Settings::print();
    for (int s = 0; s < kStrings; s++) {
        const Summary &r = g_sum[s];
        const Bus &b = g_bus[s];
        SERIALCONSOLE.print(F("STRING "));
        SERIALCONSOLE.print(s + 1);
        SERIALCONSOLE.print(F(" ("));
        SERIALCONSOLE.print(kBusName[s]);
        SERIALCONSOLE.print(F(") "));
        SERIALCONSOLE.print(r.expected);
        SERIALCONSOLE.print(F("S cells="));
        SERIALCONSOLE.print(r.cells);
        SERIALCONSOLE.print('/');
        SERIALCONSOLE.print(r.expected);
        SERIALCONSOLE.print(F(" V="));
        if (r.cells) SERIALCONSOLE.print(r.v, 2); else SERIALCONSOLE.print('-');
        SERIALCONSOLE.print(F(" min="));
        if (r.cells) SERIALCONSOLE.print(r.minV, 3); else SERIALCONSOLE.print('-');
        SERIALCONSOLE.print(F(" max="));
        if (r.cells) SERIALCONSOLE.print(r.maxV, 3); else SERIALCONSOLE.print('-');
        SERIALCONSOLE.print(F(" spread="));
        if (r.cells) { SERIALCONSOLE.print(static_cast<int>((r.maxV - r.minV) * 1000 + 0.5f)); SERIALCONSOLE.print(F("mV")); }
        else SERIALCONSOLE.print('-');
        SERIALCONSOLE.print((r.faults & kFaultMask) ? F("  FAULT ") : F("  OK "));
        printFaults(r.faults);
        SERIALCONSOLE.println();
        for (int i = 0; i < kFactoryCircuitCount; i++) {
            const bool exp = kMasks[s] & (1UL << i);
            const bool empty = !exp && (STRING_EMPTY_CIRCUITS & (1UL << i));
            const int fa = 2 * i, fb = 2 * i + 1;
            if (!exp && !empty && !b.count[fa] && !b.count[fb]) continue;
            const int sa = frameState(b, fa, now), sb = frameState(b, fb, now);
            SERIALCONSOLE.print(exp ? F("  exp ") : (empty ? F("  empty ") : F("  xtra ")));
            SERIALCONSOLE.print(modName(kFactoryFrames[fa].module));
            SERIALCONSOLE.print(F(" c"));
            SERIALCONSOLE.print(kFactoryFrames[fa].firstCh);
            SERIALCONSOLE.print('-');
            SERIALCONSOLE.print(kFactoryFrames[fa].firstCh + circCells(i) - 1);
            SERIALCONSOLE.print(' ');
            SERIALCONSOLE.print(kFactoryFrames[fa].id, HEX);
            SERIALCONSOLE.print('/');
            SERIALCONSOLE.print(kFactoryFrames[fb].id, HEX);
            SERIALCONSOLE.print(F(" age="));
            if (b.count[fa]) SERIALCONSOLE.print(now - b.last[fa]); else SERIALCONSOLE.print('-');
            SERIALCONSOLE.print(F("ms gapmax="));
            SERIALCONSOLE.print(b.maxGap[fa] > b.maxGap[fb] ? b.maxGap[fa] : b.maxGap[fb]);
            SERIALCONSOLE.print(F("ms "));
            if (empty && sa != 1 && sb != 1) SERIALCONSOLE.print(F("empty by design (not used)"));
            else if (empty) SERIALCONSOLE.print(F("REPORTING but empty by design - check M4 wiring (info, not a fault)"));
            else if (sa == 1 && sb == 1) SERIALCONSOLE.print(F("reporting"));
            else if (sa == 0 && sb == 0) SERIALCONSOLE.print(exp ? F("NO-DATA") : F("silent"));
            else SERIALCONSOLE.print(exp ? F("SILENT") : F("silent"));
            SERIALCONSOLE.println();
        }
        SERIALCONSOLE.print(F("  cells:"));
        forEachCell(s, [&](int idx, int, int f, float v) {
            SERIALCONSOLE.print(' ');
            if (frameState(b, f, now) == 1) SERIALCONSOLE.print(v, 3); else SERIALCONSOLE.print('-');
            (void)idx;
        });
        SERIALCONSOLE.println();
    }
}

// Build the per-bus planner input for string s (its expected circuits only;
// extra circuits on the bus are never bled).
static FactoryBalance::PlanInput balanceInput(int s) {
    FactoryBalance::PlanInput in;
    const Bus &b = g_bus[s];
    const uint32_t now = millis();
    for (int i = 0; i < kFactoryCircuitCount; i++) {
        if (!(kMasks[s] & (1UL << i))) continue;
        in.use[i]   = 1;
        in.group[i] = 0;  // one series string per bus
        in.fresh[i] = (frameState(b, 2 * i, now) == 1 && frameState(b, 2 * i + 1, now) == 1) ? 1 : 0;
        const int na = kFactoryFrames[2 * i].count;
        for (int c = 0; c < na; c++) in.v[i][c] = b.v[2 * i][c];
        for (int c = 0; c < kFactoryFrames[2 * i + 1].count; c++) in.v[i][na + c] = b.v[2 * i + 1][c];
    }
    in.deadbandV  = Settings::deadbandV();
    in.lowCutoffV = Settings::lowCutoffV();
    in.minBleedV  = kBalanceMinVoltage;
    return in;
}
static_assert(kFactoryCircuitCount == FactoryBalance::kCircuits, "circuit tables differ");

static void printLine(const char *s) { SERIALCONSOLE.println(s); }

void printBalancePlan() {
    SERIALCONSOLE.print(F("BALANCE PLAN (PRINT-ONLY, nothing transmitted)  live cycle="));
    SERIALCONSOLE.print(g_balCycle);
    SERIALCONSOLE.print(F(" ("));
    SERIALCONSOLE.print(FactoryBalance::kindName(FactoryBalance::cycleKind(g_balCycle)));
    SERIALCONSOLE.println(F(")  map=Tom VoltBMSV2 factory 0x300/0x310 (FactoryBalanceMap.h)"));
    for (int s = 0; s < kStrings; s++) {
        const Summary &r = g_sum[s];
        char name[40];
        snprintf(name, sizeof name, "STRING %d (%s) %dS", s + 1, kBusName[s], r.expected);
        if (r.cells != r.expected || (r.faults & kFaultMask)) {
            SERIALCONSOLE.print(F("  "));
            SERIALCONSOLE.print(name);
            SERIALCONSOLE.print(F(": no plan (string not complete/faulted) "));
            printFaults(r.faults);
            SERIALCONSOLE.println(F(" -> 0x300/0x310 would be all 00"));
            continue;
        }
        const FactoryBalance::PlanInput in = balanceInput(s);
        const FactoryBalance::Plan p = FactoryBalance::evaluate(in);
        FactoryBalance::printPlan(printLine, in, p, name);
    }
    SERIALCONSOLE.println(F("  (+mV = above string min. TX OFF: BMS_CAP_BALANCE_TX=0, FACTORY_BALANCE_MAP_VERIFIED=0)"));
}
}  // namespace StringMon

#endif  // DUAL_STRING
