#pragma once

#include <FlexCAN_T4.h>
#include "KellyKls.h"

namespace KellyDecode {

void begin();
void onFrame(const CAN_message_t &msg);
void tick();

const KellyKls::Telemetry &telemetry();
void printStatus();

}  // namespace KellyDecode
