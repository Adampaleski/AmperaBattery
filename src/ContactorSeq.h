#pragma once

#include <stdint.h>

namespace ContactorSeq {

enum class State : uint8_t {
    Open = 0,
    Precharge,
    MainClose,
    Closed,
    Fault,
    RetryWait,  // UNUSED since 2026-10-22 (no automatic precharge retry); kept so state numbers stay stable
    Lockout,    // dual-string: precharge failed after max retries; dead-man off to clear
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
Outputs driven();
uint32_t coreFaults();
#if defined(DUAL_STRING) && DUAL_STRING && (!defined(TEENSY_ROLE) || TEENSY_ROLE == 0)
}  // namespace ContactorSeq
#include "BmsCore.h"
namespace ContactorSeq {
const BmsCore::Outputs &coreOut();
#endif  // BmsCore fault bits (dual role A), 0 otherwise  // all false unless BMS_CAP_CONTACTOR_DRV && !TELEMETRY_ONLY

void printStatus();

}  // namespace ContactorSeq
