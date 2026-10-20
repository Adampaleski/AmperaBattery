// PC run of the BMS core fault scenarios (same code as the Teensy + browser).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/sim_bms.cpp -o /tmp/sim && /tmp/sim
#include <stdio.h>
#include <string.h>
#include "BmsSim.h"
#include "SocEstimator.h"

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
            {"550 A launch: no BMS cap",     3, 550.f,  100, "",               true},
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
    // Hard trip boundaries (Adam 2026-10-20): 650 A for 100 ms, or 750 A on any sample.
    int tfail = 0;
    {
        static BmsSim::World w;
        struct T { const char *name; float a1; uint32_t ms1; float a2; uint32_t ms2; bool wantTrip; };
        const T ts[] = {
            {"600 A for 3 s: stays closed",         600.f, 3000,   0.f,   0, false},
            {"640 A regen-side (-640) 3 s: closed", -640.f, 3000,  0.f,   0, false},
            {"700 A for 80 ms then 400 A: closed",  700.f,   80, 400.f, 500, false},
            {"700 A for 120 ms: trips",             700.f,  120,   0.f,   0, true},
            {"760 A single 10 ms sample: trips",    760.f,   10,   0.f,   0, true},
            {"745 A for 90 ms: closed (< 750 A)",   745.f,   90,   0.f,   0, false},
        };
        for (const T &t : ts) {
            w.reset(); BmsSim::bringUp(w, false);
            w.packA = t.a1; for (uint32_t k = 0; k < t.ms1; k += 10) w.step(10);
            w.packA = t.a2; for (uint32_t k = 0; k < t.ms2; k += 10) w.step(10);
            w.packA = 0; w.step(10);
            const auto &o = w.core.out();
            const bool tripped = (o.latched & BmsCore::F_OVERCURRENT) && o.state == BmsCore::CState::Fault;
            const bool ok = tripped == t.wantTrip && (t.wantTrip || o.state == BmsCore::CState::Closed);
            if (!ok) tfail++;
            printf("%s trip %-38s state=%s latched=0x%03X\n", ok ? "PASS" : "FAIL", t.name, BmsCore::stateName(o.state),
                   static_cast<unsigned>(o.latched));
        }
        printf("trip: %d/6 checks passed\n", 6 - tfail);
    }
    // State of charge (coulomb count of the split LEM sum). capAh 100 is a
    // TEST value only; the real bank Ah is UNVERIFIED.
    int sfail = 0;
    {
        using namespace BmsCore;
        auto soc = [](float capAh) { Soc s; SocConfig c; c.capAh = capAh; s.reset(c); return s; };
        auto run = [](Soc &s, uint32_t &t, uint32_t ms, float a, bool valid, float minCell = 3.8f) {
            for (uint32_t k = 0; k < ms; k += 100) {
                t += 100; SocIn in; in.nowMs = t; in.currentValid = valid; in.packA = a; in.minCellV = minCell; s.step(in);
            }
        };
        auto anchor = [](Soc &s, uint32_t &t, int pct) { t += 100; SocIn in; in.nowMs = t; in.atChargeLimit = true; in.chargePct = pct; in.currentValid = true; in.minCellV = 4.1f; s.step(in); };
        auto rep = [&](bool ok, const char *name, const Soc &s) {
            if (!ok) sfail++;
            printf("%s soc %-36s known=%d used=%.3f Ah pct=%.2f drift=%d (%s)\n", ok ? "PASS" : "FAIL", name, s.out().known,
                   s.out().usedAh, s.out().pct, s.out().drift, s.out().why);
        };
        // 1 boot: current flowing but no anchor -> unknown
        { Soc s = soc(100); uint32_t t = 0; run(s, t, 5000, 50, true);
          rep(!s.out().known && s.out().pct < 0, "boot, no anchor -> unknown", s); }
        // 2 real Core reaches the P100 limit -> anchor 100 %
        { static BmsSim::World w; w.reset(); w.core.cfg.chargePct = 100;
          const bool up = BmsSim::bringUp(w, true);
          for (int b = 0; b < BmsSim::kBoards; b++) for (int c = 0; c < 24; c++) w.cell[b][c] = 4.101f;
          w.step(10); w.step(10);
          Soc s = soc(100); SocIn in; in.nowMs = w.now; in.chargePct = 100; in.currentValid = true; in.minCellV = 4.1f;
          in.atChargeLimit = strcmp(w.core.out().chargeWhy, "at charge limit") == 0; s.step(in);
          rep(up && s.out().known && s.out().pct > 99.99f, "Core P100 'at charge limit' -> 100%", s); }
        // 3 discharge 100 A x 36 s = 1.000 Ah -> 99.00 %
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 100); run(s, t, 36000, 100, true);
          rep(s.out().usedAh > 0.995f && s.out().usedAh < 1.005f && s.out().pct > 98.99f && s.out().pct < 99.01f,
              "100 A x 36 s -> 1.000 Ah, 99.00%", s); }
        // 4 550 A launch 10 s then -50 A regen 36 s: 1.528 - 0.500 = 1.028 Ah
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 100); run(s, t, 10000, 550, true); run(s, t, 36000, -50, true);
          rep(s.out().usedAh > 1.020f && s.out().usedAh < 1.036f, "550 A 10 s + regen -50 A 36 s", s); }
        // 5 P80 anchor is tagged, not called 100 %
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 80);
          rep(s.out().known && s.out().pct > 79.99f && s.out().pct < 80.01f && strstr(s.out().why, "not true 100") != nullptr,
              "P80 anchor -> 80% tagged", s); }
        // 6 1 s sensor dropout keeps the count
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 100); run(s, t, 1000, 0, false); run(s, t, 1000, 100, true);
          rep(s.out().known, "1 s current dropout -> still known", s); }
        // 7 3 s dropout loses the count
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 100); run(s, t, 3000, 0, false);
          rep(!s.out().known && s.out().pct < 0, "3 s current dropout -> unknown", s); }
        // 8 low cut reached with SoC still ~99 % -> drift flag
        { Soc s = soc(100); uint32_t t = 0; anchor(s, t, 100); run(s, t, 3600, 100, true, 3.31f);
          rep(s.out().drift, "min cell 3.31 V at 99% -> drift", s); }
        // 9 no capacity configured -> counts Ah, percent unknown
        { Soc s = soc(0); uint32_t t = 0; anchor(s, t, 100); run(s, t, 36000, 100, true);
          rep(s.out().known && s.out().pct < 0 && s.out().usedAh > 0.99f, "capAh 0 -> Ah only, pct unknown", s); }
        // 10 learn capacity from a full -> low-cut run (200 A x 900 s = 50 Ah)
        { Soc s = soc(0); uint32_t t = 0; anchor(s, t, 100); run(s, t, 900000, 200, true);
          const bool l = s.learnCapacity(); run(s, t, 100, 0, true);
          rep(l && s.cfg.capAh > 49.9f && s.cfg.capAh < 50.1f && s.out().pct < 0.01f, "learn capacity 50 Ah from full->empty", s); }
        printf("soc: %d/10 checks passed\n", 10 - sfail);
    }
    return fails + cfail + lfail + kfail + tfail + sfail;
}
