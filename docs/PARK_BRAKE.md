# Park detection + parking-brake drive interlock (Adam 2026-10-21) — CREW REVIEW, nothing flashed

Labels: **SOURCED** (document / Adam), **ASSUMED**, **PROPOSED** (our design, adjustable), **UNVERIFIED**.

## Why a parking-brake switch
The drivetrain is motor → 2.0:1 chain → 4.375 diff (SOURCED). There is no gearbox Park, so Park = parking brake set.
Which parking brake the 2008 Teryx has, and whether it has a switch, is **UNVERIFIED** (need a photo of the lever / caliper).

## Hardware (PROPOSED)
Switch: a sealed lever/caliper switch with **3 poles** (or 2 switches): two NO poles for the Teensys, one NC pole for the Kelly.
```
Park pole 1 (NO, closes when SET) : Teensy A pin 18 (IN3, PARK_IN_A) -- 10k pull-up to 3.3 V -- switch -- GND
Park pole 2 (NO, closes when SET) : Teensy B pin 18 (IN3, PARK_IN_B) -- 10k pull-up to 3.3 V -- switch -- GND
  + 100 nF at each pin. LOW = brake SET. Broken wire / unplugged = HIGH = NOT parked (fail-safe for "parked").
Park pole 3 (NC, OPENS when SET) : Kelly 12 V out (pin 11) -- pole 3 -- Kelly Foot_SW (pin 15)
```
- Pins: 18 is IN3 on both Teensys. It was `pinMode(INPUT)` and never read; now `INPUT_PULLUP` + external 10k. `Config.h` static_asserts it against the other pins.
  - **Decision for Adam:** the MCP23017 expander option (docs/BODY_IO.md) wanted 18/19 as I2C on Teensy A. If you pick the expander, PARK_IN_A has to move.
- Carrier IN3 input conditioning (a divider or opto on the VoltBMS carrier?) is **UNVERIFIED**. Check it on the bench: the switch must pull the Teensy pin below 0.8 V.

## Hardware interlock (non-negotiable)
The Kelly **cannot drive while the parking brake is set, whatever the software does**:
- Kelly config **Foot Switch = checked** (item 92, KLS-8080N manual V2.11, SOURCED): "Connect Foot_SW to 12V to enable throttle, so motor can start." With SW Level = checked (default), high = enable (SOURCED).
- Pole 3 is NC and opens when the brake is set, so Foot_SW loses 12 V and the Kelly ignores the throttle.
- Chosen input: **Foot_SW pin 15** (throttle switch), not the FWD/REV feed. Breaking FWD/REV also works, but it fights the F/N/R neutral sense and reverse lamp. **UNVERIFIED** on Adam's unit: firmware version, whether Foot Switch is currently checked, harness pin. Bench test: brake set + throttle → 0 RPM; release → drives (HPD may require pedal back to 0 first).
- Like a neutral safety switch: the contact is in series with the enable, and a broken wire = no drive (fail-safe for driving).

## Software Parked (`include/ParkState.h`, sim `TeryxPark` in sim/controls.js)
`Parked = contact A SET AND contact B SET AND Kelly RPM == 0 (0x0CF11E05 fresh ≤200 ms) AND Kelly F/N/R neutral`.
Neutral = Kelly 0x0CF11F05 byte 6 bit5 (FWD) and bit4 (REV) both clear (SOURCED, Kelly broadcast CAN protocol doc). Kelly config **Three Gears Switch** must be checked for a real neutral (SOURCED item 90; UNVERIFIED on Adam's unit).

| Fault (logged, dash fault list) | Trigger (PROPOSED) |
|---|---|
| PARK-CONTACT-DISAGREE | A ≠ B for > 100 ms → also not Parked, precharge blocked |
| PARK-BRAKE-WHILE-MOVING | either contact set while RPM > 50 (~0.4 mph) for > 300 ms |
Live + latched; clear with "Clear park faults" (sim) / key cycle (firmware, PROPOSED).

## Uses
- **Kid mode:** entering, leaving or editing Kid only while Parked (sim enforces; firmware Selector hook PROPOSED).
- **Precharge:** blocked unless the parking brake is set on both contacts (`PRECHARGE_NEEDS_PARK=1`, PROPOSED, adjustable). Checked at key-on / precharge start only, so releasing the brake afterwards does not drop the contactors.
- **E-stop stays independent** of park.

## Firmware status
`ParkState.h` is header-only and tested (scripts/test_park.cpp 9/9). It is NOT yet wired into ContactorSeq / ThrottleTrim (contactors stay print-only). B → A contact state needs a link frame (PROPOSED: a bit in 0x6A4).

## Sim (tools/bms_sim.html, Cluster → Driver inputs)
Toggles: Parking brake set (default on), F/N/R Neutral (default on), Park contact B open-circuit, Clear park faults. Scenarios (scripts/test_controls.js + test_park.cpp):
park on/off · contact disagreement · brake on while moving · Kid change blocked while moving · precharge blocked (key-on refused) · stale Kelly CAN → not parked · interlock: park set + throttle = no drive.
Drive cycles and quick commands auto-set the brake for precharge, then release it and leave neutral (logged).

## Diff lock (checked 2026-10-21)
The sim **already has** diff lock (sim/controls.js, Lights · 4WD on the dash): Open / Partial / Full on a PROPOSED actuator, 4WD only, ≤1 mph, throttle released, auto-unlock above 15 mph, stall/timeout faults. It is variable and optional, as on the real machine. It is a body control and does **not** feed the vehicle model (no traction/turning effect is simulated), and it is not part of Park.
