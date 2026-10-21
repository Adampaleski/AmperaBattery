#pragma once
// Two-Teensy throttle cross-check + hardware clamp model (crew approved 2026-10-21: chief, Sparky).
// Pure C++: firmware (src/ThrottleTrim.cpp), sim scenarios (BmsSim.h), tests. CREW REVIEW. THROTTLE_TRIM_ENABLE=0.
//
// Hardware (docs/THROTTLE_CLAMP.md): Kelly throttle = digipot wiper -> 74HC4066 switch SW1 (ctl = one-shot A)
// -> SW2 (ctl = one-shot B) -> Kelly throttle input, 10k pull-down at the Kelly side. One 74HC123 holds each
// one-shot Q high only while that Teensy keeps toggling its alive line (A: MCP4131 CS, B: pin 7). A Teensy
// "drops its enable" by stopping its toggles. Either one alone forces 0 V.
//
// Cross-check: A sends its commanded trim + mode + profile CRC to B (0x6A9); B computes the same pedal map from
// its own pedal pin and synced profile and answers (0x6A4). Each side: |A - B| > tolPct for holdMs, peer frame
// heartbeat stale > hbMs, or profile CRC mismatch -> stop toggling (enable off) + latched fault (logged).
#include <stdint.h>
#include <string.h>
#include "DriveProfile.h"

namespace ThrottleCross {

constexpr float    kTolPct    = 5.0f;   // PROPOSED
constexpr uint32_t kHoldMs    = 50;     // PROPOSED
constexpr uint32_t kHbMs      = 200;    // same as the link stale limit
constexpr uint32_t kFrameMs   = 20;     // 0x6A9 / 0x6A4 period (PROPOSED)
constexpr uint32_t kOneShotMs = 5;      // 74HC123 pulse ~5 ms (R/C UNVERIFIED, scope it)

enum Fault : uint8_t { TF_NONE = 0, TF_DISAGREE = 1, TF_PEER_LOST = 2, TF_PROFILE = 4, TF_LOCAL = 8 };
inline const char *faultName(uint8_t f) {
    return f & TF_LOCAL ? "THROTTLE-LOCAL" : f & TF_PROFILE ? "THROTTLE-PROFILE-MISMATCH" :
           f & TF_PEER_LOST ? "THROTTLE-PEER-LOST" : f & TF_DISAGREE ? "THROTTLE-DISAGREE" : "";
}

// CRC-16/CCITT over the whole profile table + mode: both sides must match.
inline uint16_t profileCrc(const DriveProfile::Table &t, uint8_t mode) {
    uint16_t c = 0xFFFF;
    auto feed = [&](const uint8_t *p, int n) {
        for (int i = 0; i < n; i++) { c ^= static_cast<uint16_t>(p[i]) << 8;
            for (int b = 0; b < 8; b++) c = (c & 0x8000) ? static_cast<uint16_t>((c << 1) ^ 0x1021) : static_cast<uint16_t>(c << 1); }
    };
    for (int m = 0; m < DriveProfile::kModes; m++) {
        const float v[5] = {t.p[m].expo, t.p[m].currentPct, t.p[m].rampAps, t.p[m].topMph, t.p[m].regenPct};
        feed(reinterpret_cast<const uint8_t *>(v), sizeof v);
    }
    feed(&mode, 1);
    return c;
}

// One side's checker. Latched until clear() (key cycle / serial), like a drive fault.
class Checker {
public:
    uint8_t  latched = TF_NONE;
    bool     armed = false;     // first peer frame seen (boot: enable stays off until then, no fault)
    bool     enable() const { return armed && latched == TF_NONE; }
    void     clear() { latched = TF_NONE; over_ = false; }
    // Feed own output every 1 ms. Peer frames are up to kFrameMs old, so the peer value is compared with the
    // band [min, max] of our own outputs over the last kWinMs (no false trip while ramping).
    static constexpr int kWinMs = 32;
    uint8_t step(uint32_t now, float own, float peer, bool peerSeen, uint32_t peerMs, bool crcMatch, bool localOk) {
        hist_[now % kWinMs] = own; if (fill_ < kWinMs) fill_++;
        uint8_t f = 0;
        if (!localOk) f |= TF_LOCAL;
        // arm only once the peer is heard AND the profile copies match (B syncs from A after boot: no fault meanwhile)
        if (peerSeen && !armed && now - peerMs <= kHbMs && crcMatch) armed = true;
        if (armed) {
            if (now - peerMs > kHbMs) f |= TF_PEER_LOST;
            else {
                if (!crcMatch) { if (!crcBad_) { crcBad_ = true; crcSince_ = now; } if (now - crcSince_ >= kHoldMs) f |= TF_PROFILE; }
                else crcBad_ = false;   // mode/profile sync takes <= 2 frames: debounced like the trim
                float lo = own, hi = own;
                for (int i = 0; i < fill_; i++) { if (hist_[i] < lo) lo = hist_[i]; if (hist_[i] > hi) hi = hist_[i]; }
                const bool bad = peer < lo - kTolPct || peer > hi + kTolPct;
                if (bad) { if (!over_) { over_ = true; overSince_ = now; } if (now - overSince_ >= kHoldMs) f |= TF_DISAGREE; }
                else over_ = false;
            }
        }
        latched |= f;
        return f;
    }
private:
    bool over_ = false; uint32_t overSince_ = 0;
    bool crcBad_ = false; uint32_t crcSince_ = 0;
    float hist_[kWinMs] = {}; int fill_ = 0;
};

// Retriggerable one-shot (74HC123): Q high while toggles arrive within kOneShotMs.
struct OneShot {
    uint32_t lastEdgeMs = 0; bool seen = false;
    void edge(uint32_t now) { lastEdgeMs = now; seen = true; }
    bool q(uint32_t now) const { return seen && now - lastEdgeMs <= kOneShotMs; }
};

// ---- plant for sim scenarios / tests (1 ms steps) ----
struct Plant {
    DriveProfile::Table tabA, tabB;
    DriveProfile::Trim  trimA, trimB;
    Checker  chkA, chkB;
    OneShot  osA, osB;
    uint8_t  mode = DriveProfile::M_H;    // A's mode (driver selects on A)
    uint8_t  modeB = DriveProfile::M_H;   // B follows A's mode from 0x6A9
    float    pedal = 50, mph = 10;
    float    pedalBOffset = 0;      // B's own pedal pin reads this much off (wiring/ADC fault)
    bool     hungA = false, hungB = false, faultA = false;
    float    potStuckPct = -1;     // >= 0: MCP4131 wiper stuck (output = pedal x this %)
    uint32_t now = 0, lastAtoB = 0, lastBtoA = 0, aSeenAtB = 0, bSeenAtA = 0;
    float    aSent = 0, bSent = 0, outA = 0, outB = 0;
    uint16_t crcASent = 0, crcBSent = 0;
    bool     aSeenB = false, bSeenA = false;
    float    kellyPct = 0;          // what the Kelly throttle input sees (% of full pedal)

