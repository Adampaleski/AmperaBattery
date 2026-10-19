#pragma once
// Kelly KLS limit outputs, driven from BmsCore (Teensy A, dual-string build).
//
// HONEST LIMITATION: the KLS CAN protocol ("KLS Broadcast CAN Protocol" 1.1)
// defines only two frames, 0x0CF11E05 and 0x0CF11F05, both sent BY the
// controller every 50 ms. The controller accepts no CAN commands, so the BMS
// cannot set a current limit over CAN. Limits go through KLS digital inputs:
//   OUT3 pin 20  LIMP  -> opto/transistor onto the KLS low-speed (speed switch)
//                         input. Max speed % is set in the Kelly config tool;
//                         it limits speed/throttle, NOT battery amps.
//   OUT5 pin 2   DERATE PWM (duty = kellyPct) -> RC filter + analog switch /
//                         digital pot in the throttle wiper. Hardware TBD.
//   OUT6 pin 3   REGEN INHIBIT -> opto in series with the KLS regen/brake-switch
//                         input, HIGH = regen allowed. Wiring TBD per harness.
// The hard stop for a real fault is still the contactors (ContactorSeq).
//
// Gated: pins are only written when BMS_CAP_KELLY_LIMIT_DRV=1 and
// TELEMETRY_ONLY=0. Default prints what it WOULD do.

namespace KellyLimit {
void tick();
void printStatus();
}  // namespace KellyLimit
