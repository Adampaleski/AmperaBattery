#pragma once

// Factory-bus channel map: up to four Volt BICMs on ONE CAN3 bus (125k),
// addressed by their factory ID straps (tomdebree/AmperaBattery VoltBMS.ino).
// Used only by env teensy41_factory_bus (FACTORY_BUS_DECODE=1).
//
// Factory layout (channel = BICM-relative sense channel, 1-based):
//   module 1  30 ch  0x460 ch1-3   0x470 ch4-6   0x461 ch7-10  0x471 ch11-14
//                    0x462 ch15-18 0x472 ch19-22 0x463 ch23-26 0x473 ch27-30
//   module 2  24 ch  0x464 ch1-4   0x474 ch5-8   0x465 ch9-12  0x475 ch13-16
//                    0x466 ch17-20 0x476 ch21-24
//   module 3  24 ch  0x468 ch1-4   0x478 ch5-8   0x469 ch9-12  0x479 ch13-16
//                    0x46A ch17-20 0x47A ch21-24
//   module 4  18 ch  0x46C ch1-3   0x47C ch4-6   0x46D ch7-9   0x47D ch10-12
//                    0x46E ch13-15 0x47E ch16-18
//
// ---------------------------------------------------------------- HOW TO EDIT
// Each kFactoryMapMn[] lists, per channel in order (ch1 first), the PACK cell
// number that channel measures.  0 = channel NOT WIRED (reported as not-present:
// '-' on serial, null in JSON; never 0 V, never a fault).
//   * Every pack cell 1..FACTORY_PACK_CELLS must appear exactly once (checked at
//     compile time).  PACK_S_CELLS in platformio.ini must equal FACTORY_PACK_CELLS.
//   * FACTORY_SERIES_STRING = 1 only if ALL mapped cells are one series string
//     (then a pack total is shown).  0 = per-module voltages only, no total.
//   * Unplug unused sense groups as whole connectors (6/12-cell group), not taps.
//   * Meter every tap against this table before plugging a BICM in.
//
// DEFAULT (bench discovery map, Oct 17 2026): EVERY channel on every board is
// mapped so any circuit that sends frames is decoded and shown.
//   M1 30S -> cells  1-30   M2 24S -> cells 31-54
//   M3 24S -> cells 55-78   M4 18S -> cells 79-96
// Pack-cell numbers here are display slots only: the boards are SEPARATE
// strings (FACTORY_SERIES_STRING 0), so there is no pack total and charging
// stays refused. A circuit that never sent a frame is shown as 'empty' (not
// an alarm); one that sent frames and then stopped is an alarm.
// Example 36S (30 + 6 on module 4's 6-cell group, per Sparky): set
//   FACTORY_PACK_CELLS 36, FACTORY_SERIES_STRING 1, and in kFactoryMapM4 put
//   31..36 on whichever six channels the 6-cell group lands on (bench-verify!).
// -----------------------------------------------------------------------------

#include <stdint.h>

#define FACTORY_PACK_CELLS    96
#define FACTORY_SERIES_STRING 0   // bench: M1 30S and M2 24S are SEPARATE sections, never paralleled

// Cells in ONE series string (the charger voltage basis). 0 = no single
// series string is defined -> charging is impossible (always refused).
// Only meaningful with FACTORY_SERIES_STRING 1 (then it must equal
// FACTORY_PACK_CELLS, e.g. 36).
#if FACTORY_SERIES_STRING
#define FACTORY_STRING_CELLS FACTORY_PACK_CELLS
#else
#define FACTORY_STRING_CELLS 0
#endif

// No bench-verified 0x300/0x310 bleed-bit map exists for factory-strapped
// modules. Balance stays OFF on this build until one is verified.
#define FACTORY_BALANCE_MAP_VERIFIED 0

constexpr uint8_t kFactoryMapM1[30] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

constexpr uint8_t kFactoryMapM2[24] = {
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
};

constexpr uint8_t kFactoryMapM3[24] = {
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
};

constexpr uint8_t kFactoryMapM4[18] = {
    79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 90, 91, 92, 93, 94, 95, 96,
};

// Example: M2 used as ONE 12S group (Adam: module 2 block is 12S). Which 12
// channels are live depends on which sense connector is plugged -- confirm
// with `d` first. If it is ch1-12 (frames 0x464/0x474/0x465):
//   constexpr uint8_t kFactoryMapM2[24] = {
//       31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
//        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
//   };
// (and set FACTORY_PACK_CELLS to 42 if M1 keeps all 30.)

// Expected LIVE channels per module, for the report-only LOW-LIVE diagnostic.
// -1 (default) = mapped channels in circuits that have sent at least one frame
// (empty circuits are not expected). Override to force a count.
// Override when a module is plugged but intentionally not mapped, e.g.
//   -D FACTORY_EXPECT_LIVE_M2=12
#ifndef FACTORY_EXPECT_LIVE_M1
#define FACTORY_EXPECT_LIVE_M1 -1
#endif
#ifndef FACTORY_EXPECT_LIVE_M2
#define FACTORY_EXPECT_LIVE_M2 -1
#endif
#ifndef FACTORY_EXPECT_LIVE_M3
#define FACTORY_EXPECT_LIVE_M3 -1
#endif
#ifndef FACTORY_EXPECT_LIVE_M4
#define FACTORY_EXPECT_LIVE_M4 -1
#endif

