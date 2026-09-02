#pragma once

#include <stdint.h>

// Brusa NLG5x1 Mode C CAN (official NLG5 manual table + VoltBMSV2 TX).
// 500 kbit, 11-bit IDs. One charger — do not send finishing-charger 0x638.
namespace BrusaNlg5 {

constexpr uint32_t kCtlId     = 0x618;  // BMS -> charger, DLC 7, 100 ms
constexpr uint32_t kStatusId  = 0x610;  // charger -> BMS, DLC 4, 100 ms
constexpr uint32_t kActIId    = 0x611;  // charger -> BMS, DLC 8, 100 ms
constexpr uint32_t kActIIId   = 0x612;  // charger -> BMS, DLC 8, 100 ms
constexpr uint32_t kTempId    = 0x613;  // charger -> BMS, DLC 8, 1000 ms
constexpr uint32_t kErrId     = 0x614;  // charger -> BMS, DLC 5, 1000 ms

constexpr uint8_t kCtlLen     = 7;
constexpr uint8_t kStatusLen  = 4;
constexpr uint8_t kActILen    = 8;

constexpr uint8_t kCtlEnable  = 0x80;  // VoltBMSV2 msg.buf[0]
constexpr uint8_t kCtlIdle    = 0x00;

}  // namespace BrusaNlg5