    void step() {
        now++;
        const DriveProfile::Profile &pa = tabA.p[mode], &pb = tabB.p[modeB];
        DriveProfile::In in; in.pedalPct = pedal; in.mph = mph; in.speedValid = true; in.allowPct = 100;
        if (!hungA) { in.ok = !faultA; outA = trimA.step(in, pa, 1); }
        if (!hungB) { in.ok = true; in.pedalPct = pedal + pedalBOffset; outB = trimB.step(in, pb, 1); in.pedalPct = pedal; }
        // link frames every kFrameMs from a live side
        if (!hungA && now - lastAtoB >= kFrameMs) { lastAtoB = now; aSent = outA; crcASent = profileCrc(tabA, mode); aSeenAtB = now; aSeenB = true; if (!hungB) modeB = mode; }
        if (!hungB && now - lastBtoA >= kFrameMs) { lastBtoA = now; bSent = outB; crcBSent = profileCrc(tabB, modeB); bSeenAtA = now; bSeenA = true; }
        if (!hungA) chkA.step(now, outA, bSent, bSeenA, bSeenAtA, crcBSent == profileCrc(tabA, mode), !faultA);
        if (!hungB) chkB.step(now, outB, aSent, aSeenB, aSeenAtB, crcASent == profileCrc(tabB, modeB), true);
        // alive toggles: only from a running side whose checker still enables (A: CS edges only while ok)
        if (!hungA && chkA.enable()) osA.edge(now);
        if (!hungB && chkB.enable()) osB.edge(now);
        const float pot = potStuckPct >= 0 ? pedal * potStuckPct / 100.0f
                                           : pedal * DriveProfile::Trim::wiper(outA, pedal) / 128.0f;
        kellyPct = (osA.q(now) && osB.q(now)) ? pot : 0;
    }
    void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i++) step(); }
};

}  // namespace ThrottleCross
