// Host test: chief review CRITICAL #1 — single-string precharge never closes Main+ on a timer.
#include "PrechargeGate.h"
#include <cstdio>
#include <cmath>
using namespace PrechargeGate;
static int fails = 0, n = 0;
#define CHECK(c, m) do { n++; if (c) printf("PASS %s\n", m); else { printf("FAIL %s\n", m); fails++; } } while (0)

// Mirror of the ContactorSeq single-string loop (Open->Precharge->MainClose / RetryWait / Lockout).
enum S { Open, Pre, Main, Retry, Lock };
struct Run { S s = Open; uint32_t t0 = 0; uint8_t retries = 0; int closes = 0; int fails = 0; };
static void tick(Run &r, uint32_t now, bool key, bool bv, float bus, bool pv, float pack, const Cfg &c) {
    const uint32_t el = now - r.t0;
    switch (r.s) {
        case Open: if (key) { r.s = Pre; r.t0 = now; } break;
        case Pre: {
            Step st = precharge(el, bv, bus, pv, pack, c);
            if (st == Step::CloseMain) { r.s = Main; r.closes++; r.t0 = now; }
            else if (st == Step::Fail) { r.fails++; r.s = retryAllowed(r.retries, c) ? Retry : Lock; r.t0 = now; }
        } break;
        case Retry: if (el >= c.retryWaitMs && key) { r.s = Pre; r.t0 = now; } break;
        default: break;
    }
}

int main() {
    const Cfg c{200, 5000, 30000, 2, 0.95f, 50.0f};
    // 1. The old bug: no HV sense (busValid=false) -> timer must FAIL, never close.
    CHECK(precharge(5000, false, 0, true, 150, c) == Step::Fail, "no HV sense: timeout -> Fail (was: close Main+ on 5 s timer)");
    CHECK(precharge(60000, false, 0, true, 150, c) == Step::Fail, "no HV sense: never CloseMain at any time");
    CHECK(precharge(4999, false, 0, true, 150, c) == Step::Wait, "no HV sense: waits before timeout");
    // 2. Delta rule.
    CHECK(precharge(300, true, 142.5f, true, 150, c) == Step::CloseMain, "bus = 95% of pack after minMs -> CloseMain");
    CHECK(precharge(300, true, 142.4f, true, 150, c) == Step::Wait, "bus 94.9% -> Wait");
    CHECK(precharge(199, true, 150, true, 150, c) == Step::Wait, "within delta but before minMs -> Wait");
    CHECK(precharge(5000, true, 100, true, 150, c) == Step::Fail, "bus short of delta at timeout -> Fail");
    // 3. Missing / bogus pack voltage never closes (old code fell back to bus >= 80 V).
    CHECK(precharge(1000, true, 150, false, 0, c) == Step::Wait, "pack invalid -> no close");
    CHECK(precharge(5000, true, 150, true, 10, c) == Step::Fail, "pack < 50 V (not a reading) -> Fail at timeout, no 80 V fallback");
    CHECK(precharge(1000, true, NAN, true, 150, c) == Step::Wait, "NaN bus -> no close");
    CHECK(precharge(1000, true, 150, true, NAN, c) == Step::Wait, "NaN pack -> no close");
    // 4. Retry / lockout: no sense -> 3 attempts, 30 s apart, then Lockout, zero closes.
    Run r; uint32_t now = 0;
    for (; now < 200000; now += 10) tick(r, now, true, false, 0, true, 150, c);
    CHECK(r.closes == 0, "no HV sense, 200 s key-on: Main+ never closed");
    CHECK(r.fails == 3 && r.s == Lock, "3 failed attempts (1 + 2 retries) then Lockout");
    // 5. Retry then success: bus comes up during the 2nd attempt.
    Run q; now = 0; bool up = false;
    for (; now < 100000 && q.s != Main; now += 10) { if (q.fails == 1 && q.s == Pre) up = true; tick(q, now, true, up, up ? 149.f : 0.f, true, 150, c); }
    CHECK(q.s == Main && q.fails == 1 && q.closes == 1, "1 fail, retry, bus in delta -> CloseMain once");
    printf("precharge gate: %d/%d passed\n", n - fails, n);
    return fails ? 1 : 0;
}
