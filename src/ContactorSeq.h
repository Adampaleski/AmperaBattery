#pragma once

#include <stdint.h>

namespace ContactorSeq {

enum class State : uint8_t {
    Open = 0,
    Precharge,
    MainClose,
    Closed,
    Fault,
};

struct Outputs {
    bool precharge = false;
    bool mainPos   = false;
    bool mainNeg   = false;
};

void begin();
void tick();

void toggleDeadman();
void setDeadman(bool on);
bool deadman();

void trip(const char *why);
State state();
const char *stateName(State s);

Outputs intended();
Outputs driven();  // all false unless BMS_CAP_CONTACTOR_DRV && !TELEMETRY_ONLY

void printStatus();

}  // namespace ContactorSeq
