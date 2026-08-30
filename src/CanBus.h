#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>

namespace CanBus {

struct Stats {
    uint32_t rxCount            = 0;
    uint32_t keepAliveTxCount   = 0;
    uint32_t keepAliveSkipCount = 0;
    uint32_t keepAliveFailCount = 0;
    uint32_t errorEventCount    = 0;
    uint32_t recoveryCount      = 0;
    uint32_t controllerRestarts = 0;
    uint32_t lastRxMs           = 0;
    uint32_t lastRxId           = 0;
    uint32_t txQueueHighWater   = 0;
    uint32_t balanceTxCount     = 0;
    uint32_t balanceFailCount   = 0;
};

void begin();
void tick();

bool keepAliveEnabled();
void setKeepAliveEnabled(bool on);

const Stats &stats();
uint32_t lastRxAgeMs();

using FrameHandler = void (*)(const CAN_message_t &msg);
void setFrameHandler(FrameHandler handler);

int writeFrame(const CAN_message_t &msg);

}  // namespace CanBus
