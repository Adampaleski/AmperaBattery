// PC run of the BMS core fault scenarios (same code as the Teensy + browser).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/sim_bms.cpp -o /tmp/sim && /tmp/sim
#include <stdio.h>
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
    printf("charge limit map: 80%% = %.3f V, 90%% = %.3f V, 100%% = %.3f V per cell (42S Kelly clamp %.3f V)\n",
           BmsCore::ceilingForPct(80), BmsCore::ceilingForPct(90), BmsCore::ceilingForPct(100), 168.0f / 42);
    return fails + cfail;
}
