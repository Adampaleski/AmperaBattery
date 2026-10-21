#pragma once
// Reduce-only throttle trim at 1 kHz through the MCP4131 (include/DriveProfile.h).
// THROTTLE_TRIM_ENABLE=0 (default): nothing is written. CREW REVIEW.
#include <stdint.h>
#include "DriveProfile.h"
namespace ThrottleTrim {
void begin();
void tick();                          // loop(): commands + stats; the 1 kHz work runs in an IntervalTimer
bool handleLine(const char *line);    // "M <key> [pin]", "MPIN <old> <new>", "MP <key> <field> <v>", "M?"
void print();
uint32_t maxLatencyUs();
void linkTx(void (*send)(uint32_t id, const uint8_t *b));   // from Link::tick (dual build)
void linkRx(uint32_t id, const uint8_t *b);                  // from Link::onFrame              // worst ISR period jitter + compute
}
