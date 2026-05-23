#pragma once

#include <FlexCAN_T4.h>

namespace BicmDecode {

void begin();
void onFrame(const CAN_message_t &msg);
void tick();

int  seriesCellCount();
int  moduleCount();
bool packStable();

void printDecodeDetails();
void handleSerial(char c);

}  // namespace BicmDecode
