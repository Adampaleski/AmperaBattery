#pragma once
#include <Print.h>
// Firmware side of the state-of-charge estimate (backlog item 3, wiring step).
// Runs the same include/SocEstimator.h coulomb counter the host checks and the
// simulator use, fed from CurrentSense (LEM DHAB S/44 on this Teensy) and the
// BmsCore "at charge limit" anchor.
//
// Role A (dual string) only, where BmsCore runs. Elsewhere every call is a
// no-op. Read-only: no pins, no EEPROM, nothing drives an output.
//
// HONEST LIMITS (today):
//   - CURRENT_SENSE_ENABLE=0, so CurrentSense::valid() is false (except the
//     bench I<amps> dry-run) and the count never runs on real current.
//   - Split LEM: this Teensy reads LEM A only (strings 1+2). LEM B's reading
//     over the A-B link is NOT built, so the count is HALF the bank. While
//     that is true socPct stays null in the hub frame (no half-bank percent).
//   - capAh = 0 (UNVERIFIED), so the percent is unknown even when counting.
//   - Not persisted across power-off yet (ties to the fault log, item 5).
//
// Serial: 'p' prints a SOC line after the contactor/current status.
namespace SocRun {
void begin();
void tick();                 // after ContactorSeq ran BmsCore this loop
void printStatus();
// STATUS_SCHEMA v1: the existing socPct value (number or null) ...
void printSocPct(Print &out);
// ... and an optional read-only "soc" object:
// {"available","known","state","usedAh","pct","drift","why","halfBank","capAh"}
void printJson(Print &out);
}  // namespace SocRun
