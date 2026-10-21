#pragma once
// Drive-mode pedal-map profiles + reduce-only throttle trim (Adam approved 2026-10-21).
// Pure C++ (no Arduino): used by src/ThrottleTrim.cpp, the sim (wasm) and scripts/test_drive_profile.cpp.
// EVERY default here is PROPOSED (not a Kelly setting, not road-tested). CREW REVIEW before any flash.
//
// The trim sits in the throttle wiper through an MCP4131 used as a divider: Kelly sees pedal x k,
// k = wiper/128 in 0..1. So it can ONLY reduce the pedal, never add:
//  - "strong low end" (Mud/Sand) = no softening (linear, pass-through); real boost needs the Kelly
//    throttle curve, not this layer.
//  - "current limit" = pedal cap as % of full pedal (torque request), a proxy; the 550 A battery
//    limit stays the Kelly's own.
//  - "ramp" A/s is converted to pedal %/s against battA (550 A): rise rate-limited, release instant.
//  - "top speed" needs Kelly RPM (BMS_CAP_KELLY_RX). No valid speed: pedal passes the curve/cap only,
//    except Kid, which outputs 0 without a valid speed.
//  - "regen strength" scales the Brake_AN regen request: sim only, firmware does not drive regen yet.
#include <stdint.h>
#include <string.h>

