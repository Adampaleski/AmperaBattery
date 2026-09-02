#pragma once

#include <stdint.h>
#include <FlexCAN_T4.h>

namespace ChargeTx {

void begin();
void tick();
void onFrame(const CAN_message_t &msg);

void setChargeRequest(bool on);
bool chargeRequested();
void toggleChargeRequest();

// Pack Elcon 0x1806E5F4 (DLC 8, extended). enable=false sets control 0x01 (stop).
void packControl(uint8_t buf[8], bool enable);

void printStatus();

}  // namespace ChargeTx
