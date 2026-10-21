#pragma once
// Kelly KLS limit outputs, driven from BmsCore (Teensy A, dual-string build).
//
// HONEST LIMITATION: the KLS CAN protocol ("KLS Broadcast CAN Protocol" 1.1)
// defines only two frames, 0x0CF11E05 and 0x0CF11F05, both sent BY the
// controller every 50 ms. The controller accepts no CAN commands, so the BMS
// cannot set a current limit over CAN. Limits go through KLS digital inputs:
//   OUT3 pin 20  LOW-CAP RELEASE (Adam + crew 2026-10-21, FAIL-SAFE): drives a
//                         relay coil (FET low-side). Relay NC contact feeds Kelly
//                         12 V (pin 11) into ECO (pin 22, low-speed switch, active
//                         HIGH per default SW Level, SOURCED KLS-8080N V2.11).
//                         Coil OFF (Teensy dead/unpowered/reset) = ECO 12 V = LOW cap.
//                         Teensy must actively HIGH OUT3 to RELEASE the cap.
//                         Cap held (OUT3 LOW) for BMS limp OR Kid mode.
//                         Relay part + ECO pin on Adam's harness UNVERIFIED.
//                         Max speed % per Kelly config tool; limits speed, NOT amps.
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
