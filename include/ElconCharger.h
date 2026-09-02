#pragma once

#include <stdint.h>

// Shop Elcon / TC charger CAN (common 29-bit protocol).
// 250 kbit, extended IDs. Sticker "E5" is the CAN node ID, not voltage.
// VoltBMSV2 chargercomms() Elcon TX uses the same 0x1806E5F4 layout.
// Winding (HK-MF-108 vs 144 vs 72) is unread — protocol is identical; do not
// hardcode a range. A 72/96 V box cannot finish 36S (~149 V at 4.15 V/cell).
// A 312 V box will not start a drained 36S (~90 V at 2.5 V/cell).
namespace ElconCharger {

constexpr uint8_t  kNodeId    = 0xE5;  // sticker
constexpr uint32_t kCtlId     = 0x180600F4u | (static_cast<uint32_t>(kNodeId) << 8);  // 0x1806E5F4
constexpr uint32_t kStatusId  = 0x18FF5000u | kNodeId;                               // 0x18FF50E5

constexpr uint8_t kCtlLen     = 8;
constexpr uint8_t kStatusLen  = 8;

// BYTE5 (buf[4]) of 0x1806E5F4 — TC protocol 1430 / Elcon CAN-Commands:
// 0x00 = start charging. 0x01 = battery protection, close output.
// (Status bit3 "battery connected" is charger->BMS 0x18FF50E5, not this byte.)
constexpr uint8_t kCtlCharge  = 0x00;
constexpr uint8_t kCtlStop    = 0x01;

// 0x18FF50E5 BYTE5 status flags (bit set = fault).
constexpr uint8_t kStHwFail      = 1u << 0;
constexpr uint8_t kStOverTemp    = 1u << 1;
constexpr uint8_t kStInputVolt   = 1u << 2;
constexpr uint8_t kStBattOpen    = 1u << 3;  // 1 = not connected / reverse
constexpr uint8_t kStCommTimeout = 1u << 4;

}  // namespace ElconCharger

#if 0
// Brusa NLG5x1 Mode C — NOT the live path. Shop has Elcon, no Brusa.
// Do not compile this alongside Elcon on CAN2 (500 kbit / 11-bit vs 250 kbit / 29-bit).
// Left dead so a future Brusa is not half-wired from leftover 0x618 TX.
namespace BrusaNlg5 {
constexpr uint32_t kCtlId     = 0x618;
constexpr uint32_t kStatusId  = 0x610;
constexpr uint32_t kActIId    = 0x611;
constexpr uint32_t kActIIId   = 0x612;
constexpr uint32_t kTempId    = 0x613;
constexpr uint32_t kErrId     = 0x614;
constexpr uint8_t kCtlLen     = 7;
constexpr uint8_t kCtlEnable  = 0x80;
constexpr uint8_t kCtlIdle    = 0x00;
}  // namespace BrusaNlg5
#endif
