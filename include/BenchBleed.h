#pragma once
// ONE-CELL bench bleed test (env teensy41_bench_bleed, BENCH_BLEED_TEST=1).
// PURE C++ (no Arduino) so scripts/test_bench_bleed.cpp can test the guards.
// Frames come from FactoryBalanceMap.h (Tom de Bree VoltBMSV2 layout).
// Hard limits: exactly one bit across 0x300+0x310, 10 s max, 200 ms resend.
#include <stdint.h>
#include "FactoryBalanceMap.h"

namespace BenchBleed {

constexpr float    kMinCellV    = 3.70f;   // target cell must read >= this
constexpr uint32_t kDurationMs  = 10000;   // auto-stop
constexpr uint32_t kResendMs    = FactoryBalance::kCycleMs;  // 200 ms
constexpr uint32_t kPrintMs     = 500;

enum Refuse : int {
    kOk = 0, kBadModule, kBadCell, kNoCircuit, kNotFresh, kTargetLow,
    kBelowLowCut, kAlreadyActive, kNotOneBit, kUnsafeFrame, kTargetInvalid
};

inline const char *refuseText(int r) {
    switch (r) {
        case kOk:            return "OK";
        case kBadModule:     return "module must be 1-4";
        case kBadCell:       return "cell is outside that module's channel count";
        case kNoCircuit:     return "no bleed circuit covers that cell";
        case kNotFresh:      return "not every circuit is fresh (stale/alarm/missing data)";
        case kTargetLow:     return "target cell below 3.70 V";
        case kBelowLowCut:   return "a cell is below the low cutoff";
        case kAlreadyActive: return "a bleed is already running (x to abort)";
        case kNotOneBit:     return "frame would not contain exactly one bleed bit";
        case kUnsafeFrame:   return "frame failed framesUnsafe() check";
        case kTargetInvalid: return "target cell has no valid reading";
    }
    return "?";
}

// module 1..4, ch 1-based board channel -> circuit index + bit, -1 if none.
inline int findCircuit(int module, int ch, int *bit) {
    using namespace FactoryBalance;
    for (int i = 0; i < kCircuits; i++) {
        const CircuitDef &c = kCircuit[i];
        if (c.module == module && ch >= c.firstCh && ch < c.firstCh + c.cells) {
            if (bit) *bit = ch - c.firstCh;
            return i;
        }
    }
    return -1;
}

// Build frames for one cell. Returns kOk only if exactly one bit is set.
inline int buildFrames(int circuit, int bit, uint8_t b300[FactoryBalance::kLen300],
                       uint8_t b310[FactoryBalance::kLen310]) {
    using namespace FactoryBalance;
    uint8_t bits[kCircuits] = {};
    if (circuit < 0 || circuit >= kCircuits || bit < 0 || bit >= kCircuit[circuit].cells) {
        clearFrames(b300, b310);
        return kNoCircuit;
    }
    bits[circuit] = static_cast<uint8_t>(1u << bit);
    encode(bits, b300, b310);
    int n = 0;
    for (int k = 0; k < kLen300; k++) n += popcount8(b300[k]);
    for (int k = 0; k < kLen310; k++) n += popcount8(b310[k]);
    if (n != 1) { clearFrames(b300, b310); return kNotOneBit; }
    if (framesUnsafe(b300, b310)) { clearFrames(b300, b310); return kUnsafeFrame; }
    return kOk;
}

struct GuardIn {
    int   module = 0, cell = 0;     // requested
    int   moduleChannels = 0;       // channels on that module (0 = unknown module)
    bool  targetOk = false;         // target channel has fresh, valid data
    float targetV = 0;
    bool  allFresh = false;         // every wired+seen channel fresh and valid, >=1 seen
    float minCellV = 0;             // lowest fresh cell
    float lowCutoffV = 3.30f;
    bool  active = false;
};

inline int check(const GuardIn &g) {
    if (g.active) return kAlreadyActive;
    if (g.module < 1 || g.module > 4) return kBadModule;
    if (g.cell < 1 || g.cell > g.moduleChannels) return kBadCell;
    int bit = 0;
    if (findCircuit(g.module, g.cell, &bit) < 0) return kNoCircuit;
    if (!g.allFresh) return kNotFresh;
    if (!g.targetOk) return kTargetInvalid;
    if (g.minCellV < g.lowCutoffV) return kBelowLowCut;
    if (g.targetV < kMinCellV) return kTargetLow;
    return kOk;
}

// Checked again every 500 ms while running: any failure -> all-off.
inline int checkRunning(const GuardIn &g) {
    GuardIn h = g; h.active = false;
    const int r = check(h);
    // A cell sagging under bleed below 3.70 is still stopped (conservative).
    return r;
}

}  // namespace BenchBleed
