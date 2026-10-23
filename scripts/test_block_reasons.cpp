// /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -Iinclude scripts/test_block_reasons.cpp -o /tmp/tbr && /tmp/tbr
// Block reasons (Adam t284): plain-language "why it won't" list. Must never touch faults or the log.
#include <cstdio>
#include <cstring>
#include "BlockReasons.h"
using namespace BmsCore;
static int n = 0, f = 0;
static void ok(bool c, const char *m) { n++; if (!c) { f++; printf("FAIL %s\n", m); } }
int main() {
    using namespace BmsCore::Block;
    In i; i.keyOn = true; i.ready = true;
    ok(reasons(Drive, i) == 0, "key on, ready, park off, drive: nothing blocks");
    i.parkBrake = true;
    ok(reasons(Drive, i) == ParkOn && strcmp(text(ParkOn), "Parking brake is on, release it to drive") == 0, "park on: exactly one reason, plain words");
    i.neutral = true;
    ok(reasons(Drive, i) == (ParkOn | Neutral) && count(reasons(Drive, i)) == 2, "park + neutral: two reasons");
    i = In{}; i.plugIn = true;
    uint32_t m = reasons(Drive, i);
    ok((m & KeyOff) && (m & Plugged) && !(m & Starting), "key off + plugged: key off and charger listed, no 'powering up'");
    ok(strcmp(text(Plugged), "Charger plugged in, drive locked") == 0 && strcmp(text(KeyOff), "Key is off") == 0, "texts match Adam's wording");
    i = In{}; i.keyOn = true; i.keyRefused = true;
    ok(reasons(Drive, i) == KeyCycle, "key cycle needed after unplug");
    i = In{}; i.keyOn = true; ok(reasons(Drive, i) == Starting, "key on, contactors closing: powering up");
    i = In{}; i.keyOn = true; i.ready = true; i.pinToDrive = true;
    ok(reasons(Drive, i) == PinDrive && strcmp(text(PinDrive), "Enter PIN or bring phone to drive") == 0, "PIN to Drive on, no phone, no PIN: blocked with the pop-up text");
    i.phone = true; ok(reasons(Drive, i) == 0, "PIN to Drive: phone nearby unblocks");
    i.phone = false; i.pinOk = true; ok(reasons(Drive, i) == 0, "PIN to Drive: PIN entered unblocks");
    i.pinToDrive = false; i.pinOk = false; ok(reasons(Drive, i) == 0, "PIN to Drive off (default): no phone still drives");
    // wake
    i = In{}; ok(reasons(Wake, i) == 0, "wake: off, parked, nothing blocks");
    i.estopLatched = true; ok(reasons(Wake, i) == Estop, "wake refused: e-stop latched");
    i = In{}; i.plugIn = true; ok(reasons(Wake, i) == Plugged, "wake refused: charger plugged in");
    i = In{}; i.prechargeAllowed = false; ok(reasons(Wake, i) == ParkOffWake, "wake refused: parking brake not set");
    i.parkFault = true; ok(reasons(Wake, i) == ParkFault, "park contact fault shown instead of 'set the brake'");
    // shift (into / out of reverse)
    i = In{}; i.keyOn = true; ok(reasons(Shift, i) == BrakeShift && strcmp(text(BrakeShift), "Brake must be pressed to shift") == 0, "shift without brake: refused");
    i.brakePressed = true; ok(reasons(Shift, i) == 0, "shift with brake: ok");
    // drive settings (phone-as-key)
    i = In{}; i.phone = true; ok(reasons(Setting, i) == KeyOff, "settings: key off locks even with phone");
    i.keyOn = true; ok(reasons(Setting, i) == 0, "settings: key on + phone ok");
    i.phone = false; ok(reasons(Setting, i) == PinSetting, "settings: key on, no phone: PIN needed");
    i.pinOk = true; ok(reasons(Setting, i) == 0, "settings: PIN entered unlocks");
    for (uint32_t b = 0; b < kBits; b++) ok(text(1u << b)[0] != 0, "every bit has text");
    // tie to BmsCore: plugged in -> core says drive locked -> reasons() says charger plugged in; no fault, no log
    Core c; c.reset(); Inputs in{}; in.plugIn = true;
    for (uint32_t t = 10; t < 4000; t += 10) { in.nowMs = t; c.step(in); }
    const int logBefore = c.logCount(); const uint32_t fb = c.out().faults, lb = c.out().latched;
    In j; fromCore(j, c.out(), false, true);
    for (int a = 0; a < kActions; a++) m = reasons(Drive, j);
    ok((m & Plugged) && c.out().driveLocked, "BmsCore plugIn -> 'Charger plugged in, drive locked'");
    ok(c.out().faults == fb && c.out().latched == lb && c.logCount() == logBefore, "asking for reasons sets no fault and logs nothing");
    printf("block_reasons: %d/%d\n", n - f, n); return f ? 1 : 0;
}
