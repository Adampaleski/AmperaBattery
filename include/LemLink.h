#pragma once
// LEM B over the inter-Teensy link (item 3 SoC step, 2026-10-23 4 PM). Header-only.
// Teensy B reads LEM B (strings 3+4); Teensy A needs it for the bank-total SoC
// count and the split-LEM checks. This is the frame format + A-side freshness
// rule only. NOT consumed yet: SocRun kHaveLemB and ContactorSeq lemBValid stay
// false until the next step. With CURRENT_SENSE_ENABLE=0 B always sends valid=0.
//
// 0x6A5 B -> A every LINK_PERIOD_MS:
//   [0-1] LEM B amps x10 (int16 LE, 0.1 A, + = discharge)  [2] bit0 valid
//   [3-6] 0  [7] heartbeat (B's link hb)
// A side (fail closed): the reading counts only if valid=1, the heartbeat has
// advanced within staleMs, and |amps| is inside the encodable range. A frozen
// heartbeat, a missing frame, valid=0, or a saturated value = no LEM B.
#include <stdint.h>
#include <string.h>

namespace LemLink {
constexpr float kScale = 10.0f;              // 0.1 A per count
constexpr float kMaxA  = 3276.0f;            // |amps| >= this = saturated -> sent invalid

inline void encode(float amps, bool valid, uint8_t hb, uint8_t *b) {
    memset(b, 0, 8);
    bool ok = valid && amps == amps && amps < kMaxA && amps > -kMaxA;  // NaN / out of range -> invalid
    int32_t c = ok ? static_cast<int32_t>(amps * kScale + (amps >= 0 ? 0.5f : -0.5f)) : 0;
    const uint16_t u = static_cast<uint16_t>(static_cast<int16_t>(c));
    b[0] = u & 0xFF; b[1] = (u >> 8) & 0xFF;
    b[2] = ok ? 1 : 0;
    b[7] = hb;
}

struct Rx {
    bool     seen = false;
    uint8_t  hb = 0;
    uint32_t lastMs = 0;   // last time the heartbeat ADVANCED
    bool     valid = false;
    float    amps = 0;
    uint32_t frames = 0;

    void onFrame(const uint8_t *b, uint32_t nowMs) {
        frames++;
        if (!seen || b[7] != hb) lastMs = nowMs;   // frozen counter does not refresh
        seen = true;
        hb = b[7];
        const int16_t c = static_cast<int16_t>(static_cast<uint16_t>(b[0] | (b[1] << 8)));
        valid = (b[2] & 1) != 0;
        amps = c / kScale;
        if (amps >= kMaxA || amps <= -kMaxA) valid = false;
    }
    bool fresh(uint32_t nowMs, uint32_t staleMs) const { return seen && (nowMs - lastMs) <= staleMs; }
    // true + amps only when B said valid and the frame is fresh. Fail closed otherwise.
    bool get(uint32_t nowMs, uint32_t staleMs, float *out) const {
        if (!fresh(nowMs, staleMs) || !valid) return false;
        if (out) *out = amps;
        return true;
    }
};
}  // namespace LemLink
