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

// Sum of all PACK_S_CELLS if every cell is valid, else 0.
float packVoltage();
// Sum of cells [first, first+count-1], 0 if any invalid. Bench 2x30s sections.
float rangeVoltage(int first, int count);
float minCellVoltage();
float maxCellVoltage();

void printDecodeDetails();
void handleSerial(char c);

}  // namespace BicmDecode
