# Body / aux I/O: seats, cooling, lights, 2WD/4WD, diff lock (2026-10-21)

Status: **PROPOSED, nothing wired, nothing flashed.** The pins are reserved in `include/Config.h` (`Pins::`), but no firmware drives them and no flag changed.
Hardware: dual board gets **J9A (1×14) + J9B (1×10) BODY** headers next to Teensy A (placement v3, board stays 78 wide). It feeds the **outputs daughterboard v2**
(`perfboard/make_body.py`, `teryx_body_daughter.layout.json`), which is **not routed** yet and should be routed together with the dual-board 300-try reroute.

## Vehicle facts (sourced)
- 2008 Teryx 750 (KRF750A8F/B8F): 4WD is an **engine-vacuum diaphragm actuator**, switched by a solenoid valve, that pulls a cable to the front final gear. A switch on the front diff lights the 4WD indicator.
  Sources: 2008 Teryx 750 service manual contents ("vacuum-actuator inspection and solenoid-valve tests", repairloader.com listing); teryxforums.net threads 9680 and 17597 (diaphragm near the passenger seat, two vacuum lines, cable to the diff lever, "voltage only in 4WD").
- With the engine removed there is no vacuum. **Mechanic's plan, which replaces the vacuum parts:** a 12 V linear actuator with pot feedback on the 4WD cable (about 3/4 in of travel). The 2008 diff lock is a mechanical lever, so it gets a second linear actuator on its cable.
- Shift rules come from the Kawasaki Teryx owner's manual, 2010+ editions (KRF800, 99803-0268, "2WD/4WD Shifting" and "Shifting the Differential", pws.ktivs.net): "Always stop the vehicle before changing from 2WD to 4WD and vice versa." For the diff lock: "Stop the vehicle completely", and do not lock it while turning. **The 2008 manual's wording is UNVERIFIED.**

## Teensy A pin map (new entries)
| Pin | Name | Type | Load / source | Status |
|---|---|---|---|---|
| 5 | SEAT_DRV (OUT7) | PWM out | driver seat heater FET | PROPOSED, kit UNVERIFIED |
| 6 | SEAT_PAS (OUT8) | PWM out | passenger seat heater FET | PROPOSED |
| 7 | RAD_FAN (OUT9) | out | radiator fan relay coil | PROPOSED, on at ~40 °C |
| 8 | COOL_PUMP (OUT10) | PWM out | Bosch coolant pump | PROPOSED, runs with key on |
| 9 / 10 | ACT4WD_IN1/IN2 | PWM out ×2 | H-bridge #1, 4WD actuator | actuator and H-bridge UNVERIFIED |
| 28 / 29 | ACTDIF_IN1/IN2 | PWM out ×2 | H-bridge #2, diff actuator | UNVERIFIED |
| 41 (A17) | POS_4WD | ADC | 4WD actuator pot wiper (3.3 V) | UNVERIFIED |
| 39 (A15) | POS_DIFF | ADC | diff actuator pot wiper | UNVERIFIED |
| 40 | ST_4WD (IN5) | in | optional stock 4WD switch on the front diff, 27k/10k divider | optional |
| 37 | ACT_EN | out | R_EN+L_EN of BOTH BTS7960s, 10k pull-down (disabled at boot) | PROPOSED |
| 26 (A12) / 27 (A13) | IS_4WD / IS_DIFF | ADC | BTS7960 IS current sense via 10k/18k (5 V → 3.21 V) + 100 nF; fault ~7 A PROPOSED, IS scale UNVERIFIED | PROPOSED |
| 35 / 36 | LIGHT_DATA / LIGHT_CLK | out (bit-banged) | 2× 74HC595 lights; MCP4131 shares them | chief requirement |
| 38 | THR_CS | out | MCP4131 chip select | PROPOSED |
| 32 | LIGHT_LATCH | out | 595 RCLK | PROPOSED |
| 34 | LIGHT_OE | out | 595 /OE, pulled up (lights off at boot) | PROPOSED |
| 595 bits 0–8 | HEAD_LO, HEAD_HI, TAIL, BRAKE, TURN_L, TURN_R, REVERSE, BAR, WORK | via FET | lights; headlights/bar/work through relays | PROPOSED |

Hazard = TURN_L + TURN_R. No SPI peripheral is used for the body I/O. SPI0 is unusable anyway because 11/12/13 are OUT1/OUT2/LED.
**Spare edge pins: none.** 33 is RESERVED for charge plug-in detect (PROPOSED). 24 (HV_SENSE) is optional and unused by default.

