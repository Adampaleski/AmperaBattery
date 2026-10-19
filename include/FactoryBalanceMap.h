#pragma once
// Factory-strapped Volt/Ampera gen1 BICM bleed (passive balance) frames.
// PURE C++ (no Arduino) so scripts/test_factory_balance.cpp can run it on a PC.
//
// SOURCE (frame IDs, DLCs, byte/bit layout):
//   Tom de Bree, github.com/Tom-evnut/AmperaBattery @ f029e07 (MIT)
//     VoltBMSV2/BMSModuleManager.cpp  78-79   CMU = (id & 0x00F) + 1
//     VoltBMSV2/BMSModuleManager.cpp 108-153  0x300 DLC 8: buf[CMU-1], CMU 9 -> buf[7]
//                                             ("hack for missing module #8", 0x467)
//     VoltBMSV2/BMSModuleManager.cpp 155-198  0x310 DLC 5: CMU 10,11 -> buf[0],[1];
//                                             CMU 13-15 -> buf[2..4] (missing #12, 0x46B)
//     ...cpp 119-124 / 164-169                bit (i-1) = cell i of that CMU
//     VoltBMSV2/BMSModule.cpp 47-98           cell i: 1-4 from 0x46x, 5-8 from 0x47x
//                                             (6-cell CMUs: 1-3 / 4-6)
//     VoltBMSV2/VoltBMSV2.ino 2726-2733       trigger 0x200 DLC 3 = 02 00 00
//   swoozle, diyelectriccar.com "1st Gen Chevy Volt BMS Balance Commands Found"
//     #4  0x300#01.00.. + 0x200#020000 bleeds the most-positive cell ~130 ms/~100 mA;
//         0x460 cells 01 02 04 / 0x470 08 10 20; 0x462 01..08 / 0x472 10..80
//     #26 0x461/0x471 rows in the table were a typo: bit order is uniform pack-wide
//     #10 queuing ALL cells of a module -> nothing bleeds; every-other cell works
//   BogdanT #29: full factory pack balanced to within 10 mV with SimpBMS code.
//
// 8-cell circuits use bits 0-7 (cells 1-4 = 0x46x bits 0-3, cells 5-8 = 0x47x
// bits 4-7). 6-cell circuits use bits 0-5. (The old K112 header's "bits 0-5
// per byte" holds ONLY for 6-cell circuits.)
//
// NOT bench-verified on Adam's boards: FACTORY_BALANCE_MAP_VERIFIED stays 0.
//
// BUS CONSTRAINT: IDs are fixed by the BICM straps, so one bus can carry at most
// ONE M1, ONE M2, ONE M3 and ONE M4. A second M4 18S (or M2/M3) needs its own
// bus (CAN3 / CAN1, or the other Teensy). 0x300/0x310 are per-bus frames.
//
// Circuit index i = kFactoryFrames[2i]/[2i+1] in FactoryCellMap.h (StringConfig.h
// CIRC(i)):
//   i  board   ch      IDs          cells  frame  byte
//   0  M1 30S  1-6     0x460/0x470  6      0x300  0
//   1  M1 30S  7-14    0x461/0x471  8      0x300  1
//   2  M1 30S  15-22   0x462/0x472  8      0x300  2
//   3  M1 30S  23-30   0x463/0x473  8      0x300  3
//   4  M2 24S  1-8     0x464/0x474  8      0x300  4
//   5  M2 24S  9-16    0x465/0x475  8      0x300  5
//   6  M2 24S  17-24   0x466/0x476  8      0x300  6
//   7  M3 24S  1-8     0x468/0x478  8      0x300  7
//   8  M3 24S  9-16    0x469/0x479  8      0x310  0
//   9  M3 24S  17-24   0x46A/0x47A  8      0x310  1
//  10  M4 18S  1-6     0x46C/0x47C  6      0x310  2
//  11  M4 18S  7-12    0x46D/0x47D  6      0x310  3
//  12  M4 18S  13-18   0x46E/0x47E  6      0x310  4

#include <stdint.h>

