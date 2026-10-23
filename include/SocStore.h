#pragma once
// SoC count that survives power-off (backlog item 3 "persist used Ah + anchor",
// ties to item 5). Header-only, NOT wired into firmware yet (SocRun still
// boots UNKNOWN). Host-tested by scripts/test_soc_store.cpp.
//
// Layout: right after DriveStore (EEPROM 1469..1514), two ping-pong slots of
// kRecSize bytes. Each slot: magic "SOC1", seq, usedAh, anchorPct, capAh,
// flags (bit0 known, bit1 drift), CRC-16. Load picks the valid slot with the
// highest seq; a torn write only damages the slot being written, so the other
// (previous) count is still there. Blank / bad CRC / out-of-range = nothing
// restored (count stays UNKNOWN until the next full-charge anchor): fail closed.
//
// Wear (all PLACEHOLDER): write on key-off / power-loss edge (force), or when
// the count moved >= kMinStepAh and >= kMinGapMs since the last write, at most
// kMaxPerBoot writes per boot. Identical records are not rewritten. When the
// count goes UNKNOWN (current lost) the store writes known=0 so a stale count
// can never come back after a reboot.
//
// UNVERIFIED: assumes no unmetered Ah while the BMS is off (charger is BMS
// controlled; self-discharge / 12 V DC-DC draw while off not measured).
// Teensy emulated-EEPROM endurance UNVERIFIED (same note as FAULT_LOG.md).
#include <math.h>
#include <string.h>
#include "DriveStore.h"

namespace BmsCore {

struct SocSaved {
    bool  known = false;
    bool  drift = false;
    float usedAh = 0;      // Ah out since the anchor (+ = removed)
    float anchorPct = 100; // where the count was anchored
    float capAh = 0;       // learned bank Ah; 0 = UNKNOWN
};

class SocStore {
public:
    static constexpr int      kBase      = DriveProfile::Store::kEnd;   // 1469
    static constexpr uint32_t kMagic     = 0x31434F53;                  // "SOC1"
    static constexpr int      kRecSize   = 4 + 4 + 4 + 4 + 4 + 1 + 2;   // 23
    static constexpr int      kSlots     = 2;
    static constexpr int      kEnd       = kBase + kRecSize * kSlots;
    static_assert(kEnd <= 4284, "past Teensy 4.1 emulated EEPROM");
    static constexpr float    kMinStepAh = 0.5f;      // PLACEHOLDER
    static constexpr uint32_t kMinGapMs  = 60000;     // PLACEHOLDER
    static constexpr int      kMaxPerBoot = 120;      // PLACEHOLDER
    static constexpr float    kMaxAbsAh  = 2000.0f;   // sanity bound, PLACEHOLDER

    void begin(const LogStore &s) { st_ = s; writes_ = 0; haveLast_ = false; lastMs_ = 0; seq_ = 0; next_ = 0; scan(nullptr); }

    // Restore the newest valid known record. false = nothing usable (stay UNKNOWN).
    bool load(SocSaved &out) { return scan(&out); }

    // Call every loop with the live count. force = key-off / power-loss edge.
    // Returns true if a record was written.
    bool update(uint32_t nowMs, const SocSaved &cur, bool force) {
        if (haveLast_ && same(cur, last_)) return false;
        if (!force && haveLast_) {
            const bool knownChanged = cur.known != last_.known || cur.drift != last_.drift || cur.capAh != last_.capAh;
            const bool moved = fabsf(cur.usedAh - last_.usedAh) >= kMinStepAh || fabsf(cur.anchorPct - last_.anchorPct) > 0.01f;
            if (!knownChanged && !(moved && nowMs - lastMs_ >= kMinGapMs)) return false;
        }
        if (writes_ >= kMaxPerBoot) { limited_ = true; return false; }
        write(cur); lastMs_ = nowMs; return true;
    }
    int  writes() const { return writes_; }
    bool limited() const { return limited_; }

private:
    static bool same(const SocSaved &a, const SocSaved &b) {
        return a.known == b.known && a.drift == b.drift && a.usedAh == b.usedAh && a.anchorPct == b.anchorPct && a.capAh == b.capAh;
    }
    static bool sane(const SocSaved &r) {
        return isfinite(r.usedAh) && isfinite(r.anchorPct) && isfinite(r.capAh) && fabsf(r.usedAh) <= kMaxAbsAh &&
               r.anchorPct >= 0 && r.anchorPct <= 100 && r.capAh >= 0 && r.capAh <= kMaxAbsAh;
    }
    bool readSlot(int i, uint32_t &seq, SocSaved &r) const {
        uint8_t b[kRecSize];
        for (int k = 0; k < kRecSize; k++) b[k] = st_.read(kBase + i * kRecSize + k);
        uint32_t magic; memcpy(&magic, b, 4);
        uint16_t crc; memcpy(&crc, b + kRecSize - 2, 2);
        if (magic != kMagic || crc != Thresholds::crc16pub(b, kRecSize - 2)) return false;
        memcpy(&seq, b + 4, 4); memcpy(&r.usedAh, b + 8, 4); memcpy(&r.anchorPct, b + 12, 4); memcpy(&r.capAh, b + 16, 4);
        r.known = b[20] & 1; r.drift = b[20] & 2;
        return sane(r);
    }
    bool scan(SocSaved *out) {
        bool any = false; uint32_t best = 0; SocSaved br; int bi = -1;
        for (int i = 0; i < kSlots; i++) {
            uint32_t s; SocSaved r;
            if (!readSlot(i, s, r)) continue;
            if (!any || s > best) { any = true; best = s; br = r; bi = i; }
        }
        if (any) { seq_ = best; next_ = (bi + 1) % kSlots; last_ = br; haveLast_ = true; }
        else { seq_ = 0; next_ = 0; }
        if (!out || !any || !br.known) return false;
        *out = br; return true;
    }
    void write(const SocSaved &r) {
        uint8_t b[kRecSize];
        const uint32_t s = seq_ + 1;
        memcpy(b, &kMagic, 4); memcpy(b + 4, &s, 4); memcpy(b + 8, &r.usedAh, 4); memcpy(b + 12, &r.anchorPct, 4);
        memcpy(b + 16, &r.capAh, 4); b[20] = (r.known ? 1 : 0) | (r.drift ? 2 : 0);
        const uint16_t crc = Thresholds::crc16pub(b, kRecSize - 2); memcpy(b + kRecSize - 2, &crc, 2);
        for (int k = 0; k < kRecSize; k++) st_.write(kBase + next_ * kRecSize + k, b[k]);
        seq_ = s; next_ = (next_ + 1) % kSlots; last_ = r; haveLast_ = true; writes_++;
    }
    LogStore st_{nullptr, nullptr};
    uint32_t seq_ = 0, lastMs_ = 0;
    int      next_ = 0, writes_ = 0;
    bool     haveLast_ = false, limited_ = false;
    SocSaved last_;
};

}  // namespace BmsCore