## Dash / sim (sim/controls.js, sim/dash.js "Lights · 4WD" page)
Every threshold is PROPOSED and can be edited on the page (saved in localStorage):
- 2WD/4WD: only at ≤ 1 mph with the throttle released below 2 %. An optional brake requirement is off by default because the manual doesn't call for one.
- Diff lock: Off, Partial or Full position presets. It works only in 4WD, at ≤ 1 mph, with the throttle released. It unlocks automatically above 15 mph, and you can't leave 4WD while it's locked. Steering angle sensing is UNAVAILABLE, so the page shows a warning instead of blocking.
- Actuator endpoints are bench PLACEHOLDERS: 4WD 5/95 %, diff 5/50/95 %, with a 3 % tolerance.
- Faults: a stall (less than 1 % movement in 600 ms) or a timeout (4 s) turns the H-bridge off, logs the fault with its reason, and blocks further shifts until the driver clears it.
- Lights: low, high (which also turns on low), tail, light bar or aux, work, turn left and right and hazard blinking at 1.5 Hz, plus automatic brake (pedal above 3 %) and reverse (gear R) lights.
- Buttons that can't be used are greyed out and say why, for example "Stop (≤ 1 mph) to shift 2WD/4WD".

## Chosen parts (Sparky 2026-10-21, specs as given, not re-checked against datasheets, nothing bought)
- Actuator: Glideforce **LACT4P-12V-10**. Stall is 7 A at 12 V; it draws about 1 A unloaded and over 3 A at full load. Speed is about 1.1 in/s unloaded and 0.9 in/s loaded. IP65. The alternative is the -20 version (110 lb, half the speed). Pot resistance is UNVERIFIED; feed the pot from 3.3 V.
- Driver: **BTS7960** module, one per actuator. RPWM and LPWM go on the two existing pins (9/10 for 4WD, 28/29 for the diff). R_EN and L_EN are tied together with a pull-down, so the driver is disabled at boot. 3.3 V logic compatibility is UNVERIFIED, and the VNH5019 is the alternative.
  Enable: pin 37, shared by both modules, with a 10k pull-down on the daughterboard.
- Faults: an over-current fault at about **7 A** (PROPOSED) reads the IS lines on A12/A13. The BTS7960 IS-to-amps ratio and its module resistor are UNVERIFIED, so it needs a bench calibration. Over-current isn't simulated; the sim's stall and timeout faults use the pot.
- Fuses / relays (ESTIMATE): fan 15 A, headlights 10 A, coolant pump 10 A, seats 10 A each.

## Earlier candidates (per Mechanic, not re-checked, nothing bought)
- Firgelli 4 in stroke, 150 lb, with pot: $149.99
- Glideforce LACT4P: $194.88

Stall current, speed and pot resistance must come from the chosen part's datasheet before the H-bridge and fuse are sized: **UNVERIFIED.**

## Zero spare pins: PROPOSED fix (not implemented)
Teensy A now has no free edge pins; 33 is reserved for charge detect. Teensy 4.1 has three I2C buses, but **none is free today**: Wire (18/19) = IN3/IN4, Wire1 (16/17) = Serial4 to the ESP32, and Wire2 (24/25) = HV_SENSE/CUR_VC. So there is no "existing I2C bus" to hang an expander on yet.
1. **Recommended: MCP23017 on Wire (SDA 18 / SCL 19).** IN3 and IN4 are only `pinMode(INPUT)` in `BmsApp.cpp` and nothing reads them. Repurpose them as I2C with 4.7k pull-ups to 3V3. One MCP23017 gives 16 I/O. Move the 9 light bits plus the 4WD stock switch onto it. That drops the two 595s and frees 32, 34, 35, 36 and 40. The MCP4131 then needs its own 2 pins: reuse 35 and 36 bit-banged. Net result: **3 pins free (32, 34, 40)**, and the 7 unused expander bits cover more lights later.
   Costs: lights and brake light depend on I2C, which needs a bus watchdog plus a hardware default (MCP23017 resets to inputs, so the FET pulldowns keep the loads off). The turn-signal blink rate over I2C is fine.
2. **Alternative: Teensy B takes the lights.** Teensy B has its own edge pins and already listens on CAN. It could drive the lights from a CAN or link command. That mixes body loads into the redundant BMS role, so I don't recommend it.
3. **Alternative: free pin 24.** HV_SENSE is optional and unused by default (HV_ANALOG_ENABLE=0), which gives 1 pin if the HV divider is never fitted.
