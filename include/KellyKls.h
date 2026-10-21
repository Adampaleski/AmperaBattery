#pragma once

#include <stdint.h>

// Kelly KLS8080 NPS — broadcast CAN (J1939-style 29-bit) @ 250 kbit.
// Shares Teensy CAN2 (pins 0/1) with Elcon. NOT the BICM 125 kbit CAN3 bus.
// NOT a serial sniffer / RS232 bridge mystery — controller already broadcasts.
//
// Layout below is the common public NPS map; VERIFY against a candump before
// trusting numbers in the hub. Placeholder until BMS_CAP_KELLY_RX=1 unpack.

namespace KellyKls {

// Extended 29-bit IDs (NPS broadcast). Byte order little-endian unless noted.
constexpr uint32_t kIdRpmCurrentV = 0x0CF11E05u;  // RPM / current / pack V / errors
constexpr uint32_t kIdThrottleTemp = 0x0CF11F05u; // throttle / controller+motor temps

// Layout per Kelly broadcast CAN protocol doc (docs/KELLY_CAN_ROLES.md; SOURCED doc, UNVERIFIED on Adam's unit).
// Both frames every 50 ms. Kelly accepts NO CAN commands (broadcast only).
// 0x0CF11E05 DLC 8:
//   b0-b1  RPM            uint16 LE, 1 rpm/bit (0-6000)
//   b2-b3  motor current  uint16 LE, 0.1 A (0-4000 = 0-400 A; UNSIGNED, saturates < Adam's 550 A)
//   b4-b5  battery V      uint16 LE, 0.1 V
//   b6-b7  error bits     ERR0-ERR7 (b6), ERR8-ERR15 (b7)
// 0x0CF11F05 DLC 8:
//   b0     throttle       0-255 = 0-5 V
//   b1     controller °C  raw - 40
//   b2     motor °C       raw - 30
//   b3     reserved
//   b4     status         bits3-2 feedback dir, bits1-0 command dir (0 stop, 1 fwd, 2 rev)
//   b5     switches       bit7 Boost, bit6 Foot_SW, bit5 FWD, bit4 REV, bit3 12V brake, bits2-0 Hall C/B/A
//   b6-b7  reserved
constexpr uint8_t kSwFoot = 0x40, kSwFwd = 0x20, kSwRev = 0x10, kSwBrake = 0x08;
inline bool neutral(uint8_t sw) { return (sw & (kSwFwd | kSwRev)) == 0; }

struct Telemetry {
    bool     haveRpmFrame     = false;
    bool     haveThrottleFrame = false;
    uint32_t lastRpmMs        = 0;
    uint32_t lastThrottleMs   = 0;
    uint32_t rpmFrameCount    = 0;
    uint32_t throttleFrameCount = 0;

    uint16_t rpm              = 0;
    int16_t  motorCurrentDeciA = 0;  // 0.1 A
    uint16_t batteryDeciV     = 0;   // 0.1 V
    uint8_t  errorCode        = 0;
    uint8_t  errorBits        = 0;

    uint8_t  throttleRaw      = 0;
    uint8_t  controllerTempRaw = 0;
    uint8_t  motorTempRaw     = 0;

    uint8_t  lastRpmRaw[8]      = {};
    uint8_t  lastThrottleRaw[8] = {};
};

}  // namespace KellyKls
