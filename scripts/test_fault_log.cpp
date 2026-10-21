// Host checks for include/FaultLog.h (power-off survival, wrap, torn write, rate limit).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/test_fault_log.cpp -o /tmp/flog && /tmp/flog
#include <stdio.h>
#include <string.h>
#include "BmsCore.h"
#include "FaultLog.h"

using namespace BmsCore;
static uint8_t g_mem[4284];
static int g_writes = 0;
static uint8_t rd(int a) { return g_mem[a]; }
static void wr(int a, uint8_t v) { if (g_mem[a] != v) { g_mem[a] = v; g_writes++; } }  // like EEPROM.update
static const LogStore kStore{rd, wr};

int main() {
    int fails = 0, n = 0;
    auto rep = [&](bool ok, const char *name) { n++; if (!ok) fails++; printf("%s faultlog %s\n", ok ? "PASS" : "FAIL", name); };
    memset(g_mem, 0xFF, sizeof g_mem);
    for (int i = 0; i < 32; i++) g_mem[i] = static_cast<uint8_t>(0x40 + i);   // stand-in Settings blob

    { FaultLog L; L.begin(kStore);
      rep(L.count() == 0 && L.boot() == 1, "blank EEPROM (0xFF) -> empty log, boot 1"); }

    { FaultLog L; L.begin(kStore);
      L.edges(0, F_CELL_LOW, 5000, 3290);
      L.edges(F_CELL_LOW, F_CELL_LOW | F_OVERCURRENT, 7000, 760);
      L.add(LogKind::Note, NOTE_BOOT, 0);
      // power off: object gone, memory stays
      FaultLog R; R.begin(kStore); LogRec r0, r2;
      const bool ok = R.count() == 3 && R.boot() == 2 && R.get(0, r0) && R.get(2, r2) &&
                      r0.kind == LogKind::Note && r2.kind == LogKind::FaultSet && r2.code == 3 && r2.value == 3290 && r2.ms == 5000 && r2.boot == 1;
      rep(ok, "3 records survive power-off, newest first, boot 2 next"); }

    { bool same = true; for (int i = 0; i < 32; i++) same &= g_mem[i] == 0x40 + i;
      rep(same, "Settings area (0..31) untouched"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.minGapMs = 0; L.maxPerBoot = 1000;
      for (uint32_t i = 0; i < 60; i++) L.add(LogKind::FaultSet, static_cast<uint8_t>(i % 13), i * 10, static_cast<int16_t>(i));
      FaultLog R; R.begin(kStore); LogRec r0, rl;
      const bool ok = R.count() == FaultLog::kSlots && R.get(0, r0) && r0.value == 59 && R.get(FaultLog::kSlots - 1, rl) && rl.value == 60 - FaultLog::kSlots;
      rep(ok, "60 writes into 48 slots: keeps newest 48 after reboot"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.minGapMs = 0;
      for (int i = 0; i < 5; i++) L.add(LogKind::FaultSet, 6, 100u * i, static_cast<int16_t>(i));
      g_mem[FaultLog::kBase + 4 * FaultLog::kRecSize + 9] ^= 0x5A;   // newest record torn mid-write
      FaultLog R; R.begin(kStore); LogRec r0;
      rep(R.count() == 4 && R.get(0, r0) && r0.value == 3, "torn newest record (bad CRC) skipped, other 4 kept");
      R.add(LogKind::FaultSet, 6, 9999, 77); FaultLog Q; Q.begin(kStore); LogRec q0;
      rep(Q.get(0, q0) && q0.value == 77 && q0.seq == 5, "next write after a torn record continues the sequence"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.begin(kStore);
      int ok = 0; for (uint32_t t = 0; t < 5000; t += 10) ok += L.add(LogKind::FaultSet, 3, t) ? 1 : 0;
      rep(ok == 5, "flapping CELL-LOW every 10 ms for 5 s -> 5 records (1 s gap)"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.begin(kStore); L.minGapMs = 0;
      const int before = g_writes;
      for (uint32_t t = 0; t < 200; t++) L.add(LogKind::FaultSet, static_cast<uint8_t>(t % 13), t);
      LogRec r0; L.get(0, r0);
      rep(L.writtenThisBoot() == L.maxPerBoot && L.limited() && r0.kind == LogKind::Note && r0.code == NOTE_LIMIT,
          "200 events in one boot -> 23 records + 'log limit reached', then stops");
      rep(g_writes - before <= L.maxPerBoot * FaultLog::kRecSize, "byte writes bounded by limit x 20 B"); }

    { FaultLog L; L.begin(kStore); L.clear();
      bool inside = true; for (int a = 0; a < 4284; a++) if ((a < FaultLog::kBase || a >= FaultLog::kEnd) && a >= 32 && g_mem[a] != 0xFF) inside = false;
      rep(inside && FaultLog::kEnd <= 4284, "never writes outside 256..1215"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.begin(kStore);
      L.add(LogKind::DerateSet, D_CELL_SAG, 1000, 12); L.add(LogKind::DerateClear, D_CELL_SAG, 3000, 0);
      FaultLog R; R.begin(kStore); LogRec r0, r1;
      rep(R.count() == 2 && R.get(0, r0) && R.get(1, r1) && r0.kind == LogKind::DerateClear && r1.kind == LogKind::DerateSet &&
          r1.code == D_CELL_SAG && r1.value == 12 && R.deratesThisBoot() == 0, "derate set/clear survive power-off with reason + %"); }

    { FaultLog L; L.begin(kStore); L.clear(); L.begin(kStore); L.minGapMs = 0;
      for (uint32_t t = 0; t < 40; t++) L.add((t & 1) ? LogKind::DerateClear : LogKind::DerateSet, D_PACK_SAG, t * 100, 50);
      const bool fault = L.add(LogKind::FaultSet, 3, 9000, 2990);
      rep(L.deratesThisBoot() == L.maxDeratePerBoot && L.deratesDropped() == 40 - L.maxDeratePerBoot && fault && !L.limited(),
          "40 derates in one boot: 12 saved, rest dropped, a real fault still gets a slot"); }

    printf("faultlog: %d/%d checks passed\n", n - fails, n);
    return fails ? 1 : 0;
}
