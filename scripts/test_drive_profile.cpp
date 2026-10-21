// Drive-mode pedal maps + reduce-only trim (include/DriveProfile.h).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O2 -Iinclude scripts/test_drive_profile.cpp -o /tmp/dp && /tmp/dp
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "DriveProfile.h"
using namespace DriveProfile;
static int pass = 0, fail = 0;
static void check(bool c, const char *m) { std::printf("%s %s\n", c ? "PASS" : "FAIL", m); (c ? pass : fail)++; }

// run trim to steady state at pedal/mph
static float settle(const Profile &p, float pedal, float mph, bool sv = true, float allow = 100) {
    Trim t; In in; in.pedalPct = pedal; in.mph = mph; in.speedValid = sv; in.allowPct = allow; in.ok = true;
    float o = 0; for (int i = 0; i < 20000; i++) o = t.step(in, p, 1.0f); return o;
}
int main() {
    Table tab;
    char m[160];
    // 1. each mode's curve: monotonic, 0 -> 0, full pedal -> currentPct, never above pedal
    for (int k = 0; k < kModes; k++) {
        const Profile &p = tab.p[k];
        bool mono = true, below = true; float prev = -1;
        for (int x = 0; x <= 100; x++) {
            const float y = curve(x / 100.0f, p.expo) * 100;
            if (y < prev - 1e-4f) mono = false; if (y > x + 1e-3f) below = false; prev = y;
        }
        const float full = settle(p, 100, 0), half = settle(p, 50, 0);
        const float want = curve(0.5f, p.expo) * p.currentPct;
        std::snprintf(m, sizeof m, "%-4s curve: monotonic, <= pedal, 100%% -> %.1f%% (cap %.0f), 50%% -> %.1f%% (want %.1f)", p.key, full, p.currentPct, half, want);
        check(mono && below && full > p.currentPct - 0.5f && full <= p.currentPct + 1e-3f && half > want - 0.5f && half <= want + 0.01f, m);
    }
    // 2. top speed: full output 2 mph below, 0 at/above top; Kid 0 without speed
    for (int k = 0; k < kModes; k++) {
        const Profile &p = tab.p[k];
        const float below = settle(p, 100, p.topMph - kTaperMph - 0.1f), at = settle(p, 100, p.topMph), over = settle(p, 100, p.topMph + 5);
        std::snprintf(m, sizeof m, "%-4s top %.0f mph: %.1f%% at %.1f mph, %.2f%% at top, %.2f%% above", p.key, p.topMph, below, p.topMph - 2.1f, at, over);
        check(below > p.currentPct - 0.5f && at == 0 && over == 0, m);
    }
    check(tab.p[M_KID].topMph <= 10.0f, "Kid top speed ~10 mph (PROPOSED 10)");
    check(settle(tab.p[M_KID], 100, 0, false) == 0, "Kid: no valid speed -> output 0");
    check(settle(tab.p[M_H], 100, 0, false) > 99, "High: no speed -> pedal passes (top speed not enforced, Kelly RPM cap only)");
    // 3. trim never exceeds the pedal: random pedal (incl. steps/noise), speed, allowance, mode edits
    {
        std::srand(7); bool okAll = true, wipOk = true; float worst = -1e9f;
        for (int k = 0; k < kModes; k++) {
            Trim t; In in; in.ok = true; in.speedValid = true;
            for (int i = 0; i < 200000; i++) {
                if (i % 37 == 0) in.pedalPct = std::rand() % 1010 / 10.0f;
                if (i % 501 == 0) in.mph = std::rand() % 600 / 10.0f;
                if (i % 997 == 0) in.allowPct = std::rand() % 101;
                in.pedalPct += (std::rand() % 21 - 10) / 10.0f; in.pedalPct = clampf(in.pedalPct, 0, 100);
                const float o = t.step(in, tab.p[k], 1.0f);
                if (o - in.pedalPct > worst) worst = o - in.pedalPct;
                if (o > in.pedalPct + 1e-5f || o < 0) okAll = false;
                const uint8_t w = Trim::wiper(o, in.pedalPct);
                if (in.pedalPct * w / 128.0f > o + 1e-3f || w > 128) wipOk = false;
            }
        }
        std::snprintf(m, sizeof m, "trim <= pedal over 1.4 M random steps (worst out-pedal %.4f %%)", worst);
        check(okAll, m);
        check(wipOk, "MCP4131 wiper: pedal x wiper/128 <= trim output, wiper <= 128");
        Trim t; In in; in.pedalPct = 80; in.ok = true; t.step(in, tab.p[M_H], 1); in.ok = false;
        check(t.step(in, tab.p[M_H], 1) == 0 && Trim::wiper(0, 80) == 0, "fault / not ok -> output 0, wiper 0 at once");
    }
    // 4. ramps: Snow slower than Sand
    {
        auto tRise = [&](int k) { Trim t; In in; in.pedalPct = 100; in.ok = true; in.speedValid = true; int ms = 0;
            while (t.step(in, tab.p[k], 1) < 0.9f * tab.p[k].currentPct && ms < 60000) ms++; return ms; };
        const int s = tRise(M_SNOW), d = tRise(M_SAND), kd = tRise(M_KID);
        std::snprintf(m, sizeof m, "ramp to 90%% of cap: Snow %d ms, Sand %d ms, Kid %d ms (Snow/Kid soft, Sand fast)", s, d, kd);
        check(s > 4 * d && kd > s && d < 400, m);
        Trim t; In in; in.ok = true; in.pedalPct = 100; for (int i = 0; i < 5000; i++) t.step(in, tab.p[M_H], 1);
        in.pedalPct = 0; check(t.step(in, tab.p[M_H], 1) == 0, "pedal release: output follows down at once (never above raw pedal)");
    }
    // 5. Kid PIN
    {
        Selector s;
        check(s.select(M_KID) == Selector::NO_PIN_SET && s.mode == M_H, "Kid refused until a PIN is set");
        s.pin = 4321;
        check(s.select(M_KID) == Selector::OK && s.mode == M_KID, "enter Kid (PIN set)");
        check(s.select(M_H) == Selector::NEED_PIN && s.mode == M_KID, "leave Kid without PIN: refused");
        check(s.select(M_SNOW, 1111) == Selector::BAD_PIN && s.mode == M_KID, "leave Kid with wrong PIN: refused");
        check(s.select(M_H, 4321) == Selector::OK && s.mode == M_H, "leave Kid with PIN: ok");
    }
    // 6. adjustable + bounds
    check(tab.set(M_SNOW, "cur", 35) && tab.p[M_SNOW].currentPct == 35 && !tab.set(M_SNOW, "cur", 150) && !tab.set(M_KID, "top", 0.5f) && !tab.set(M_H, "bogus", 1),
          "profile edit: in-bounds accepted, out-of-bounds / unknown refused");
    tab.reset();
    // 7. timing: one 1 kHz tick (filter + curve + ramp + wiper) must be << 2 ms
    {
        Trim t; In in; in.ok = true; in.speedValid = true; volatile float sink = 0;
        const int N = 1000000; auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; i++) { in.pedalPct = (i % 100); in.mph = (i % 50); sink += t.step(in, tab.p[i % kModes], 1.0f) + Trim::wiper(sink, 50); }
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / N;
        std::snprintf(m, sizeof m, "loop latency: %.3f us per tick on host (budget 2000 us; Teensy: analogRead ~17 us + 16-bit bit-bang ~4 us)", us);
        check(us < 2000.0, m);
        // simulated 1 kHz schedule: worst period stays < 2 ms when compute < 1 ms
        check(us + 21.0 < 1000.0, "1 kHz period: compute + ADC + SPI estimate < 1 ms -> worst latency < 2 ms (measure maxPeriodUs on the bench)");
    }
    std::printf("driveprofile: %d/%d checks passed\n", pass, pass + fail);
    return fail ? 1 : 0;
}
