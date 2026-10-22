#pragma once
// Host stub of Arduino.h for scripts/test_soc_run.cpp.
#include <stdint.h>
#include "Print.h"
extern uint32_t g_hostMs;
inline uint32_t millis() { return g_hostMs; }
extern Print Serial;   // Config.h maps SERIALCONSOLE to Serial
