#pragma once
// Single-string precharge gate + fail -> lockout (no retry) (chief review CRITICAL #1, 2026-10-22).
// Pure logic, host-tested by scripts/test_precharge_gate.cpp.
// Rule: Main+ may close ONLY when a VALID measured HV bus voltage is within the agreed delta of a
// VALID pack voltage: busV >= packV * ratio (ratio 0.95 = Config.h kPrechargeRatio, SOURCED Sparky
// 2026-10-17), and precharge has run >= minMs. There is NO timer path to Main+: no HV sense
// (HV_ANALOG_ENABLE=0), no pack voltage, or not there by timeoutMs -> precharge FAIL, mains stay open.
// Fail -> PRECHARGE-FAIL fault: everything open, Lockout until a key cycle. NO automatic retry
// on either path (chief 2026-10-22). Timing: 300 ms min / 3 s timeout (agreed, same as dual-string).
#include <stdint.h>

namespace PrechargeGate {

struct Cfg {
    uint32_t minMs;        // precharge must run at least this long
    uint32_t timeoutMs;    // not within delta by then -> fail
    float    ratio;        // busV >= packV * ratio
    float    minPackV;     // pack below this = not a real pack reading -> no close
};

// Bus within the agreed delta of the pack? Both readings must be valid; never true without them.
inline bool withinDelta(bool busValid, float busV, bool packValid, float packV, const Cfg &c) {
    if (!busValid || !packValid) return false;
    if (!(packV >= c.minPackV)) return false;          // also rejects NaN
    if (!(busV == busV)) return false;                  // NaN bus
    return busV >= packV * c.ratio;
}

enum class Step : uint8_t { Wait, CloseMain, Fail };

// One precharge-state evaluation. Fail is the ONLY outcome of the timeout.
inline Step precharge(uint32_t elapsedMs, bool busValid, float busV, bool packValid, float packV, const Cfg &c) {
    if (elapsedMs >= c.minMs && withinDelta(busValid, busV, packValid, packV, c)) return Step::CloseMain;
    if (elapsedMs >= c.timeoutMs) return Step::Fail;
    return Step::Wait;
}

}  // namespace PrechargeGate
