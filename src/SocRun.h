#pragma once
#include <Print.h>
// Firmware side of the state-of-charge estimate (backlog item 3, wiring step).
// Runs the same include/SocEstimator.h coulomb counter the host checks and the
// simulator use, fed from CurrentSense (LEM DHAB S/44 on this Teensy) and the
// BmsCore "at charge limit" anchor.
//
// Role A (dual string) only, where BmsCore runs. Elsewhere every call is a
// no-op. No pins, nothing drives an output. EEPROM: the count is saved to
// include/SocStore.h (1469..1514) and restored at boot (2026-10-23 1 PM).
//
// HONEST LIMITS (today):
//   - CURRENT_SENSE_ENABLE=0, so CurrentSense::valid() is false (except the
//     bench I<amps> dry-run) and the count never runs on real current.
//   - Split LEM: this Teensy reads LEM A only (strings 1+2). LEM B's reading
//     over the A-B link is NOT built, so the count is HALF the bank. While
//     that is true socPct stays null in the hub frame (no half-bank percent).
//   - capAh = 0 (UNVERIFIED), so the percent is unknown even when counting.
//   - Persisted (SocStore): restored at boot, saved by the wear rules and forced
//     on the DRIVE/CHARGE -> IDLE edge. No KEY-ON pin or power-loss detect yet,
//     so a hard power pull keeps the last periodic save (<= 0.5 Ah / 60 s old,
//     PLACEHOLDER). A restored count with no valid current goes UNKNOWN after
//     2 s and that is saved too (fail closed). Off-state Ah UNVERIFIED.
//
// Serial: 'p' prints a SOC line after the contactor/current status.
namespace SocRun {
void begin();
void tick();                 // after ContactorSeq ran BmsCore this loop
void printStatus();
// STATUS_SCHEMA v1: the existing socPct value (number or null) ...
void printSocPct(Print &out);
// ... and an optional read-only "soc" object:
// {"available","known","state","usedAh","pct","drift","why","halfBank","capAh","restored","saves"}
void printJson(Print &out);
}  // namespace SocRun
