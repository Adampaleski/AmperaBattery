#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>

// PlatformIO link fix for some FlexCAN_T4 revisions.
inline uint8_t FlexCAN_T4_Base::getFirstTxBoxSize() {
    return 8;
}

struct CAN_message_flags_t {
    uint8_t extended = 0;
    uint8_t remote   = 0;
    uint8_t overrun  = 0;
};

// Thin wrapper: RX mailboxes 0–14, TX on MB15, FIFO off (bench-proven layout).
// Bus is CAN3 (BICM 125k, 11-bit, pins 30/31) or CAN2 (Elcon 250k, 29-bit, pins 0/1).
template <CAN_DEV_TABLE Bus>
class FlexCanCompatT {
public:
    void begin(uint32_t baudrate, bool extendedIds = false);
    void restart(uint32_t baudrate, bool listenOnly = false);

    bool available();
    bool read(CAN_message_t &msg);
    int  writeStatus(const CAN_message_t &msg);
    uint32_t getTXQueueCount();
    bool error(CAN_error_t &err, bool printDetails = false);

private:
    FlexCAN_T4<Bus, RX_SIZE_256, TX_SIZE_16> can_;
    CAN_message_t pending_{};
    bool has_pending_      = false;
    bool started_          = false;
    uint32_t current_baud_ = 0;
    bool listen_only_      = false;
    bool extended_ids_     = false;
};

// Controller map. Single-bus bench envs: CAN3 = BICM, CAN1 = string 2, CAN2 = Elcon/Kelly (unchanged).
// Dual-string 4-bus pack (Adam 2026-10-21, CAN_LAYOUT_V2=1, default when DUAL_STRING=1):
//   CAN1 22/23 @125k = local BICM string 1 (A: S1, B: S3)
//   CAN2 0/1   @125k = local BICM string 2 (A: S2, B: S4)
//   CAN3 30/31 @250k = vehicle bus: Kelly + Elcon + A-B link (B joins the same bus; was "spare")
// CREW REVIEW: firmware logic change (bus remap), not bench-tested.
#ifndef CAN_LAYOUT_V2
#if defined(DUAL_STRING) && DUAL_STRING
#define CAN_LAYOUT_V2 1
#else
#define CAN_LAYOUT_V2 0
#endif
#endif
#if CAN_LAYOUT_V2
#define BICM1_DEV CAN1
#define BICM2_DEV CAN2
#define VEH_DEV   CAN3
#define BICM1_NAME "CAN1 22/23"
#define BICM2_NAME "CAN2 0/1"
#define VEH_NAME   "CAN3 30/31"
#else
#define BICM1_DEV CAN3
#define BICM2_DEV CAN1
#define VEH_DEV   CAN2
#define BICM1_NAME "CAN3 30/31"
#define BICM2_NAME "CAN1 22/23"
#define VEH_NAME   "CAN2 0/1"
#endif
using FlexCanCompat = FlexCanCompatT<BICM1_DEV>;
extern FlexCanCompat Can0;                      // local BICM string 1
extern FlexCanCompatT<VEH_DEV> CanCharger;      // 250k vehicle bus: Elcon + Kelly + link
extern FlexCanCompatT<BICM2_DEV> CanString2;    // local BICM string 2 (dual-string build)
