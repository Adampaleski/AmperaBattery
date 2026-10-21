#pragma once
// Per-string monitor for the dual-string build (StringConfig.h). Decodes the
// factory frames of each bus independently, checks the expected-circuit list,
// and produces per-string totals / min / max / spread / faults.
#include <Arduino.h>
#include <FlexCAN_T4.h>

namespace StringMon {
constexpr int kStrings = 2;

// Fault bits (also carried over the link, byte 6 of 0x6A0/0x6A1).
constexpr uint8_t kFSilent   = 0x01;  // expected circuit silent > STRING_SILENT_MS
constexpr uint8_t kFNoData   = 0x02;  // expected circuit never seen since boot
constexpr uint8_t kFCellBad  = 0x04;  // a cell outside 0.5-4.3 V (bad sense / decode)
constexpr uint8_t kFLowCut   = 0x08;  // a cell below low cutoff (L setting)
constexpr uint8_t kFOverMax  = 0x10;  // a cell above kCellFaultMaxV (4.25 V)
constexpr uint8_t kFLinkStale= 0x20;  // (A only) B's summary stale > LINK_STALE_MS
constexpr uint8_t kFCount    = 0x40;  // reporting cell count != expected
constexpr uint8_t kFExtra    = 0x80;  // INFO only: unexpected circuit reporting (not a fault)
constexpr uint8_t kFaultMask = 0x7F;

struct Summary {
    float   v = 0, minV = 0, maxV = 0;  // valid only when cells == expected
    uint8_t cells = 0, expected = 0;
    uint8_t faults = 0;                 // bits above
    // BICM temps from 0x7E0-0x7EF on this bus. Decode = Tom bytes 6-7 BE x -0.0324 + 150 C
    // (BmsCore::bicmTempC). UNVERIFIED on our boards; thermistor -> cell map UNVERIFIED.
    // Only acted on when TEMPS_VERIFIED=1 (stays 0).
    bool    tFresh = false;             // >= 1 plausible temp frame within STRING_SILENT_MS
    float   tMinC = 0, tMaxC = 0;
    uint8_t tHot = 0xFF;                // hottest thermistor index (0x7E0 + n), 0xFF none
    uint8_t tN = 0;                     // fresh thermistor frames
};

void begin();
void onFrame(uint8_t s, const CAN_message_t &msg);  // s 0 = CAN3 string1, 1 = CAN1 string2
void tick();
Summary summary(int s);
float cellV(int s, int idx);  // idx 0-based in string order, 0 if none
void printFaults(uint8_t bits);
void print();               // part of 'd'
void printBalancePlan();    // 'b' (PRINT-ONLY, never transmits)
}  // namespace StringMon
