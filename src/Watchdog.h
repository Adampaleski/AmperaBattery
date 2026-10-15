#pragma once

#include <stdint.h>

// Teensy 4.1 hardware watchdog (i.MX RT1062 WDOG3 / RTWDOG), direct registers.
// ~2 s timeout on the 32 kHz LPO clock. Fed ONLY from loop() in main.cpp.
// If loop() stalls for ~2 s the chip resets; on reset every GPIO comes up as
// an input (coil FET gates pulled low by their resistors) and setup() drives
// the contactor pins LOW before anything else.
namespace Watchdog {

// Read + clear SRC_SRSR. Call first thing in setup(), before begin().
void captureResetCause();
bool lastResetWasWatchdog();
uint32_t lastResetRaw();

void begin(uint32_t timeoutMs = 2000);
void feed();

}  // namespace Watchdog
