#pragma once
// Host stub of Teensy EEPROM (4284 bytes, erased = 0xFF) for scripts/test_soc_run.cpp.
#include <stdint.h>
#include <string.h>
struct HostEeprom {
    uint8_t b[4284];
    int writes = 0;   // bytes actually changed
    HostEeprom() { memset(b, 0xFF, sizeof b); }
    uint8_t read(int a) const { return b[a]; }
    void update(int a, uint8_t v) { if (b[a] != v) { b[a] = v; writes++; } }
};
extern HostEeprom EEPROM;
