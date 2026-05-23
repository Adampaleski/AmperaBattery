#pragma once

#include <FlexCAN_T4.h>

namespace BicmSniffer {

void begin();
void onFrame(const CAN_message_t &msg);
void tick();

bool canDebugEnabled();
void setCanDebug(bool on);

void printIdStats();
void resetStats();

}  // namespace BicmSniffer
