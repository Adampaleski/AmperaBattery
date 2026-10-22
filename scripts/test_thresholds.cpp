// Host checks for include/Thresholds.h (serial-adjustable BMS thresholds).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/test_thresholds.cpp -o /tmp/thr && /tmp/thr
#include <stdio.h>
#include <string.h>
#include "Thresholds.h"

using namespace BmsCore;
static uint8_t g_mem[4284];
static uint8_t rd(int a) { return g_mem[a]; }
static void wr(int a, uint8_t v) { g_mem[a] = v; }
static const LogStore kStore{rd, wr};
static char g_out[4096];
static void sink(const char *s) { strncat(g_out, s, sizeof g_out - strlen(g_out) - 1); }
static int pass = 0, fail = 0;
static void check(bool ok, const char *name) {
    printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? pass : fail)++;
}
static bool said(const char *frag) { return strstr(g_out, frag) != nullptr; }
static void cmd(Thresholds &t, const char *l) { g_out[0] = 0; t.handleLine(l, sink); }

int main() {
    memset(g_mem, 0xFF, sizeof g_mem);
    {   // 1 blank flash = no overrides, Config untouched
        Thresholds t(kStore);
        const bool ok = t.load();
        Config c; t.apply(c); Config d;
        check(!ok && t.overrides() == 0 && c.overcurrentA == d.overcurrentA && c.overTempC == d.overTempC,
              "blank EEPROM: no overrides, code defaults");
    }
    {   // 2 set, persist, reload, apply
        Thresholds t(kStore); t.load();
        cmd(t, "TimbPct 30");
        cmd(t, "TfloorMs400");
        Thresholds t2(kStore);
        Config c; const bool ok = t2.load(); t2.apply(c);
        check(ok && c.imbalancePct == 30.0f && c.hardFloorMs == 400 && t2.overrides() == 2,
              "set imbPct 30 + floorMs 400 survives power-off");
    }
    {   // 3 trips may only be tightened
        Thresholds t(kStore); t.load();
        cmd(t, "TocA 700");
        const bool r1 = said("REFUSED");
        cmd(t, "TinstA 800");
        const bool r2 = said("REFUSED");
        cmd(t, "TtripC 60");
        const bool r3 = said("REFUSED");
        Config c; t.apply(c);
        check(r1 && r2 && r3 && c.overcurrentA == 650.0f && c.instantTripA == 750.0f && c.overTempC == 55.0f,
              "650 A / 750 A / 55 C cannot be loosened over serial");
    }
    {   // 4 order rules
        Thresholds t(kStore); t.load();
        cmd(t, "TderateC 55");
        const bool r1 = said("derateC < tripC");
        cmd(t, "TtripC 38");
        const bool r2 = said("derateC < tripC");   // default derate 40 >= 38
        cmd(t, "TsagStart 0.45");
        const bool r3 = said("sagStart < sagZero");
        Config base; base.lowCutV = 3.10f; t.setBase(base);
        cmd(t, "TfloorV 3.2");
        const bool r4 = said("floorV < low cutoff");
        cmd(t, "TderateC 30");
        cmd(t, "TtripC 48");
        Config c; t.apply(c);
        check(r1 && r2 && r3 && r4 && c.derateTempC == 30.0f && c.overTempC == 48.0f,
              "order rules: derate<trip, sagStart<sagZero, floor<L; tightening both temps in order works");
    }
    {   // 5 reset one + reset all
        Thresholds t(kStore); t.load();
        cmd(t, "TimbPct -");
        Config c; t.apply(c);
        const bool one = c.imbalancePct == 50.0f && c.hardFloorMs == 400;
        cmd(t, "TX");
        Thresholds t2(kStore); t2.load();
        Config c2; t2.apply(c2); Config d;
        check(one && t2.overrides() == 0 && c2.hardFloorMs == d.hardFloorMs && c2.overTempC == d.overTempC,
              "T<key> - resets one, TX resets all (persisted)");
    }
    {   // 6 corrupt storage ignored
        Thresholds t(kStore); t.load();
        cmd(t, "TrestA 30");
        g_mem[Thresholds::kBase + 9] ^= 0x40;   // flip a value bit
        Thresholds t2(kStore);
        Config c; const bool ok = t2.load(); t2.apply(c);
        check(!ok && c.restA == 20.0f, "CRC mismatch (torn/corrupt) -> code defaults");
    }
    {   // 7 bad input
        Thresholds t(kStore); t.load();
        cmd(t, "Tbogus 3");
        const bool a = said("unknown key");
        cmd(t, "TocA abc");
        const bool b = said("needs a number");
        cmd(t, "TocA nan");
        const bool c = said("REFUSED") || said("needs a number");
        check(a && b && c && t.overrides() == 0, "unknown key / non-number / NaN refused");
    }
    {   // 8 list shows every key with range + placeholder tag
        Thresholds t(kStore); t.load();
        cmd(t, "T");
        bool all = true;
        for (int i = 0; i < kThrN; i++) all = all && said(kThr[i].key);
        check(all && said("PLACEHOLDER") && said("range 100-650"), "T lists all keys, ranges, PLACEHOLDER tags");
    }
    {   // 9 layout: after FaultLog, inside EEPROM, defaults inside their own bounds
        bool inb = true; Config d;
        for (int i = 0; i < kThrN; i++) {
            const float v = kThr[i].isMs ? static_cast<float>(d.*(kThr[i].u)) : d.*(kThr[i].f);
            if (v < kThr[i].lo || v > kThr[i].hi) { inb = false; printf("  default out of bounds: %s\n", kThr[i].key); }
        }
        check(inb && Thresholds::kBase >= FaultLog::kEnd && Thresholds::kEnd <= 4284 && !Thresholds::orderError(d),
              "every code default is inside its bounds and passes order rules; storage after FaultLog");
    }
    {   // 10 THR2 keys: lemOcA (tighten-only, 400 A default) + drLowV
        Thresholds t(kStore); t.load(); cmd(t, "TX");
        Config d; const bool defs = d.lemTripA == 400.0f && d.derateLowV == 0.20f && d.derateTempC == 40.0f;
        cmd(t, "TlemOcA 500"); const bool loosen = said("outside");
        cmd(t, "TlemOcA 350"); cmd(t, "TdrLowV 0.25");
        Thresholds t2(kStore); t2.load(); Config c; t2.apply(c);
        check(defs && loosen && c.lemTripA == 350.0f && c.derateLowV == 0.25f && Thresholds::kMagic == 0x54485232,
              "THR2: lemOcA 400 default, cannot exceed 450, set 350 + drLowV 0.25 persist; derateC default 40");
        cmd(t, "TX");
    }
    printf("thresholds %d/%d\n", pass, pass + fail);
    return fail ? 1 : 0;
}
