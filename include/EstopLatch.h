#pragma once
// E-stop software latch (crew 2026-10-22, CRITICAL). Pure C++ (no Arduino), host-tested by scripts/test_estop_latch.cpp.
// Hardware (docs/ESTOP.md): 3x Schneider ZBE102 NC blocks. Block 1 = both EV200 coils, block 2 = precharge coil + Kelly KSI,
// block 3 = Teensy B log input (the "NC2 reports" input). Blocks 1-2 are the real cut, wired directly (no relay).
// Rule: when the log input reports OPEN (pressed, or wire broken), the Teensy drops every coil driver and LATCHES.
// Releasing the button never closes anything: only an explicit reset while the input is CLOSED (and key cycled) clears it.
// NOT wired into any output yet: BMS_CAP_CONTACTOR_DRV=0 / TELEMETRY_ONLY=1, so today this only reports (print-only).
#include <cstdint>
namespace Estop {
struct Latch {
    bool latched = false;     // true = coil drivers must be OFF
    uint32_t trips = 0;
    // inputClosed: block-3 contact reads closed (button released, wire intact). Call every loop.
    void update(bool inputClosed) { if (!inputClosed) { if (!latched) trips++; latched = true; } }
    // Reset request (e.g. key off -> on). Refused while the input is still open. Returns true if cleared.
    bool reset(bool inputClosed) { if (!inputClosed) return false; latched = false; return true; }
    // Gate for every coil driver: AND this with the normal contactor/precharge command.
    bool coilsAllowed() const { return !latched; }
};
}
