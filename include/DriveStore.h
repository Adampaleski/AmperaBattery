#pragma once
// EEPROM record for drive modes (chief 2026-10-21): profiles, current mode, Kid PIN hash, CRC-16.
// Boot restores the last mode (Kid stays Kid). Blank/corrupt/out-of-bounds -> PROPOSED defaults, High, no PIN.
#include <string.h>
#include "DriveProfile.h"
#include "Thresholds.h"

namespace DriveProfile {
class Store {
public:
    static constexpr int      kBase  = BmsCore::Thresholds::kEnd;   // 1318 (THR2; was 1310 with THR1: old drive-store saves read as blank)
    static constexpr uint32_t kMagic = 0x44525631;                  // "DRV1"
    static constexpr int      kNF    = 5;
    static constexpr int      kSize  = 4 + 1 + 4 + 4 * kNF * kModes + 2;
    static constexpr int      kEnd   = kBase + kSize;
    static_assert(kEnd <= 4284, "past Teensy 4.1 emulated EEPROM");

    explicit Store(const BmsCore::LogStore &s) : st_(s) {}

    bool load(Table &t, Selector &sel) const {
        uint8_t b[kSize];
        for (int i = 0; i < kSize; i++) b[i] = st_.read(kBase + i);
        uint32_t magic; memcpy(&magic, b, 4);
        uint16_t crc; memcpy(&crc, b + kSize - 2, 2);
        if (magic != kMagic || crc != BmsCore::Thresholds::crc16pub(b, kSize - 2) || b[4] >= kModes) return false;
        Table nt;
        static const char *kF[kNF] = {"expo", "cur", "ramp", "top", "regen"};
        for (int m = 0; m < kModes; m++)
            for (int f = 0; f < kNF; f++) {
                float v; memcpy(&v, b + 9 + 4 * (m * kNF + f), 4);
                if (!nt.set(m, kF[f], v)) return false;
            }
        uint32_t h; memcpy(&h, b + 5, 4);
        if (b[4] == M_KID && !h) return false;
        t = nt; sel.pinHash = h; sel.mode = static_cast<Mode>(b[4]);
        return true;
    }
    void save(const Table &t, const Selector &sel) const {
        uint8_t b[kSize];
        memcpy(b, &kMagic, 4); b[4] = sel.mode; memcpy(b + 5, &sel.pinHash, 4);
        for (int m = 0; m < kModes; m++) {
            const float v[kNF] = {t.p[m].expo, t.p[m].currentPct, t.p[m].rampAps, t.p[m].topMph, t.p[m].regenPct};
            memcpy(b + 9 + 4 * kNF * m, v, sizeof v);
        }
        const uint16_t crc = BmsCore::Thresholds::crc16pub(b, kSize - 2); memcpy(b + kSize - 2, &crc, 2);
        for (int i = 0; i < kSize; i++) st_.write(kBase + i, b[i]);
    }
private:
    BmsCore::LogStore st_;
};
}  // namespace DriveProfile
