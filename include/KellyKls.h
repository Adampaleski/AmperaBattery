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

// 0x0CF11E05 DLC 8 (verify on wire):
//   b0-b1  RPM            uint16 LE
//   b2-b3  motor current  int16 LE, 0.1 A
//   b4-b5  battery V      uint16 LE, 0.1 V
//   b6     error code     uint8
//   b7     error bits     uint8
//
// 0x0CF11F05 DLC 8 (verify on wire):
//   b0     throttle %     uint8 0–255 → scale /2.55 for %
//   b1     controller °C  uint8 (offset TBD — often raw °C or +40)
//   b2     motor °C       uint8
//   b3-b7  reserved / NPS status — capture before decoding

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
