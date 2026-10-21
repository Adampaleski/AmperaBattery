// Low cap vs soft derate (crew 2026-10-21). /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/test_limp.cpp -o /tmp/tl && /tmp/tl
#include <stdio.h>
#include "BmsSim.h"
static int n = 0, f = 0;
static void ok(bool c, const char *m, float v = 0) { n++; if (!c) f++; printf("%s %s (%.1f)\n", c ? "PASS" : "FAIL", m, v); }
int main() {
    static BmsSim::World w;
    auto up = [&]() { w.reset(); BmsSim::bringUp(w, false); };
    auto run = [&](uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) w.step(10); };
    // soft: pack hot -> scaled, no cap
    up(); w.tMax = 52.5f; run(500);
    ok(w.core.out().kellyPct < 60 && !w.core.out().kellyLimp, "soft: pack hot scales current, no Low cap", w.core.out().kellyPct);
    up(); for (int b = 0; b < BmsSim::kBoards; b++) for (int c = 0; c < 24; c++) w.cell[b][c] = 3.375f; run(500);
    ok(w.core.out().kellyPct < 60 && !w.core.out().kellyLimp, "soft: low-ish cells 3.375 V scale, no Low cap", w.core.out().kellyPct);
    // hard: lost current sensor, ramp ~1 s
    up(); w.currentFitted = false; run(100);
    const float mid = w.core.out().kellyPct; const bool capEarly = w.core.out().kellyLimp;
    run(500); const float mid2 = w.core.out().kellyPct;
    ok(!capEarly && mid > mid2 && mid2 > w.core.cfg.limpRampPct, "hard: LEM lost ramps down (not instant)", mid2);
    run(600);
    ok(w.core.out().kellyLimp && w.core.out().kellyPct <= w.core.cfg.limpRampPct + 0.1f, "hard: LEM lost -> Low cap after ~1 s", w.core.out().kellyPct);
    w.currentFitted = true; run(100);
    ok(!w.core.out().kellyLimp, "LEM back -> cap released", w.core.out().kellyPct);
    up(); w.tempsValid = false; run(1200);
    ok(w.core.out().kellyLimp, "hard: stale temps -> Low cap");
    up(); w.packA = 200; w.cell[2][5] = 2.95f; run(200);
    // NOTE: the 3.00 V hard floor (500 ms) opens the mains before the 1 s ramp ends, so only the ramp start is seen.
    ok(w.core.out().limpWhy[0] && w.core.out().state == BmsCore::CState::Closed, "hard: cell 2.95 V under load -> Low cap ramp starts (floor trip follows at 500 ms)", w.core.out().kellyPct);
    up(); w.lemShareA = 0.9f; w.packA = 300; run(12000);
    ok(w.core.out().kellyLimp, "hard: LEM A/B mismatch -> Low cap", w.core.out().lemDiffA);
    printf("limp: %d/%d\n", n - f, n); return f ? 1 : 0;
}
