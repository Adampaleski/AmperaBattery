#pragma once

#include <FlexCAN_T4.h>

namespace BicmDecode {

void begin();
void onFrame(const CAN_message_t &msg);
void tick();

int  seriesCellCount();
int  moduleCount();
bool packStable();

// 1-based pack cell. Returns 0 if missing / out of range.
float cellVoltage(uint8_t cell);

void printDecodeDetails();
void handleSerial(char c);

}  // namespace BicmDecode
