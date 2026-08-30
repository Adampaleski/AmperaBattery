#pragma once

#include <stdint.h>

namespace BalanceTx {

void begin();
void tick();

// Pack current decoded cells into 0x300/0x310 payloads (no CAN TX).
// Returns number of cells marked to bleed. Frames are zeroed if the pack
// is not stable or no cell is a candidate.
int packFrames(uint8_t buf300[8], uint8_t buf310[5]);

// Called from CanBus just before 0x200. No-op unless BMS_CAP_BALANCE_TX.
void sendQueuedBeforeKeepAlive();

void printStatus();

}  // namespace BalanceTx