// ---- do not edit below: frame layout from Tom's candecode() ----------------

constexpr int kFactoryModules   = 4;
constexpr int kFactoryMaxCh     = 30;

struct FactoryFrameDef {
    uint16_t id;
    uint8_t  module;     // 1..4
    uint8_t  firstCh;    // 1-based channel of byte pair 0/1
    uint8_t  count;      // 3 or 4 cells in this frame
};

constexpr FactoryFrameDef kFactoryFrames[] = {
    {0x460, 1,  1, 3}, {0x470, 1,  4, 3}, {0x461, 1,  7, 4}, {0x471, 1, 11, 4},
    {0x462, 1, 15, 4}, {0x472, 1, 19, 4}, {0x463, 1, 23, 4}, {0x473, 1, 27, 4},
    {0x464, 2,  1, 4}, {0x474, 2,  5, 4}, {0x465, 2,  9, 4}, {0x475, 2, 13, 4},
    {0x466, 2, 17, 4}, {0x476, 2, 21, 4},
    {0x468, 3,  1, 4}, {0x478, 3,  5, 4}, {0x469, 3,  9, 4}, {0x479, 3, 13, 4},
    {0x46A, 3, 17, 4}, {0x47A, 3, 21, 4},
    {0x46C, 4,  1, 3}, {0x47C, 4,  4, 3}, {0x46D, 4,  7, 3}, {0x47D, 4, 10, 3},
    {0x46E, 4, 13, 3}, {0x47E, 4, 16, 3},
};
constexpr int kFactoryFrameCount = sizeof(kFactoryFrames) / sizeof(kFactoryFrames[0]);

constexpr uint8_t kFactoryChannels[kFactoryModules + 1] = {0, 30, 24, 24, 18};
constexpr uint8_t kFactorySeries[kFactoryModules + 1]   = {0, 30, 24, 24, 18};  // label 'M4 18S'

// Isolated sense circuits (one L9763 each). A circuit = one 0x46x/0x47x frame
// pair = kFactoryFrames[2*i] and [2*i+1]. 6-cell circuits are 3+3, 8-cell 4+4.
//   M1 30S 6/8/8/8   M2 24S 8/8/8   M3 24S 8/8/8   M4 18S 6/6/6
constexpr int kFactoryCircuitCount = kFactoryFrameCount / 2;  // 13
static_assert(kFactoryFrameCount % 2 == 0, "frames must come in 0x46x/0x47x pairs");

constexpr const uint8_t *kFactoryMaps[kFactoryModules + 1] = {
    nullptr, kFactoryMapM1, kFactoryMapM2, kFactoryMapM3, kFactoryMapM4,
};

constexpr int kFactoryExpectOverride[kFactoryModules + 1] = {
    0, FACTORY_EXPECT_LIVE_M1, FACTORY_EXPECT_LIVE_M2, FACTORY_EXPECT_LIVE_M3, FACTORY_EXPECT_LIVE_M4,
};

constexpr int factoryMappedCount(int m) {
    int n = 0;
    for (int c = 0; c < kFactoryChannels[m]; c++)
        if (kFactoryMaps[m][c]) n++;
    return n;
}

constexpr int factoryExpectedLive(int m) {
    return kFactoryExpectOverride[m] >= 0 ? kFactoryExpectOverride[m] : factoryMappedCount(m);
}

namespace factory_map_check {
constexpr int countCell(int cell) {
    int n = 0;
    for (int m = 1; m <= kFactoryModules; m++)
        for (int c = 0; c < kFactoryChannels[m]; c++)
            if (kFactoryMaps[m][c] == cell) n++;
    return n;
}
constexpr bool allOnce() {
    for (int cell = 1; cell <= FACTORY_PACK_CELLS; cell++)
        if (countCell(cell) != 1) return false;
    return true;
}
constexpr bool noneTooBig() {
    for (int m = 1; m <= kFactoryModules; m++)
        for (int c = 0; c < kFactoryChannels[m]; c++)
            if (kFactoryMaps[m][c] > FACTORY_PACK_CELLS) return false;
    return true;
}
}  // namespace factory_map_check

static_assert(factory_map_check::allOnce(),
              "FactoryCellMap: every pack cell 1..FACTORY_PACK_CELLS must map exactly once");
static_assert(FACTORY_EXPECT_LIVE_M1 <= 30 && FACTORY_EXPECT_LIVE_M2 <= 24 &&
                  FACTORY_EXPECT_LIVE_M3 <= 24 && FACTORY_EXPECT_LIVE_M4 <= 18,
              "FACTORY_EXPECT_LIVE_Mn exceeds that module's channel count");
static_assert(factory_map_check::noneTooBig(),
              "FactoryCellMap: a channel maps to a cell > FACTORY_PACK_CELLS");
#if defined(PACK_S_CELLS)
static_assert(PACK_S_CELLS == FACTORY_PACK_CELLS,
              "PACK_S_CELLS (platformio.ini) must equal FACTORY_PACK_CELLS");
#endif
