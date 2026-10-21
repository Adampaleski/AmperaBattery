# Body / aux I/O: seats, cooling, lights, 2WD/4WD, diff lock (2026-10-21)

Status: **PROPOSED, nothing wired, nothing flashed.** The pins are reserved in `include/Config.h` (`Pins::`), but no firmware drives them and no flag changed.
Hardware: dual board gets a new **J9 BODY** header (18 pins, Teensy A). It feeds the **outputs daughterboard v2**
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
| 26 / 27 | SPI1 MOSI/SCK | SPI | 2× 74HC595 lights + MCP4131 throttle pot | PROPOSED |
| 38 | THR_CS1 | out | MCP4131 chip select | PROPOSED |
| 32 | LIGHT_LATCH | out | 595 RCLK | PROPOSED |
| 34 | LIGHT_OE | out | 595 /OE, pulled up (lights off at boot) | PROPOSED |
| 595 bits 0–8 | HEAD_LO, HEAD_HI, TAIL, BRAKE, TURN_L, TURN_R, REVERSE, BAR, WORK | via FET | lights; headlights, bar and work lights through relays | PROPOSED |

Hazard is TURN_L and TURN_R together, so it has no channel of its own. SPI0 can't be used because pins 11, 12 and 13 are OUT1, OUT2 and LED.
**Spare edge pins left: 33, 35, 36, 37.** Pin 33 is PROPOSED for the charge plug-in detect.

## Dash / sim (sim/controls.js, sim/dash.js "Lights · 4WD" page)
Every threshold is PROPOSED and can be edited on the page (saved in localStorage):
- 2WD/4WD: only at ≤ 1 mph with the throttle released below 2 %. An optional brake requirement is off by default because the manual doesn't call for one.
- Diff lock: Off, Partial or Full position presets. It works only in 4WD, at ≤ 1 mph, with the throttle released. It unlocks automatically above 15 mph, and you can't leave 4WD while it's locked. Steering angle sensing is UNAVAILABLE, so the page shows a warning instead of blocking.
- Actuator endpoints are bench PLACEHOLDERS: 4WD 5/95 %, diff 5/50/95 %, with a 3 % tolerance.
- Faults: a stall (less than 1 % movement in 600 ms) or a timeout (4 s) turns the H-bridge off, logs the fault with its reason, and blocks further shifts until the driver clears it.
- Lights: low, high (which also turns on low), tail, light bar or aux, work, turn left and right and hazard blinking at 1.5 Hz, plus automatic brake (pedal above 3 %) and reverse (gear R) lights.
- Buttons that can't be used are greyed out and say why, for example "Stop (≤ 1 mph) to shift 2WD/4WD".

## Candidate actuators (per Mechanic, not re-checked, nothing bought)
- Firgelli 4 in stroke, 150 lb, with pot: $149.99
- Glideforce LACT4P: $194.88

Stall current, speed and pot resistance must come from the chosen part's datasheet before the H-bridge and fuse are sized: **UNVERIFIED.**
