#pragma once
// Print-only bleed planner for factory-strapped BICMs on ONE CAN bus.
// PURE C++ (no Arduino): firmware ('b' command) and the PC test
// scripts/test_factory_balance.cpp run exactly this code.
//
// Rules (crew/Adam 10/19):
//  * plan only if every included circuit is fresh, every wired cell is
//    0.5-4.3 V and no cell is below the low cutoff (L setting)
//  * candidate cell: v >= minBleedV AND v > groupMin + deadband (W setting)
//  * cycles repeat every kSampleEvery: odd board channels, even channels, odd,
//    even, then one SAMPLE cycle with blank 0x300/0x310 (clean voltage read).
//    Odd/even sets never put adjacent cells of one circuit in the same frame
//    and can never mark every cell (swoozle #10).
//  * frame set per cycle = 0x300, 0x310, then 0x200 02 00 00, every kCycleMs.
// Nothing here transmits.

#include <stdint.h>
#include <stdio.h>
#include "FactoryBalanceMap.h"

namespace FactoryBalance {

constexpr const char *kBoardName[5] = {"?", "M1 30S", "M2 24S", "M3 24S", "M4 18S"};

struct PlanInput {
    float   v[kCircuits][8] = {};      // [circuit][bit] volts
    uint8_t use[kCircuits]   = {};     // 1 = circuit belongs to this plan
    uint8_t fresh[kCircuits] = {};     // 1 = both frames of the circuit are fresh
    uint8_t wired[kCircuits] = {};     // bit mask of wired cells (0 -> all cells)
    uint8_t group[kCircuits] = {};     // min-reference group (0..kCircuits-1)
    float   deadbandV  = 0.010f;
    float   lowCutoffV = 3.30f;
    float   minBleedV  = 3.90f;
};

enum Status : int { kOk = 0, kNoCircuits, kNotFresh, kCellBad, kBelowLowCut };

inline const char *statusText(int s) {
    switch (s) {
        case kOk:          return "ok";
        case kNoCircuits:  return "no circuits on this bus";
        case kNotFresh:    return "a circuit is not reporting (stale/partial)";
        case kCellBad:     return "a cell is outside 0.5-4.3 V";
        case kBelowLowCut: return "a cell is below the low cutoff";
        default:           return "?";
    }
}

struct Plan {
    int     status = kNoCircuits;
    uint8_t cand[kCircuits] = {};      // all candidates (before odd/even split)
    float   groupMin[kCircuits] = {};  // indexed by group id
    int     nCand = 0;
};

inline uint8_t wiredMask(const PlanInput &in, int i) {
    return in.wired[i] ? static_cast<uint8_t>(in.wired[i] & cellMask(i)) : cellMask(i);
}

inline Plan evaluate(const PlanInput &in) {
    Plan p;
    int nUse = 0;
    for (int g = 0; g < kCircuits; g++) p.groupMin[g] = 99.0f;
    for (int i = 0; i < kCircuits; i++) {
        if (!in.use[i]) continue;
        nUse++;
        if (!in.fresh[i]) { p.status = kNotFresh; return p; }
        const uint8_t w = wiredMask(in, i);
        for (int b = 0; b < kCircuit[i].cells; b++) {
            if (!(w & (1u << b))) continue;
            const float v = in.v[i][b];
            if (v < 0.5f || v > 4.3f) { p.status = kCellBad; return p; }
            if (v < in.lowCutoffV)    { p.status = kBelowLowCut; return p; }
            const uint8_t g = in.group[i] < kCircuits ? in.group[i] : 0;
            if (v < p.groupMin[g]) p.groupMin[g] = v;
        }
    }
    if (!nUse) { p.status = kNoCircuits; return p; }
    for (int i = 0; i < kCircuits; i++) {
        if (!in.use[i]) continue;
        const uint8_t w = wiredMask(in, i);
        const float target = p.groupMin[in.group[i] < kCircuits ? in.group[i] : 0] + in.deadbandV;
        for (int b = 0; b < kCircuit[i].cells; b++) {
            if (!(w & (1u << b))) continue;
            const float v = in.v[i][b];
            if (v >= in.minBleedV && v > target) { p.cand[i] |= static_cast<uint8_t>(1u << b); p.nCand++; }
        }
    }
    p.status = kOk;
    return p;
}

// 0 = odd board channels (bits 0,2,4,6), 1 = even (bits 1,3,5,7), 2 = sample.
inline int cycleKind(uint32_t cycle) {
    const uint32_t k = cycle % kSampleEvery;
    if (k == kSampleEvery - 1) return 2;
    return static_cast<int>(k & 1u);
}
inline const char *kindName(int k) { return k == 0 ? "odd ch" : (k == 1 ? "even ch" : "SAMPLE"); }

// Bits for one cycle. Returns number of cells marked.
inline int cycleBits(const Plan &p, uint32_t cycle, uint8_t bits[kCircuits]) {
    const int k = cycleKind(cycle);
    const uint8_t parity = k == 0 ? 0x55 : 0xAA;
    int n = 0;
    for (int i = 0; i < kCircuits; i++) {
        bits[i] = (p.status == kOk && k != 2) ? static_cast<uint8_t>(p.cand[i] & parity & cellMask(i)) : 0;
        n += popcount8(bits[i]);
    }
    return n;
}

// Encode one cycle; on a failed safety check frames are zeroed. Returns cells marked.
inline int cycleFrames(const Plan &p, uint32_t cycle, uint8_t b300[kLen300], uint8_t b310[kLen310],
                       int *unsafe = nullptr) {
    uint8_t bits[kCircuits];
    int n = cycleBits(p, cycle, bits);
    encode(bits, b300, b310);
    const int u = framesUnsafe(b300, b310);
    if (unsafe) *unsafe = u;
    if (u) { clearFrames(b300, b310); n = 0; }
    return n;
}

inline int hexLine(char *s, int cap, const char *tag, uint32_t id, const uint8_t *buf, int len) {
    int o = snprintf(s, cap, "      WOULD TX 0x%03X:", static_cast<unsigned>(id));
    for (int k = 0; k < len && o < cap; k++) o += snprintf(s + o, cap - o, " %02X", buf[k]);
    if (tag && o < cap) o += snprintf(s + o, cap - o, "%s", tag);
    return o;
}

// Print the plan for one bus as lines: header, candidates, then one block per
// distinct cycle kind (odd / even / sample) with the exact would-be frames.
// out(const char *line) is called per line.
template <typename Out>
void printPlan(Out out, const PlanInput &in, const Plan &p, const char *busName) {
    char s[200];
    int o;
    if (p.status != kOk) {
        snprintf(s, sizeof s, "  %s: no plan (%s) -> 0x300/0x310 would be all 00", busName, statusText(p.status));
        out(s);
        return;
    }
    o = snprintf(s, sizeof s, "  %s: deadband=%dmV bleed-floor=%dmV low-cut=%dmV cadence=%ums  ref min:",
                 busName, static_cast<int>(in.deadbandV * 1000 + 0.5f), static_cast<int>(in.minBleedV * 1000 + 0.5f),
                 static_cast<int>(in.lowCutoffV * 1000 + 0.5f), static_cast<unsigned>(kCycleMs));
    bool seen[kCircuits] = {};
    for (int i = 0; i < kCircuits; i++) {
        if (!in.use[i]) continue;
        const uint8_t g = in.group[i] < kCircuits ? in.group[i] : 0;
        if (seen[g]) continue;
        seen[g] = true;
        o += snprintf(s + o, sizeof s - o, " g%d=%dmV", g, static_cast<int>(p.groupMin[g] * 1000 + 0.5f));
    }
    out(s);
    // candidates per board
    for (int m = 1; m <= 4; m++) {
        bool any = false;
        o = 0;
        for (int i = 0; i < kCircuits; i++) {
            if (!in.use[i] || kCircuit[i].module != m) continue;
            if (!any) { o = snprintf(s, sizeof s, "    %s over target:", kBoardName[m]); any = true; }
            const float mn = p.groupMin[in.group[i] < kCircuits ? in.group[i] : 0];
            for (int b = 0; b < kCircuit[i].cells; b++) {
                if (!(p.cand[i] & (1u << b)) || o >= static_cast<int>(sizeof s) - 16) continue;
                o += snprintf(s + o, sizeof s - o, " ch%d(+%d)", kCircuit[i].firstCh + b,
                              static_cast<int>((in.v[i][b] - mn) * 1000 + 0.5f));
            }
        }
        if (any) {
            bool none = true;
            for (int i = 0; i < kCircuits; i++) if (in.use[i] && kCircuit[i].module == m && p.cand[i]) none = false;
            if (none && o < static_cast<int>(sizeof s)) snprintf(s + o, sizeof s - o, " none");
            out(s);
        }
    }
    static const uint32_t kShow[3] = {0, 1, kSampleEvery - 1};
    for (int c = 0; c < 3; c++) {
        const uint32_t cyc = kShow[c];
        uint8_t bits[kCircuits], b300[kLen300], b310[kLen310];
        int unsafe = 0;
        cycleBits(p, cyc, bits);
        const int n = cycleFrames(p, cyc, b300, b310, &unsafe);
        if (cyc == kSampleEvery - 1)
            o = snprintf(s, sizeof s, "    cycle %u (%s, every %u): bleed nothing, clean read", static_cast<unsigned>(cyc),
                         kindName(cycleKind(cyc)), static_cast<unsigned>(kSampleEvery));
        else
            o = snprintf(s, sizeof s, "    cycle %u%s (%s): %d cells", static_cast<unsigned>(cyc),
                         cyc == 0 ? ",2" : ",3", kindName(cycleKind(cyc)), n);
        if (unsafe) o += snprintf(s + o, sizeof s - o, "  SAFETY BLOCK code %d -> frames zeroed", unsafe);
        out(s);
        if (!unsafe && cyc != kSampleEvery - 1) {
            for (int m = 1; m <= 4; m++) {
                bool any = false;
                o = 0;
                for (int i = 0; i < kCircuits; i++) {
                    if (!in.use[i] || kCircuit[i].module != m) continue;
                    if (!any) { o = snprintf(s, sizeof s, "      %s:", kBoardName[m]); any = true; }
                    for (int b = 0; b < kCircuit[i].cells; b++)
                        if ((bits[i] & (1u << b)) && o < static_cast<int>(sizeof s) - 8)
                            o += snprintf(s + o, sizeof s - o, " ch%d", kCircuit[i].firstCh + b);
                }
                if (any) {
                    bool none = true;
                    for (int i = 0; i < kCircuits; i++) if (in.use[i] && kCircuit[i].module == m && bits[i]) none = false;
                    if (none && o < static_cast<int>(sizeof s)) snprintf(s + o, sizeof s - o, " -");
                    out(s);
                }
            }
        }
        hexLine(s, sizeof s, nullptr, kId300, b300, kLen300); out(s);
        hexLine(s, sizeof s, nullptr, kId310, b310, kLen310); out(s);
        hexLine(s, sizeof s, nullptr, kIdTrigger, kTrigger, 3); out(s);
    }
}

}  // namespace FactoryBalance
