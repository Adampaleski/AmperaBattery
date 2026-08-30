#pragma once

#include <stdint.h>
#include <string.h>

// K112 36S (one 24S KICM + one 12S KICM) → 0x300 / 0x310 bleed bits.
//
// Frame FORMAT (IDs, DLC, bitmask-in-bytes) is from VoltBMSV2 / OEM capture:
//   0x300 DLC=8, 0x310 DLC=5, each byte bits 0–5 = one 6-cell group.
// Empirical 6S pair (swoozle / diyelectriccar): 0x46x → bits 0-2 (0x01,0x02,0x04),
// 0x47x → bits 3-5 (0x08,0x10,0x20) of the SAME byte. Then the next 0x200#02.00.00
// triggers ~130 ms / ~100 mA bleed.
//
// Cell INDEX and BICM addressing are from THIS repo, not VoltBMSV2's
// 4-module 6S/8S CMU loop (modules[y] → buf[y-1] with missing-8/12 hacks).
// That loop would dump the whole 0x460/0x470 24S burst into 0x300[0] as 6 cells.

struct K112BalanceBit {
    uint8_t frame;  // 0 = 0x300, 1 = 0x310
    uint8_t byte;
    uint8_t bit;    // 0-5
};

constexpr uint8_t  kBalance300Len    = 8;
constexpr uint8_t  kBalance310Len    = 5;
constexpr uint32_t kBalanceCanId300  = 0x300;
constexpr uint32_t kBalanceCanId310  = 0x310;

// Pack cell 1..36. Index 0 unused.
constexpr K112BalanceBit kK112_36S_BalanceBit[37] = {
    {0, 0, 0},
    // BICM-A (K112 24S): burst 0x460/0x470 → pack cells 1–24
    // Physical groups 6S + 12S + 6S (docs/K112_24S.md; decode print 1-6 / 7-18 / 19-24).
    // Four 6-cell AFE groups on 0x300[0..3] — first BICM on the chain (0x460).
    {0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {0, 0, 3}, {0, 0, 4}, {0, 0, 5},  //  1–6
    {0, 1, 0}, {0, 1, 1}, {0, 1, 2}, {0, 1, 3}, {0, 1, 4}, {0, 1, 5},  //  7–12
    {0, 2, 0}, {0, 2, 1}, {0, 2, 2}, {0, 2, 3}, {0, 2, 4}, {0, 2, 5},  // 13–18
    {0, 3, 0}, {0, 3, 1}, {0, 3, 2}, {0, 3, 3}, {0, 3, 4}, {0, 3, 5},  // 19–24
    // BICM-B (12S sparse): BicmSparseMap 0x46D/0x47D/0x46E/0x47E → pack 25–36
    // DBC: BO_ 1133/1149/1134/1150 = Cells_85..96 on BMS_Module_4 (6-byte, 3 cells).
    // 0x46D nibble D → CMU 14 → 0x310 byte 3; 0x46E → CMU 15 → 0x310 byte 4
    // (OEM 0x310 DLC=5 holds later CMUs after skipped 8 and 12).
    {1, 3, 0}, {1, 3, 1}, {1, 3, 2}, {1, 3, 3}, {1, 3, 4}, {1, 3, 5},  // 25–30
    {1, 4, 0}, {1, 4, 1}, {1, 4, 2}, {1, 4, 3}, {1, 4, 4}, {1, 4, 5},  // 31–36
};

static_assert(sizeof(kK112_36S_BalanceBit) / sizeof(kK112_36S_BalanceBit[0]) == 37,
              "map covers pack cells 1-36");
static_assert(kK112_36S_BalanceBit[1].frame == 0 && kK112_36S_BalanceBit[1].byte == 0 &&
                  kK112_36S_BalanceBit[1].bit == 0,
              "cell 1 = 0x300[0] bit0 (0x460 group)");
static_assert(kK112_36S_BalanceBit[6].byte == 0 && kK112_36S_BalanceBit[6].bit == 5,
              "cell 6 = 0x300[0] bit5 (0x470 group)");
static_assert(kK112_36S_BalanceBit[7].byte == 1 && kK112_36S_BalanceBit[7].bit == 0,
              "cell 7 starts second 6S group, not CMU-1 leftover");
static_assert(kK112_36S_BalanceBit[24].frame == 0 && kK112_36S_BalanceBit[24].byte == 3 &&
                  kK112_36S_BalanceBit[24].bit == 5,
              "cell 24 is last bit of BICM-A");
static_assert(kK112_36S_BalanceBit[25].frame == 1 && kK112_36S_BalanceBit[25].byte == 3 &&
                  kK112_36S_BalanceBit[25].bit == 0,
              "cell 25 = 0x46D → 0x310[3] bit0");
static_assert(kK112_36S_BalanceBit[28].frame == 1 && kK112_36S_BalanceBit[28].byte == 3 &&
                  kK112_36S_BalanceBit[28].bit == 3,
              "cell 28 = 0x47D → 0x310[3] bit3");
static_assert(kK112_36S_BalanceBit[36].frame == 1 && kK112_36S_BalanceBit[36].byte == 4 &&
                  kK112_36S_BalanceBit[36].bit == 5,
              "cell 36 = 0x47E last bit");

inline void k112ClearBalanceFrames(uint8_t buf300[8], uint8_t buf310[5]) {
    memset(buf300, 0, 8);
    memset(buf310, 0, 5);
}

inline void k112SetBalanceBit(uint8_t cell, uint8_t buf300[8], uint8_t buf310[5]) {
    if (cell < 1 || cell > 36) {
        return;
    }
    const K112BalanceBit &b = kK112_36S_BalanceBit[cell];
    const uint8_t mask = static_cast<uint8_t>(1u << b.bit);
    if (b.frame == 0) {
        if (b.byte < kBalance300Len) {
            buf300[b.byte] = static_cast<uint8_t>(buf300[b.byte] | mask);
        }
    } else if (b.byte < kBalance310Len) {
        buf310[b.byte] = static_cast<uint8_t>(buf310[b.byte] | mask);
    }
}

// If two series-adjacent pack cells are both marked, keep the higher voltage.
inline void k112DropAdjacent(uint8_t *want, const float *cells, int nCells) {
    for (int c = 1; c < nCells; c++) {
        if (!want[c] || !want[c + 1]) {
            continue;
        }
        if (cells[c] >= cells[c + 1]) {
            want[c + 1] = 0;
        } else {
            want[c] = 0;
        }
    }
}

inline void k112PackWanted(const uint8_t *want, int nCells, uint8_t buf300[8],
                           uint8_t buf310[5]) {
    k112ClearBalanceFrames(buf300, buf310);
    const int n = (nCells < 36) ? nCells : 36;
    for (int c = 1; c <= n; c++) {
        if (want[c]) {
            k112SetBalanceBit(static_cast<uint8_t>(c), buf300, buf310);
        }
    }
}

