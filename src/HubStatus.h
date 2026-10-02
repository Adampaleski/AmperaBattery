#pragma once

#include <Arduino.h>

// STATUS_SCHEMA v1 JSON-line emitter on Teensy Serial4 (RX16 / TX17) @ 115200.
// Consumed by teryx-ev-hub ESP32 gateway UART2 (GPIO16 RX ← TX17, GPIO17 TX → RX16).
// See teryx-ev-hub docs/STATUS_SCHEMA.md.

namespace HubStatus {

void begin();
void tick();

// One schema-v1 object, newline-terminated. Used by Serial4 emit and USB 'j'.
void printFrame(Print &out);

}  // namespace HubStatus
