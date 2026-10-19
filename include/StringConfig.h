#pragma once
// Dual-string (36S 4P on two Teensy 4.1s) configuration. Build-time only.
//
//   Each Teensy:  CAN3 (30/31) @125k = string 1   CAN1 (22/23) @125k = string 2
//                 CAN2 (0/1)   @250k = Elcon + Kelly + inter-Teensy link
//   TEENSY_ROLE 0 = A (master: owns contactor/precharge state machine, listens to B)
//   TEENSY_ROLE 1 = B (reports its two strings to A every 100 ms; no contactors)
//
// A string is a set of factory circuits on ONE bus. Circuit numbers follow the
// frame-pair order in FactoryCellMap.h (same IDs on every bus):
//    0 M1 c1-6   (6S) 0x460/470    4 M2 c1-8  0x464/474    7 M3 c1-8  0x468/478
//    1 M1 c7-14  (8S) 0x461/471    5 M2 c9-16 0x465/475    8 M3 c9-16 0x469/479
//    2 M1 c15-22 (8S) 0x462/472    6 M2 c17-24 0x466/476   9 M3 c17-24 0x46A/47A
//    3 M1 c23-30 (8S) 0x463/473   10 M4 c1-6  0x46C/47C   11 M4 c7-12 0x46D/47D
//                                 12 M4 c13-18 0x46E/47E
// String cell numbering = expected circuits in ascending circuit order (M1/M2/M3
// before M4). That is DISPLAY order; meter the physical series order at the bench.
//
// Override per env, e.g. string 2 on M3 with the M4 12S on circuits 10+11:
//   -D STRING2_CIRCUITS="(M3_ALL|M4_C1|M4_C2)"
//   M4 6S mode  = one of M4_C1/M4_C2/M4_C3;  12S mode = two of them.

#include <stdint.h>

#ifndef DUAL_STRING
#define DUAL_STRING 0
#endif
#ifndef TEENSY_ROLE
#define TEENSY_ROLE 0
#endif
#define ROLE_IS_A (TEENSY_ROLE == 0)

#define CIRC(i)  (1UL << (i))
#define M1_ALL   (CIRC(0) | CIRC(1) | CIRC(2) | CIRC(3))
#define M2_ALL   (CIRC(4) | CIRC(5) | CIRC(6))
#define M3_ALL   (CIRC(7) | CIRC(8) | CIRC(9))
#define M4_C1    CIRC(10)   // 0x46C/47C bottom 6S
#define M4_C2    CIRC(11)   // 0x46D/47D middle 6S
#define M4_C3    CIRC(12)   // 0x46E/47E top 6S

#define M4_ALL   (M4_C1 | M4_C2 | M4_C3)
// Adam 10/19: strings are 24S + 18S = 42S. One board of each type per bus
// (fixed BICM IDs), so string 1 (CAN3) = M2 24S + M4 18S and string 2 (CAN1)
// = M3 24S + its own M4 18S. Old 36S layout: -D STRING1_CIRCUITS="(M1_ALL|M4_C1)"
// -D STRING2_CIRCUITS="(M2_ALL|M4_C2|M4_C3)" -D STRING_EXPECT_S=36
#ifndef STRING1_CIRCUITS
#define STRING1_CIRCUITS (M2_ALL | M4_ALL)         // 24S + 18S = 42S
#endif
#ifndef STRING2_CIRCUITS
#define STRING2_CIRCUITS (M3_ALL | M4_ALL)         // 24S + 18S = 42S (second M4 on CAN1)
#endif
#ifndef STRING_EXPECT_S
#define STRING_EXPECT_S 42                         // every string must total this (compile check)
#endif
#ifndef STRING_SILENT_MS
#define STRING_SILENT_MS 2000                      // two missed 1 s keep-alives -> FAULT (crew OK 10/17; confirm vs d gapmax)
#endif
#ifndef LINK_STALE_MS
#define LINK_STALE_MS 1000                         // A: B summary older -> FAULT
#endif
#ifndef LINK_PERIOD_MS
#define LINK_PERIOD_MS 100
#endif
#ifndef LINK_TX_ENABLE
#define LINK_TX_ENABLE 1                           // link frames on CAN2 (not charge/balance/contactor)
#endif
#ifndef B_FAULT_PIN_ENABLE
#define B_FAULT_PIN_ENABLE 0                       // role B fault output on pin 4: DISABLED
#endif

// Inter-Teensy link on CAN2 @250k. 29-bit EXTENDED IDs (CAN2 mailboxes run in
// extended mode for Elcon/Kelly). No clash with Elcon 0x1806E5F4 / 0x18FF50E5
// or Kelly 0x0CF11E05 / 0x0CF11F05.
constexpr uint32_t kLinkIdBString1 = 0x6A0;  // B -> A  string 1 summary
constexpr uint32_t kLinkIdBString2 = 0x6A1;  // B -> A  string 2 summary
constexpr uint32_t kLinkIdBStatus  = 0x6A2;  // B -> A  status / cell counts
constexpr uint32_t kLinkIdAStatus  = 0x6A8;  // A -> B  heartbeat + contactor state
// 0x6A3-0x6A7, 0x6A9-0x6AF reserved.