namespace DriveProfile {

enum Mode : uint8_t { M_H = 0, M_M, M_L, M_SNOW, M_MUD, M_SAND, M_KID, kModes };

struct Profile {
    const char *key;
    const char *name;
    float expo;        // 0 = linear .. 1 = softest (y = (1-e)x + e x^3); < 0 clipped to 0 (reduce-only)
    float currentPct;  // pedal cap, % of full pedal (proxy for current limit)
    float rampAps;     // A/s rise rate (pedal %/s = rampAps / battA x 100)
    float topMph;      // output tapers to 0 over the last kTaperMph
    float regenPct;    // % of max regen (sim)
};

// [key, name, expo, current %, ramp A/s, top mph, regen %] -- all PROPOSED
constexpr Profile kDefaults[kModes] = {
    {"H",    "High",     0.00f, 100.0f, 2000.0f, 49.0f, 100.0f},
    {"M",    "Medium",   0.30f,  70.0f, 1000.0f, 36.0f,  80.0f},
    {"L",    "Low",      0.50f,  45.0f,  600.0f, 22.0f,  60.0f},
    {"SNOW", "Snow",     0.80f,  40.0f,  150.0f, 25.0f,  25.0f},
    {"MUD",  "Mud",      0.00f,  90.0f, 2500.0f, 30.0f,  20.0f},
    {"SAND", "Sand",     0.00f, 100.0f, 3000.0f, 35.0f,  15.0f},
    {"KID",  "Kid",      0.70f,  20.0f,   60.0f, 10.0f,  30.0f},
};
// Adjustable bounds (field: lo, hi)
struct Bound { float lo, hi; };
constexpr Bound kExpo{0.0f, 1.0f}, kCur{0.0f, 100.0f}, kRamp{10.0f, 5000.0f}, kTop{1.0f, 60.0f}, kRegen{0.0f, 100.0f};
constexpr float kTaperMph = 2.0f;
constexpr float kBattA    = 550.0f;   // SOURCED: Adam (Kelly battery limit)
constexpr float kFilterMs = 5.0f;     // pedal filter time constant (Adam: ~5 ms)
constexpr uint32_t kPeriodUs = 1000;  // 1 kHz

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float curve(float x, float expo) {
    x = clampf(x, 0, 1);
    const float e = clampf(expo, 0, 1);
    return (1 - e) * x + e * x * x * x;   // y <= x for x in 0..1
}

class Table {
public:
    Profile p[kModes];
    Table() { reset(); }
    void reset() { for (int i = 0; i < kModes; i++) p[i] = kDefaults[i]; }
    static int find(const char *key) {
        for (int i = 0; i < kModes; i++) if (!strcmp(kDefaults[i].key, key)) return i;
        return -1;
    }
    // field: expo|cur|ramp|top|regen. Returns false if out of bounds / unknown.
    bool set(int m, const char *field, float v) {
        if (m < 0 || m >= kModes) return false;
        Profile &q = p[m];
        auto in = [&](const Bound &b) { return v >= b.lo && v <= b.hi; };
        if (!strcmp(field, "expo") && in(kExpo)) q.expo = v;
        else if (!strcmp(field, "cur") && in(kCur)) q.currentPct = v;
        else if (!strcmp(field, "ramp") && in(kRamp)) q.rampAps = v;
        else if (!strcmp(field, "top") && in(kTop)) q.topMph = v;
        else if (!strcmp(field, "regen") && in(kRegen)) q.regenPct = v;
        else return false;
        return true;
    }
};

// Mode selection with the Kid PIN. PIN 0 = not set: Kid cannot be entered (so it can always be left).
class Selector {
public:
    Mode mode = M_H;
    uint16_t pin = 0;   // PROPOSED: set by the owner (4 digits); RAM only, reboot -> High, no PIN
    enum Result : uint8_t { OK = 0, NEED_PIN, BAD_PIN, NO_PIN_SET, BAD_MODE };
    Result select(int m, uint16_t enteredPin = 0) {
        if (m < 0 || m >= kModes) return BAD_MODE;
        if (m == M_KID && mode != M_KID && pin == 0) return NO_PIN_SET;
        if (mode == M_KID && m != M_KID) {
            if (!enteredPin) return NEED_PIN;
            if (enteredPin != pin) return BAD_PIN;
        }
        mode = static_cast<Mode>(m);
        return OK;
    }
};

struct In {
    float pedalPct = 0;     // raw pedal 0..100 (ADC)
    float mph = 0;
    bool  speedValid = false;
    float allowPct = 100;   // BMS kellyPct (derates)
    bool  ok = false;       // BMS closed, no drive fault, trim healthy. false -> output 0
};

class Trim {
public:
    float filt = 0, out = 0;
    void reset() { filt = 0; out = 0; }
    // dtMs ~1 at 1 kHz. Returns output pedal %, always <= raw pedal and <= filtered pedal.
    float step(const In &in, const Profile &p, float dtMs) {
        const float raw = clampf(in.pedalPct, 0, 100);
        if (!in.ok) { out = 0; filt = raw; return 0; }
        const float a = dtMs / (kFilterMs + dtMs);
        filt += a * (raw - filt);
        float tgt = curve(filt / 100.0f, p.expo) * 100.0f;
        tgt *= clampf(p.currentPct, 0, 100) / 100.0f;
        tgt *= clampf(in.allowPct, 0, 100) / 100.0f;
        if (in.speedValid) {
            const float over = in.mph - (p.topMph - kTaperMph);
            if (over > 0) tgt *= clampf(1.0f - over / kTaperMph, 0, 1);
        } else if (p.key[0] == 'K') {
            tgt = 0;   // Kid: no speed, no drive
        }
        const float rise = p.rampAps / kBattA * 100.0f * dtMs / 1000.0f;
        out = tgt > out ? (out + rise < tgt ? out + rise : tgt) : tgt;
        if (out > filt) out = filt;
        if (out > raw) out = raw;
        if (out < 0) out = 0;
        return out;
    }
    // MCP4131 wiper 0..128 so that pedal x wiper/128 <= out (floor).
    static uint8_t wiper(float outPct, float pedalPct) {
        if (pedalPct <= 0.01f || outPct <= 0) return 0;
        const float k = clampf(outPct / pedalPct, 0, 1);
        return static_cast<uint8_t>(k * 128.0f);   // truncation = never above
    }
};

// Motor RPM -> top-speed check helper (sim/tests): mph at the trim's cutoff = topMph.
}  // namespace DriveProfile
