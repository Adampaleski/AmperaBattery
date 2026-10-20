// PC run of the BMS core fault scenarios (same code as the Teensy + browser).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/sim_bms.cpp -o /tmp/sim && /tmp/sim
#include <stdio.h>
#include <string.h>
#include "BmsSim.h"

int main() {
    static BmsSim::Result r[BmsSim::kScenarios];
    const int n = BmsSim::runAll(r);
    static char buf[8192];
    const int fails = BmsSim::formatTable(buf, sizeof buf, r, n);
    fputs(buf, stdout);
    // current sensor round-trip (DHAB S/44 model)
    BmsCore::CurrentCal cal;
    const float amps[] = {0, 1.5f, -3, 15, 25, -60, 200, -400};
    int cfail = 0;
    for (float a : amps) {
        float p1, p2, ps; bool ok;
        BmsCore::dhabPinVolts(cal, a, 5.05f, p1, p2, ps);
        const float got = BmsCore::dhabAmps(cal, p1, p2, ps, &ok);
        const float err = got - a;
        const float tol = (a < 18 && a > -18) ? 0.35f : 1.0f;
        if (!ok || err > tol || err < -tol) { cfail++; printf("FAIL current %.1f A -> %.2f A\n", a, got); }
    }
    printf("current sensor model: %d/8 round-trips within tolerance (ratiometric, ch1<18 A, ch2 above)\n", 8 - cfail);
    printf("charge limit map: 80%% = %.3f V, 90%% = %.3f V, 100%% = %.3f V per cell\n",
           BmsCore::ceilingForPct(80), BmsCore::ceilingForPct(90), BmsCore::ceilingForPct(100));
    // 36S full charge must be the real 4.10 V/cell = 147.6 V (pack clamp 163 V inactive).
    int lfail = 0;
    {
        static BmsSim::World w;
        w.reset();
        w.core.cfg.chargePct = 100;
        BmsSim::bringUp(w, true);
        const auto &o = w.core.out();
        const bool ok = o.ceilingV > 4.0999f && o.ceilingV < 4.1001f && o.chargeV > 147.55f && o.chargeV < 147.65f;
        if (!ok) lfail++;
        printf("%s P100 on %dS: ceiling %.3f V/cell, Elcon request %.1f V (Kelly %.0f V - %.0f V margin = %.0f V clamp, inactive)\n",
               ok ? "PASS" : "FAIL", w.core.cfg.stringCells, o.ceilingV, o.chargeV, w.core.cfg.busMaxV,
               w.core.cfg.busMarginV, w.core.cfg.busMaxV - w.core.cfg.busMarginV);
        // general clamp still works if someone builds a longer string
        w.reset(); w.core.cfg.chargePct = 100; w.core.cfg.stringCells = 42;
        w.step(10);
        const bool clampOk = w.core.out().ceilingV * 42 <= 163.01f;
        if (!clampOk) lfail++;
        printf("%s pack clamp still active for a hypothetical 42S: %.3f V/cell = %.1f V\n", clampOk ? "PASS" : "FAIL",
               w.core.out().ceilingV, w.core.out().ceilingV * 42);
    }
    // Kelly cutback: each limiter alone, at its 50% point, must win and name itself.
    int kfail = 0;
    {
        static BmsSim::World w;
        struct K { const char *name; int what; float val; float wantPct; const char *why; bool regen; };
        const K ks[] = {
            {"full power, healthy",          0, 0,      100, "",               true},
            {"low cell 3.375 V (halfway)",   1, 3.375f,  50, "low cell",       true},
            {"pack hot 52.5 C (halfway)",    2, 52.5f,   50, "pack hot",       true},
            {"500 A draw (250 A target)",    3, 500.f,   50, "discharge amps", true},
            {"cells full 4.09 V: no regen",  4, 4.09f,  100, "",               false},
            {"pack cold -2 C: no regen",     5, -2.f,   100, "",               false},
        };
        for (const K &k : ks) {
            w.reset(); BmsSim::bringUp(w, false);
            if (k.what == 1) w.cell[0][5] = k.val;
            if (k.what == 2) w.tMax = k.val;
            if (k.what == 3) w.packA = k.val;
            if (k.what == 4) w.cell[1][3] = k.val;
            if (k.what == 5) { w.tMin = k.val; }
            w.step(10); w.step(10);
            const auto &o = w.core.out();
            const bool ok = o.state == BmsCore::CState::Closed && o.kellyPct > k.wantPct - 1.5f && o.kellyPct < k.wantPct + 1.5f &&
                            strcmp(o.kellyWhy, k.why) == 0 && o.kellyRegenOk == k.regen;
            if (!ok) kfail++;
            printf("%s kelly %-30s %5.1f%% why='%s' regen=%d (%s)\n", ok ? "PASS" : "FAIL", k.name, o.kellyPct, o.kellyWhy,
                   o.kellyRegenOk, o.regenWhy);
        }
    }
    return fails + cfail + lfail + kfail;
}
