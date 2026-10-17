#pragma once
// Inter-Teensy link on CAN2 @250k (29-bit extended IDs, StringConfig.h).
//   B -> A every LINK_PERIOD_MS (100 ms):
//     0x6A0 string 1, 0x6A1 string 2:  [0-1] V x100 (u16 LE, 0.01 V)
//        [2-3] min cell mV (u16 LE)  [4-5] max cell mV (u16 LE)
//        [6] fault bits (StringMon::kF*)  [7] heartbeat counter (wraps)
//     0x6A2 status: [0] hb  [1] role (1=B)  [2] s1 cells  [3] s2 cells
//        [4] s1|s2 fault bits  [5] bit0 = B fault pin enabled  [6-7] 0
//   A -> B every 100 ms:
//     0x6A8: [0] hb  [1] contactor state (ContactorSeq::State)  [2] A fault bits (s1|s2)
// A: a B string frame whose heartbeat has not advanced for > LINK_STALE_MS
//    => that remote string gets kFLinkStale (FAULT -> contactors open, print-only).
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include "StringMon.h"

namespace Link {
void begin();
void tick();
void onFrame(const CAN_message_t &msg);   // from CAN2 RX
StringMon::Summary remote(int s);         // A: B's string s (faults include kFLinkStale)
uint8_t remoteFaults();                   // A: OR of remote faults (masked)
bool    peerAlive();                      // B: A's heartbeat fresh
void print();
}  // namespace Link
