// PC test for the one-cell bench bleed guards (include/BenchBleed.h).
//   g++ -std=c++17 -Wall -Wextra -Iinclude scripts/test_bench_bleed.cpp -o /tmp/tbb && /tmp/tbb
#include "BenchBleed.h"
#include <stdio.h>
using namespace BenchBleed;
using namespace FactoryBalance;
static int g_fail = 0, g_ok = 0;
static void expect(bool c, const char *w) { if (c) g_ok++; else { g_fail++; printf("FAIL %s\n", w); } }
static GuardIn good() {
    GuardIn g; g.module = 2; g.cell = 5; g.moduleChannels = 24; g.targetOk = true;
    g.targetV = 3.85f; g.allFresh = true; g.minCellV = 3.60f; g.lowCutoffV = 3.30f; return g;
}
static int bitsSet(const uint8_t *a, const uint8_t *b) {
    int n = 0; for (int k = 0; k < kLen300; k++) n += popcount8(a[k]);
    for (int k = 0; k < kLen310; k++) n += popcount8(b[k]); return n;
}
int main() {
    expect(check(good()) == kOk, "good request accepted");
    { GuardIn g = good(); g.targetV = 3.699f; expect(check(g) == kTargetLow, "3.699 V refused"); }
    { GuardIn g = good(); g.targetV = 3.70f;  expect(check(g) == kOk, "3.70 V accepted"); }
    { GuardIn g = good(); g.allFresh = false; expect(check(g) == kNotFresh, "stale circuit refused"); }
    { GuardIn g = good(); g.targetOk = false; expect(check(g) == kTargetInvalid, "target no data refused"); }
    { GuardIn g = good(); g.minCellV = 3.29f; expect(check(g) == kBelowLowCut, "cell below low cutoff refused"); }
    { GuardIn g = good(); g.active = true; expect(check(g) == kAlreadyActive, "second bleed refused"); }
    { GuardIn g = good(); g.module = 0; expect(check(g) == kBadModule, "module 0 refused"); }
    { GuardIn g = good(); g.module = 5; expect(check(g) == kBadModule, "module 5 refused"); }
    { GuardIn g = good(); g.cell = 25; expect(check(g) == kBadCell, "cell 25 on 24-ch refused"); }
    { GuardIn g = good(); g.cell = 0;  expect(check(g) == kBadCell, "cell 0 refused"); }
    { GuardIn g = good(); g.allFresh = false; g.targetV = 4.0f; expect(checkRunning(g) != kOk, "running: stale stops"); }
    { GuardIn g = good(); g.active = true; expect(checkRunning(g) == kOk, "running ignores own active flag"); }
    // Every channel of every module -> exactly one bit, in the right byte.
    const int chans[5] = {0, 30, 24, 24, 18};
    int total = 0;
    for (int m = 1; m <= 4; m++) for (int ch = 1; ch <= chans[m]; ch++) {
        int bit = -1; const int i = findCircuit(m, ch, &bit);
        uint8_t a[kLen300], b[kLen310];
        const int r = buildFrames(i, bit, a, b);
        const uint8_t v = kCircuit[i].frame == 0 ? a[kCircuit[i].byte] : b[kCircuit[i].byte];
        if (i >= 0 && r == kOk && bitsSet(a, b) == 1 && v == (1u << bit)) total++;
        else printf("FAIL M%d ch%d\n", m, ch);
    }
    expect(total == 96, "all 96 channels -> exactly one bit");
    { int bit; expect(findCircuit(1, 1, &bit) == 0 && bit == 0, "M1 ch1 -> 0x460 bit0"); }
    { int bit; expect(findCircuit(3, 9, &bit) == 8 && bit == 0, "M3 ch9 -> 0x469 (0x310[0])"); }
    { int bit; expect(findCircuit(4, 18, &bit) == 12 && bit == 5, "M4 ch18 -> 0x46E bit5"); }
    { uint8_t a[kLen300], b[kLen310]; expect(buildFrames(0, 6, a, b) == kNoCircuit && bitsSet(a, b) == 0, "bit beyond 6-cell circuit refused, frames zero"); }
    { uint8_t a[kLen300], b[kLen310]; expect(buildFrames(-1, 0, a, b) == kNoCircuit, "bad circuit refused"); }
    { uint8_t a[kLen300], b[kLen310]; buildFrames(1, 3, a, b);
      expect(a[1] == 0x08 && bitsSet(a, b) == 1, "M1 ch10 -> 0x300#00.08.00..."); }
    expect(kDurationMs == 10000 && kResendMs == 200 && kPrintMs == 500, "timing 10 s / 200 ms / 500 ms");
    printf("%d/%d bench-bleed checks passed\n", g_ok, g_ok + g_fail);
    return g_fail;
}