namespace FactoryBalance {

constexpr int      kCircuits    = 13;
constexpr uint32_t kId300       = 0x300;
constexpr uint32_t kId310       = 0x310;
constexpr uint32_t kIdTrigger   = 0x200;
constexpr uint8_t  kLen300      = 8;
constexpr uint8_t  kLen310      = 5;
constexpr uint8_t  kTrigger[3]  = {0x02, 0x00, 0x00};
constexpr uint32_t kCycleMs     = 200;  // OEM BECM resend period (swoozle #1); Tom used 1000
constexpr uint8_t  kSampleEvery = 5;    // every 5th cycle blank 0x300/0x310 = clean read (BogdanT #29)

struct CircuitDef {
    uint16_t idA;     // 0x46x
    uint8_t  module;  // 1..4
    uint8_t  firstCh; // board channel of bit 0
    uint8_t  cells;   // 6 or 8
    uint8_t  frame;   // 0 = 0x300, 1 = 0x310
    uint8_t  byte;
};

constexpr CircuitDef kCircuit[kCircuits] = {
    {0x460, 1,  1, 6, 0, 0}, {0x461, 1,  7, 8, 0, 1}, {0x462, 1, 15, 8, 0, 2}, {0x463, 1, 23, 8, 0, 3},
    {0x464, 2,  1, 8, 0, 4}, {0x465, 2,  9, 8, 0, 5}, {0x466, 2, 17, 8, 0, 6},
    {0x468, 3,  1, 8, 0, 7}, {0x469, 3,  9, 8, 1, 0}, {0x46A, 3, 17, 8, 1, 1},
    {0x46C, 4,  1, 6, 1, 2}, {0x46D, 4,  7, 6, 1, 3}, {0x46E, 4, 13, 6, 1, 4},
};

// Tom's CMU number for a circuit: (id & 0xF) + 1.
constexpr int tomCmu(int i) { return (kCircuit[i].idA & 0xF) + 1; }
// Tom's byte placement, written exactly as his two loops do it.
constexpr int tomFrame(int cmu) { return cmu <= 9 ? 0 : 1; }
constexpr int tomByte(int cmu)  { return cmu <= 9 ? (cmu == 9 ? cmu - 2 : cmu - 1)
                                                  : (cmu > 11 ? cmu - 11 : cmu - 10); }
constexpr bool tableMatchesTom() {
    for (int i = 0; i < kCircuits; i++) {
        const int c = tomCmu(i);
        if (tomFrame(c) != kCircuit[i].frame || tomByte(c) != kCircuit[i].byte) return false;
        if (c == 8 || c == 12) return false;  // 0x467 / 0x46B do not exist
    }
    return true;
}
static_assert(tableMatchesTom(), "circuit -> 0x300/0x310 byte must match Tom's balanceCells()");
static_assert(kCircuit[0].byte == 0 && kCircuit[7].frame == 0 && kCircuit[7].byte == 7, "0x468 -> 0x300[7]");
static_assert(kCircuit[8].frame == 1 && kCircuit[8].byte == 0, "0x469 -> 0x310[0]");
static_assert(kCircuit[12].frame == 1 && kCircuit[12].byte == 4, "0x46E -> 0x310[4]");

constexpr uint8_t cellMask(int i) { return static_cast<uint8_t>((1u << kCircuit[i].cells) - 1u); }

inline void clearFrames(uint8_t b300[kLen300], uint8_t b310[kLen310]) {
    for (int k = 0; k < kLen300; k++) b300[k] = 0;
    for (int k = 0; k < kLen310; k++) b310[k] = 0;
}

// bits[i] bit n = bleed circuit i cell (n+1). Bits above the circuit's cell
// count are dropped.
inline void encode(const uint8_t bits[kCircuits], uint8_t b300[kLen300], uint8_t b310[kLen310]) {
    clearFrames(b300, b310);
    for (int i = 0; i < kCircuits; i++) {
        const uint8_t v = bits[i] & cellMask(i);
        if (kCircuit[i].frame == 0) b300[kCircuit[i].byte] |= v;
        else                        b310[kCircuit[i].byte] |= v;
    }
}

inline int popcount8(uint8_t v) { int n = 0; while (v) { n += v & 1u; v >>= 1; } return n; }

// Safety check on finished frames. Returns 0 if OK, else a reason code:
//  1 = adjacent cells set in one circuit  2 = every cell of a circuit set
//  3 = bit set beyond the circuit's cell count
// (Circuits are even-sized and start on odd board channels, so "no adjacent
// bits in a byte" also means no adjacent board channels across circuits when
// the parity planner below is used.)
inline int framesUnsafe(const uint8_t b300[kLen300], const uint8_t b310[kLen310]) {
    for (int i = 0; i < kCircuits; i++) {
        const uint8_t v = kCircuit[i].frame == 0 ? b300[kCircuit[i].byte] : b310[kCircuit[i].byte];
        if (v & static_cast<uint8_t>(~cellMask(i))) return 3;
        if (v & (v >> 1)) return 1;
        if (v && v == cellMask(i)) return 2;
    }
    return 0;
}

}  // namespace FactoryBalance
