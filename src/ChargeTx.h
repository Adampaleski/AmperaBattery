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

// Pack NLG5_CTL 0x618 (DLC 7). enable=false clears the run bit.
void packControl(uint8_t buf[7], bool enable);

void printStatus();

}  // namespace ChargeTx
