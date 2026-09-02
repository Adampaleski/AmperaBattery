#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>

namespace ChargerCan {

struct Stats {
    uint32_t rxCount          = 0;
    uint32_t txCount          = 0;
    uint32_t txFailCount      = 0;
    uint32_t errorEventCount  = 0;
    uint32_t lastRxMs         = 0;
    uint32_t lastRxId         = 0;
};

void begin();
void tick();

const Stats &stats();
uint32_t lastRxAgeMs();

using FrameHandler = void (*)(const CAN_message_t &msg);
void setFrameHandler(FrameHandler handler);

int writeFrame(const CAN_message_t &msg);

}  // namespace ChargerCan
