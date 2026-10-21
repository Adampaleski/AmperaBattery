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
            {"all cells 3.375 V at rest (half)", 1, 3.375f, 50, "low cell",       true},
            {"pack hot 52.5 C (halfway)",    2, 52.5f,   50, "pack hot",       true},
            {"550 A launch: no BMS cap",     3, 550.f,  100, "",               true},
            {"cells full 4.09 V: no regen",  4, 4.09f,  100, "",               false},
            {"pack cold -2 C: no regen",     5, -2.f,   100, "",               false},
        };
        for (const K &k : ks) {
            w.reset(); BmsSim::bringUp(w, false);
            if (k.what == 1) for (int b = 0; b < BmsSim::kBoards; b++) for (int c = 0; c < 24; c++) w.cell[b][c] = k.val;
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
    // Derates (Adam 2026-10-20): LEM imbalance > 60 A (crew 2026-10-21, was 100) for 1 s; cell sag 3.2 -> 3.0 V.
    int dfail = 0;
    {
        static BmsSim::World w;
        auto rep = [&](bool ok, const char *name) {
            if (!ok) dfail++;
            const auto &o = w.core.out();
            printf("%s derate %-40s %5.1f%% why='%s' imb=%d diff=%.0f A state=%s\n", ok ? "PASS" : "FAIL", name, o.kellyPct,
                   o.kellyWhy, o.lemImbalance, o.lemDiffA, BmsCore::stateName(o.state));
        };
        auto ms = [&](uint32_t t) { for (uint32_t k = 0; k < t; k += 10) w.step(10); };
        const float pct50 = w.core.cfg.imbalancePct;  // PLACEHOLDER level
        // 1 even split at the 550 A launch: no derate
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; ms(2000);
        rep(w.core.out().kellyPct > 99.9f && !w.core.out().lemImbalance, "550 A even 275/275 for 2 s: full power");
        // 2 340/210 A (130 A apart) for 0.9 s: not yet
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; w.lemShareA = 340.f / 550.f; ms(900);
        rep(w.core.out().kellyPct > 99.9f && !w.core.out().lemImbalance, "340/210 A for 0.9 s: not yet");
        // 3 same for 1.1 s: derate + warn, contactors stay closed
        ms(200);
        { const auto &o = w.core.out();
          rep(o.lemImbalance && o.kellyPct > pct50 - 0.1f && o.kellyPct < pct50 + 0.1f && strcmp(o.kellyWhy, "LEM imbalance") == 0 &&
              o.state == BmsCore::CState::Closed, "340/210 A for 1.1 s: derate + warn, closed"); }
        // 4 back to even: clears
        w.lemShareA = 0.5f; ms(20);
        rep(!w.core.out().lemImbalance && w.core.out().kellyPct > 99.9f, "back to 275/275: clears");
        // 5 50 A apart for 5 s: under the 60 A threshold
        w.reset(); BmsSim::bringUp(w, false); w.packA = 300; w.lemShareA = 175.f / 300.f; ms(5000);
        rep(!w.core.out().lemImbalance && w.core.out().kellyPct > 99.9f, "175/125 A (50 A) for 5 s: no derate");
        // 5b 90 A apart: now over 60 A -> soft 50 % derate, never the Low cap
        w.reset(); BmsSim::bringUp(w, false); w.packA = 300; w.lemShareA = 195.f / 300.f; ms(2000);
        rep(w.core.out().lemImbalance && w.core.out().kellyPct < pct50 + 0.1f && !w.core.out().kellyLimp, "195/105 A (90 A): soft 50 %, no Low cap");
        // 6 imbalance timer restarts if it dips under 60 A
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; w.lemShareA = 340.f / 550.f; ms(700);
        w.lemShareA = 0.5f; ms(20); w.lemShareA = 340.f / 550.f; ms(700);
        rep(!w.core.out().lemImbalance, "130 A 0.7 s, dip, 0.7 s: timer restarts");
        // Cell sag (Adam 2026-10-21): relative cut, whole-pack sag vs load, debounced floors. PLACEHOLDER values.
        auto all = [&](float v) { for (int b = 0; b < BmsSim::kBoards; b++) for (int c = 0; c < 24; c++) w.cell[b][c] = v; };
        auto closed = [&]() { return w.core.out().state == BmsCore::CState::Closed; };
        auto cellLow = [&]() { return w.core.out().state == BmsCore::CState::Fault && (w.core.out().latched & BmsCore::F_CELL_LOW); };
        // 7 uniform 3.15 V under a 550 A launch for 3 s: no trip, full power (avg above floor + band)
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; all(3.15f); ms(3000);
        rep(closed() && w.core.out().kellyPct > 99.9f, "uniform 3.15 V @ 550 A 3 s: closed, full");
        // 8 uniform 3.05 V under load: pack-sag taper 50 %, closed
        all(3.05f); ms(20);
        rep(closed() && w.core.out().kellyPct > 49 && w.core.out().kellyPct < 51 && strcmp(w.core.out().kellyWhy, "pack sag") == 0,
            "uniform 3.05 V @ 550 A: pack sag 50%");
        // 9 load released, cells stay 3.15 V (rest): closed at 0.9 s, CELL-LOW by 1.1 s
        w.reset(); BmsSim::bringUp(w, false); w.packA = 0; all(3.15f); ms(900);
        { const bool c9 = closed(); ms(200); rep(c9 && cellLow(), "uniform 3.15 V at rest: open at 1 s, not 0.9 s"); }
        // 10 one cell 0.25 V below the rest under load: relative cell sag 60 %
        w.reset(); BmsSim::bringUp(w, false); w.packA = 400; all(3.70f); w.cell[0][6] = 3.45f; ms(20);
        { const auto &o = w.core.out();
          rep(closed() && o.kellyPct > 55 && o.kellyPct < 65 && strcmp(o.kellyWhy, "cell sag") == 0, "one cell 0.25 V low @ 400 A: sag ~60%"); }
        // 11 hard floor under load: all 2.95 V, closed at 0.4 s, CELL-LOW by 0.6 s
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; all(2.95f); ms(400);
        { const bool c11 = closed(); ms(200); rep(c11 && cellLow(), "all 2.95 V @ 550 A: floor opens at 0.5 s"); }
        // 12 brief launch dip: one cell 3.20 V for 300 ms at 550 A -> derated (cell sag), mains stay closed
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; w.cell[1][4] = 3.20f; ms(300);
        { const bool c12 = closed() && w.core.out().kellyPct < 1 && strcmp(w.core.out().kellyWhy, "cell sag") == 0;
          w.cell[1][4] = 3.80f; w.packA = 100; ms(20);
          rep(c12 && closed() && w.core.out().kellyPct > 99.9f, "one cell 3.20 V 300 ms @ 550 A: cut, no trip"); }
        // Derate events (Adam 2026-10-21: log every power cut with its reason). PLACEHOLDER 100 ms / 1 s debounce.
        auto logHas = [&](const char *t) { for (int i = 0; i < w.core.logCount(); i++) if (strstr(w.core.logAt(i).text, t)) return true; return false; };
        // 13 the 300 ms launch dip above: one event, reason cell sag, lowest 0 %, cleared after 1 s full power
        { const auto &o = w.core.out();
          const bool held = o.derateCode == BmsCore::D_CELL_SAG && o.derateEvents == 1 && logHas("DERATE cell sag");
          ms(1100);
          rep(held && w.core.out().derateCode == BmsCore::D_NONE && w.core.out().lastDerate == BmsCore::D_CELL_SAG &&
              w.core.out().derateMinPct < 1 && logHas("derate cleared: cell sag"), "launch dip: DERATE cell sag logged, cleared at 1 s"); }
        // 14 a 50 ms blip is shown live but not logged as an event
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; w.cell[1][4] = 3.20f; ms(50);
        { const bool live = w.core.out().kellyPct < 1; w.cell[1][4] = 3.80f; w.packA = 100; ms(1500);
          rep(live && w.core.out().derateEvents == 0 && !logHas("DERATE"), "50 ms cell blip: live cut, no derate event"); }
        // 15 reason change mid-event: cell sag -> LEM imbalance is logged, still one event
        w.reset(); BmsSim::bringUp(w, false); w.packA = 550; w.cell[0][6] = 3.45f; w.lemShareA = 340.f / 550.f; ms(1200);
        w.cell[0][6] = 3.80f; ms(50);
        rep(w.core.out().derateCode == BmsCore::D_LEM_IMB && w.core.out().derateEvents == 1 && logHas("DERATE LEM imbalance (was cell sag)"),
            "cell sag then LEM imbalance: reason change logged");
        printf("derate: %d/15 checks passed\n", 15 - dfail);
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
    return fails + cfail + lfail + kfail + tfail + sfail + dfail;
}
