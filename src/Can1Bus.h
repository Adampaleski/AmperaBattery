#pragma once
// CAN1 (pins 22/23) @125k: string 2 BICM bus on the dual-string build.
// RX all frames -> handler; TX only the 0x200 keep-alive (BICMs stop reporting
// without it; BMS_CAP_KEEPALIVE_TX=1). Never balance/charge TX.
#include <Arduino.h>
#include <FlexCAN_T4.h>

namespace Can1Bus {
using FrameHandler = void (*)(const CAN_message_t &msg);
void begin();
void tick();
void setFrameHandler(FrameHandler h);
uint32_t rxCount();
uint32_t keepAliveTx();
uint32_t keepAliveFail();
uint32_t lastRxAgeMs();
void printStatus();
}  // namespace Can1Bus
