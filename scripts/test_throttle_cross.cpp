// Two-Teensy throttle cross-check + clamp (include/ThrottleCross.h).
#include <cstdio>
#include <cstdlib>
#include "ThrottleCross.h"
using namespace ThrottleCross;
static int pass = 0, fail = 0;
static void check(bool c, const char *m) { std::printf("%s %s\n", c ? "PASS" : "FAIL", m); (c ? pass : fail)++; }
int main() {
    char m[160];
    { Plant p; p.pedal = 0; p.run(3);
      check(p.kellyPct == 0 && !p.chkA.latched && !p.chkB.latched, "boot: clamp 0 V until both have seen the other, no fault latched"); }
    // normal driving: random pedal steps, mph, mode changes (A -> B sync) for 120 s: no false trip
    { std::srand(3); Plant p; uint32_t trips = 0; float maxK = 0;
      for (int i = 0; i < 120000; i++) {
          if (i % 700 == 0) p.pedal = std::rand() % 101;
          if (i % 5000 == 0) p.mph = std::rand() % 50;
          if (i % 9000 == 0) p.mode = static_cast<uint8_t>(std::rand() % 6);   // H..SAND (Kid needs PIN)
          p.step(); if (p.kellyPct > maxK) maxK = p.kellyPct;
          if (p.chkA.latched || p.chkB.latched) { trips++; break; }
      }
      std::snprintf(m, sizeof m, "120 s random pedal / speed / mode changes: no false trip (A %s, B %s), Kelly reached %.0f%%",
                    faultName(p.chkA.latched), faultName(p.chkB.latched), maxK);
      check(trips == 0 && maxK > 90, m); }
    auto drop = [](void (*inj)(Plant &), uint32_t lim, uint8_t wantA, uint8_t wantB) {
        Plant p; p.pedal = 60; p.run(1000); inj(p); int t = -1;
        for (int i = 1; i <= 2000; i++) { p.step(); if (p.kellyPct == 0 && t < 0) t = i; }
        return t >= 0 && static_cast<uint32_t>(t) <= lim && p.kellyPct == 0 && (p.chkA.latched & wantA) == wantA && (p.chkB.latched & wantB) == wantB;
    };
    check(drop([](Plant &p) { p.hungA = true; }, 10, 0, TF_PEER_LOST), "A hang: clamp 0 V <= 10 ms, B logs PEER-LOST");
    check(drop([](Plant &p) { p.hungB = true; }, 10, TF_PEER_LOST, 0), "B hang: clamp 0 V <= 10 ms, A logs PEER-LOST");
    check(drop([](Plant &p) { p.pedalBOffset = 10; }, 100, TF_DISAGREE, TF_DISAGREE), "disagree 10% (B pedal): both drop <= 100 ms, both log DISAGREE");
    check(!drop([](Plant &p) { p.pedalBOffset = 3; }, 2000, TF_DISAGREE, 0), "disagree 3% (< 5% tolerance): no trip");
    check(drop([](Plant &p) { p.tabB.p[DriveProfile::M_H].rampAps = 2100; }, 80, TF_PROFILE, TF_PROFILE), "B profile copy differs (CRC): both drop, PROFILE-MISMATCH");
    check(drop([](Plant &p) { p.potStuckPct = 50; p.faultA = true; }, 10, TF_LOCAL, 0), "digipot stuck 50% + A fault: clamp holds 0 V <= 10 ms");
    { Plant p; p.pedal = 60; p.run(1000); p.potStuckPct = 50; p.run(500);
      check(p.kellyPct > 0 && p.kellyPct <= 60, "digipot stuck 50% with no fault: Kelly sees pedal x 50% (<= pedal; reduce-only)"); }
    { Plant p; p.pedal = 60; p.run(1000); p.hungA = true; p.run(5); const bool hold = p.kellyPct > 0; p.run(1);
      check(hold && p.kellyPct == 0, "one-shot: Q holds 5 ms (kOneShotMs) after the last toggle, then 0"); }
    std::printf("throttlecross: %d/%d checks passed\n", pass, pass + fail);
    return fail ? 1 : 0;
}
