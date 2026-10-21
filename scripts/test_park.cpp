// /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/test_park.cpp -o /tmp/tp && /tmp/tp
#include <cstdio>
#include "ParkState.h"
using namespace ParkState;
static int n = 0, f = 0;
static void ok(bool c, const char *m) { n++; if (!c) { f++; printf("FAIL %s\n", m); } }
int main() {
    Detector d; In in; uint32_t t = 0;
    auto run = [&](uint32_t ms) { for (uint32_t i = 0; i < ms; i++) d.step(++t, in); };
    in.rpmValid = true; in.neutral = true;
    run(10); ok(!d.parked && !d.prechargeAllowed(), "off: not parked, precharge blocked");
    in.contactA = in.contactB = true; run(10); ok(d.parked && d.prechargeAllowed() && d.kidChangeAllowed(), "park on");
    in.neutral = false; run(5); ok(!d.parked && d.prechargeAllowed(), "not neutral -> not parked, precharge still ok");
    in.neutral = true; in.contactB = false; run(50); ok(!d.parked && !d.fault, "disagree <100 ms: no fault yet");
    run(60); ok(d.fault & PF_DISAGREE && !d.prechargeAllowed(), "contact disagreement fault");
    in.contactB = true; run(5); ok(!d.fault && (d.latched & PF_DISAGREE), "disagree clears, stays latched");
    d.clear(); in.rpm = 600; run(400); ok((d.fault & PF_BRAKE_MOVING) && !d.parked && !d.kidChangeAllowed(), "brake on while moving");
    in.contactA = in.contactB = false; run(5); ok(!d.kidChangeAllowed(), "Kid change blocked while moving");
    in.rpm = 0; in.rpmValid = false; in.contactA = in.contactB = true; run(5); ok(!d.parked, "stale Kelly CAN -> not parked");
    printf("park: %d/%d\n", n - f, n); return f ? 1 : 0;
}
