// Host test: chief review CRITICAL #1 — single-string precharge never closes Main+ on a timer.
#include "PrechargeGate.h"
#include <cstdio>
#include <cmath>
using namespace PrechargeGate;
static int fails = 0, n = 0;
#define CHECK(c, m) do { n++; if (c) printf("PASS %s\n", m); else { printf("FAIL %s\n", m); fails++; } } while (0)

// Mirror of the ContactorSeq single-string loop (Open->Precharge->MainClose / Lockout). Key off = key cycle.
enum S { Open, Pre, Main, Lock };
struct Run { S s = Open; uint32_t t0 = 0; int closes = 0; int fails = 0; int preEntries = 0; };
static void tick(Run &r, uint32_t now, bool key, bool bv, float bus, bool pv, float pack, const Cfg &c) {
    const uint32_t el = now - r.t0;
    if (!key) { r.s = Open; return; }                 // key cycle: openAll("reset") clears Lockout
    switch (r.s) {
        case Open: r.s = Pre; r.t0 = now; r.preEntries++; break;
        case Pre: {
            Step st = precharge(el, bv, bus, pv, pack, c);
            if (st == Step::CloseMain) { r.s = Main; r.closes++; r.t0 = now; }
            else if (st == Step::Fail) { r.fails++; r.s = Lock; r.t0 = now; }   // no retry
        } break;
        default: break;
    }
}

int main() {
    const Cfg c{300, 3000, 0.95f, 50.0f};   // agreed 300 ms / 3 s
    // 1. The old bug: no HV sense (busValid=false) -> timer must FAIL, never close.
    CHECK(precharge(3000, false, 0, true, 150, c) == Step::Fail, "no HV sense: timeout -> Fail (was: close Main+ on 5 s timer)");
    CHECK(precharge(60000, false, 0, true, 150, c) == Step::Fail, "no HV sense: never CloseMain at any time");
    CHECK(precharge(2999, false, 0, true, 150, c) == Step::Wait, "no HV sense: waits before timeout");
    // 2. Delta rule.
    CHECK(precharge(300, true, 142.5f, true, 150, c) == Step::CloseMain, "bus = 95% of pack after minMs -> CloseMain");
    CHECK(precharge(300, true, 142.4f, true, 150, c) == Step::Wait, "bus 94.9% -> Wait");
    CHECK(precharge(299, true, 150, true, 150, c) == Step::Wait, "within delta but before minMs -> Wait");
    CHECK(precharge(3000, true, 100, true, 150, c) == Step::Fail, "bus short of delta at timeout -> Fail");
    // 3. Missing / bogus pack voltage never closes (old code fell back to bus >= 80 V).
    CHECK(precharge(1000, true, 150, false, 0, c) == Step::Wait, "pack invalid -> no close");
    CHECK(precharge(3000, true, 150, true, 10, c) == Step::Fail, "pack < 50 V (not a reading) -> Fail at timeout, no 80 V fallback");
    CHECK(precharge(1000, true, NAN, true, 150, c) == Step::Wait, "NaN bus -> no close");
    CHECK(precharge(1000, true, 150, true, NAN, c) == Step::Wait, "NaN pack -> no close");
    // 4. One fail -> Lockout, NO automatic retry (chief 2026-10-22): key held on 200 s.
    Run r; uint32_t now = 0;
    for (; now < 200000; now += 10) tick(r, now, true, false, 0, true, 150, c);
    CHECK(r.closes == 0, "no HV sense, 200 s key-on: Main+ never closed");
    CHECK(r.fails == 1 && r.preEntries == 1 && r.s == Lock, "ONE failed attempt -> Lockout, precharge never re-energised");
    // 5. Lockout persists even when the bus comes good; only a key cycle clears it.
    for (; now < 260000; now += 10) tick(r, now, true, true, 150, true, 150, c);
    CHECK(r.s == Lock && r.closes == 0, "bus now in delta, key still on: stays locked, no close");
    tick(r, now, false, true, 150, true, 150, c); now += 10;
    for (uint32_t e = now + 1000; now < e; now += 10) tick(r, now, true, true, 150, true, 150, c);
    CHECK(r.s == Main && r.closes == 1 && r.preEntries == 2, "key cycle -> fresh precharge -> CloseMain once");
    // 6. Timing: the agreed 3 s timeout fails at 3000 ms, not 5000.
    CHECK(precharge(3000, true, 0, true, 150, c) == Step::Fail && precharge(2990, true, 0, true, 150, c) == Step::Wait, "3 s timeout");
    printf("precharge gate: %d/%d passed\n", n - fails, n);
    return fails ? 1 : 0;
}
