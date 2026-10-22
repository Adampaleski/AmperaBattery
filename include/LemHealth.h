#pragma once
// Dead-LEM / LEM-health checker (chief + Sparky 2026-10-21/22, CRITICAL before first drive).
// Pure C++ (no Arduino), host-tested by scripts/test_lem_health.cpp. One instance per LEM DHAB S/44
// (LEM A on Teensy A, LEM B on Teensy B). Uses BmsCore::dhabAmps maths, so pin volts in = same as CurrentSense.
// NOT wired into firmware yet: CURRENT_SENSE_ENABLE=0, split-LEM read not built. Report-only until then;
// a hard fault here is meant to engage the 10 mph (Kid/Low) cap, never to close anything.
//
// Checks (thresholds PROPOSED, from Sparky's notes; confirm on the bench):
//   RAIL   any output (ch1, ch2) within railV (0.2 V) of 0 V or of Vc, or Vc outside 4.5-5.5 V
//          -> dead / disconnected. Datasheet (BmsCore.h notes) says a live output clamps at 0.25 / 4.75 V,
//          so a working sensor never sits inside the 0.2 V rail band.
//   RANGE  |I| > rangeA (450 A) -> out of range.
//   AGREE  |I_ch2| < agreeBelowA (15 A) and |I_ch1 - I_ch2| > agreeTolA (5 A): low and high channels disagree.
//          ch2 is 6.15 mV/A, so 5 A = ~31 mV at the sensor (~20 mV at the pin): PLACEHOLDER, bench it.
//   ZERO   key-on self-test with mains open (no current can flow): |I| > zeroTolA (5 A) -> zero offset bad.
//          The stored zero itself comes from 'Z' (CurrentSense::calibrateZero, EEPROM Settings).
// Each live check must hold for holdMs (100 ms; AGREE 500 ms) before it latches. Latched until reset()
// at a key cycle, and reset() is refused while the condition is still present.
#include <cstdint>
#include <cmath>
#include "BmsCore.h"

namespace LemHealth {
enum Code : uint8_t { OK = 0, RAIL = 1, RANGE = 2, AGREE = 3, ZERO = 4 };
inline const char *name(uint8_t c) {
    switch (c) { case RAIL: return "LEM dead/disconnected"; case RANGE: return "LEM out of range";
                 case AGREE: return "LEM channels disagree"; case ZERO: return "LEM zero offset"; default: return "ok"; }
}
struct Limits {               // all PROPOSED
    float railV = 0.20f;      // sensor-side volts from 0 V / Vc
    float rangeA = 450.0f;
    float agreeBelowA = 15.0f;
    float agreeTolA = 5.0f;
    float zeroTolA = 5.0f;
    uint32_t holdMs = 100;
    uint32_t agreeHoldMs = 500;
};
struct Monitor {
    Limits lim;
    BmsCore::CurrentCal cal;
    uint8_t latched = OK;     // first fault that latched
    uint8_t now = OK;         // condition seen this sample (before debounce)
    float lastA = 0;
    uint32_t trips = 0;
    uint32_t sinceMs[5] = {0, 0, 0, 0, 0};
    bool active[5] = {false, false, false, false, false};

    bool hardFault() const { return latched != OK; }  // -> engage 10 mph cap

    // Raw channel currents, same maths as dhabAmps (no clamp/deadband) so AGREE compares real readings.
    void channelAmps(float adc1, float adc2, float vc, float &i1, float &i2) const {
        const float s = vc / 5.0f;
        i1 = (adc1 / cal.divider - cal.off1Frac * vc) / (cal.g1VperA * s);
        i2 = (adc2 / cal.divider - cal.off2Frac * vc) / (cal.g2VperA * s);
        if (cal.invert) { i1 = -i1; i2 = -i2; }
    }
    // What is wrong in this one sample (priority RAIL > RANGE > AGREE).
    uint8_t classify(float adc1, float adc2, float supplyAdc, float &amps) const {
        amps = 0;
        const float vc = supplyAdc / cal.divider;
        if (vc < 4.5f || vc > 5.5f) return RAIL;
        const float v1 = adc1 / cal.divider, v2 = adc2 / cal.divider;
        if (v1 < lim.railV || v1 > vc - lim.railV || v2 < lim.railV || v2 > vc - lim.railV) return RAIL;
        bool ok = false;
        amps = BmsCore::dhabAmps(cal, adc1, adc2, supplyAdc, &ok);
        if (!ok) return RAIL;
        if (std::fabs(amps) > lim.rangeA) return RANGE;
        float i1, i2; channelAmps(adc1, adc2, vc, i1, i2);
        if (std::fabs(i2) < lim.agreeBelowA && std::fabs(i1 - i2) > lim.agreeTolA) return AGREE;
        return OK;
    }
    // Call every sample (CurrentSense ticks at 10 ms). Returns the latched code.
    uint8_t update(uint32_t ms, float adc1, float adc2, float supplyAdc) {
        float a; now = classify(adc1, adc2, supplyAdc, a); lastA = a;
        for (uint8_t c = RAIL; c <= AGREE; c++) {
            if (now == c) {
                if (!active[c]) { active[c] = true; sinceMs[c] = ms; }
                const uint32_t hold = (c == AGREE) ? lim.agreeHoldMs : lim.holdMs;
                if (ms - sinceMs[c] >= hold && latched == OK) { latched = c; trips++; }
            } else active[c] = false;
        }
        return latched;
    }
    // Key-on self-test, mains open (no current possible). Latches ZERO if the reading is > zeroTolA.
    bool zeroSelfTest(float adc1, float adc2, float supplyAdc) {
        float a; const uint8_t c = classify(adc1, adc2, supplyAdc, a);
        if (c != OK) { if (latched == OK) { latched = c; trips++; } return false; }
        if (std::fabs(a) > lim.zeroTolA) { if (latched == OK) { latched = ZERO; trips++; } return false; }
        return true;
    }
    // Key cycle. Refused while the last sample still shows a live fault.
    bool reset() { if (now != OK) return false; latched = OK; for (auto &b : active) b = false; return true; }
};
}  // namespace LemHealth
