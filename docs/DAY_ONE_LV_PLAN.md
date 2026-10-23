# Day One: low-voltage wiring plan (Mechanic draft, Sparky to review)

Follows `AmperaBattery/docs/CONNECTIONS.md` steps W1–W8. Wire IDs match PINMAP.yaml. Tick a box only after its meter check passes. **If any reading is wrong, STOP.** Don't move on until it's fixed.

## Rules for the whole day
- **No HV.** All 4 string fuses OUT, no pack connected. Power comes from the bench 12 V supply (current-limited to about 1 A) or the 12 V battery only.
- **Cut 5 V before any harness or sense change**, including BICM plugs, LEM and pedal leads, and the Teensy pins.
- **Never cut the factory harness.** Use pigtails and DT connectors only.
- **No firmware on the Teensys** until Gate 1 passes.
- Label both ends of every wire with its ID and the board (TA or TB).
- Anything marked **TBD** is unresolved. Don't wire it and don't guess at it.

## W0. Before you start (power off)
- [ ] **DC-DC isolation** (TDC-JH-144-12, per the shop log). Meter the HV input +/− to the 12 V− output. Expect open or MΩ (≥1 MΩ on an insulation tester). **STOP: don't bond 12 V− to chassis until this passes.**
- [ ] Every fuse slot is empty, and its rating is written on the block. Ratings (Sparky, 10/22): F-KEL 2 A, F-COIL 10 A, F-EBOX 5 A. F-MAIN is about 1.25× the TDC-JH rated output (Adam reads it off the label); the P01/P02 gauge follows it (10 AWG up to 30 A, 8 AWG up to 40 A, 6 AWG up to 60 A).

## W1. 12 V and grounds
| Wire | Connect | Meter check (power off unless noted) | Expect |
|---|---|---|---|
| G01 | 12 V− bus to frame strap | Ω, 12 V− to bare frame | **< 0.1 Ω** |
| G02 | Kelly case to frame | Ω | **< 0.1 Ω** |
| G03 | Buck OUT− to TB-GND | Ω to 12 V− | < 0.5 Ω |
| P01/P02 | DC-DC +12 V to F-MAIN to bus + (gauge **TBD**) | Ω end to end; + to chassis | < 1 Ω; **> 1 MΩ** |
| P03 | Bus + to F-KEL to Kelly PWR | Continuity | < 1 Ω |
| P04 | Bus + to F-COIL | Continuity | < 1 Ω |
| P05 | Bus + to F-EBOX | Continuity | < 1 Ω |
| P07 | F-EBOX to F-BUCK 3 A to buck IN+ | Continuity | < 1 Ω |
| P08 | Buck OUT+ to TB-5V | **Power on, no loads** | **4.95–5.05 V** |
| P09 | Zener 1N5339B across TB-5V/GND | Diode test, band on +5 V | Diode reads one way only |

**Gate G (grounds):** each strap under 0.1 Ω. **STOP** if any reading is above 0.1 Ω: clean to bare metal and re-torque.
**Power test:** fuses in, 12 V on, bench current under 0.1 A with nothing loaded, 12–14 V on the bus, 5.00 V on TB-5V. **STOP** if the current limit trips. Power off afterwards.

## W2. Teensys (5 V off)
| Wire | Connect | Check | Expect |
|---|---|---|---|
| — | VIN/VUSB pad cut on both boards | Ω, VIN to VUSB | Open |
| G04/G06 | TA/TB GND to TB-GND | Ω | < 0.5 Ω |
| P11/P12 | TB-5V to TA/TB VIN | Power on: VIN to GND | 4.95–5.05 V |

Don't plug in USB and VIN together until the pad cut is confirmed.

## W3. CAN transceivers and vehicle bus (5 V off)
Only **4 SN65HVD230s are owned** (6 are needed), so on the bench fit **#1 (A-CAN1, C01) and #5 (A-CAN3, C05)**, or as Gilfoyle assigns. The rest are **TBD** until more are bought.
- [ ] C01/C05: TX to TXD and RX to RXD (not crossed). Power on: 3.3 V at VCC.
- [ ] C10 vehicle CAN (YE=H, GN=L, twisted). Power off, CANH to CANL: **≈ 60 Ω**. 120 Ω means one terminator is missing; open means none. **STOP** if it isn't about 60 Ω.

## W4. BICM, one string only (5 V off to plug in)
- [ ] G07 X2 pin 10 to TB-GND: < 0.5 Ω.
- [ ] P10 TB-5V to F-BICM 1 A to X2 pin 9 (string 1 only). Power on: 5 V at pin 9. **Never connect it to Teensy I/O.**
- [ ] C11 SN65 #1 to X2 pin 12 CAN_H / pin 11 CAN_L. Power off: ≈ 60 Ω. Power on: cells appear on the console (after Gate 1, telemetry build).
- Strings 2–4 (C12–C14) wait on more transceivers.

## W5. ESP32 gateway (later, optional on Day One)
- [ ] G05 TA GND to ESP32 GND: < 0.5 Ω. S30 TA pin 17 to ESP32 GPIO16, continuity.
- [ ] TA pin 16 and ESP32 GPIO17 stay **unconnected**. Serial4 (pins 16/17) is the gateway only; IN1–IN4 move to free GPIO, pins **TBD**. Pins 18/19 stay I2C. Don't wire IN1–IN4 yet.

## W6. Safety I/O, e-stop, throttle clamp, LEM, park, Kelly LV
**E-stop:** Harmony XB4/XB5 twist-release head with 3× ZBE102 blocks.
| Wire | Connect | Check | Expect |
|---|---|---|---|
| E01 | F-COIL out to block 1:11 and block 2:11 | Power on | 12 V at both terminal 11s |
| E02 | Block 1:12 to EV200 B+ and B− coil + | Released / pushed | < 1 Ω / open |
| E03 | Block 2:12 to AEV14012 precharge coil + and Kelly KSI pin 7 | Released / pushed; KSI current | < 1 Ω / open; **meter KSI draw, expect < 0.2 A** |
| E09 | Block 3:11 to TB-GND; 3:12 to Teensy B pin **TBD (PROPOSED 9)**, 10k pull-up | Released / pushed | LOW / HIGH |

**Gate E (e-stop cuts the coils):** power on, button pushed. Expect **0 V** at both EV200 coil + and the precharge coil +, and at Kelly pin 7. Twist to release: 12 V returns, and nothing should click on, because the coil FETs are off. **STOP** if any coil + stays live while pushed.

**Coil drivers (5 V off):** S01 (TA11 Main+), S02 (TA12 precharge), S03 (TA21 Main−), S20 (TA20 Low-cap relay). Meter gate to GND: **≈ 10 kΩ** (pull-down present).

**Throttle clamp:** B01, B02, B03, S10, S11 (TA4 alive, R6 ≈ 10 kΩ to GND), S12 (TB7 alive, R4 ≈ 10 kΩ), P20 (5 V at each chip VCC).
- K01: U2b out to Kelly pin 3 (shielded). Fit a **100k** pull-down from pin 3 to ground (decided 10/22). Check: with both 4066 switches open, pin 3 reads under 0.2 V.

**GATE 1 (no firmware, required before any flash):**
1. Teensys **unpowered**, rest of LV on: Kelly pin 3 **≈ 0 V**, and every coil FET gate (S01, S02, S03, S20) **≈ 0 V**.
2. Teensys powered but **held in reset** (PROGRAM/reset held): same readings, ≈ 0 V.
3. **STOP** if anything reads above about 0.1 V. That means the digipot mid-scale is getting through or a gate pull-down is missing.

**Pedal (5 V off to connect):**
- S14 track 1 to divider to **TA pin 40** (and MCP4131 P0A). S15 track 2 to its own 10k/18k divider to **TB pin 41** (PEDAL_IN_B).
- Power on: log the pedal-up and pedal-down volts on each track. Expect track 2 to read about half of track 1.
- **Full-voltage check:** with the clamp enabled on the bench, pedal down at the Kelly pin 3. Expect it close to the Hall pedal's full output through the 4066 with the chosen pull-down. **STOP** if it's well short, and report it to Sparky.

**LEM (5 V off):** S05/S06/S25/S26 with no current: ≈ 2.5 V at the DHAB and ≈ 1.6 V at the Teensy pin. S07/S27: ≈ 3.2 V at pin 25.
**Park:** S16 (TA32) and S17 (TB32): brake set 0 V, released 3.3 V.

**Kelly LV (per kelly-settings.md). Relay coils on vehicle 12 V only, never Kelly pin 11 (40 mA max). Pin 11 feeds contacts only.**
- K02 Foot_SW pin 15: pin 11 to park NC to a contact closed **only while both Teensys are alive** (series contacts) to pin 15. Relay/opto part and the coil wiring are **TBD**. Check: either Teensy off means pin 15 open; brake set means open.
- K03 ECO pin 22: pin 11 to the Low-cap relay **NC** to pin 22. Relay coil on fused vehicle 12 V via S20. Check: relay de-energised means **12 V at pin 22 (Low)**.

**S40 12 V sense:** pin **TBD (PROPOSED TB 26)**, divider **PROPOSED**. Don't wire it until the pin is decided.

## W7. HV (not Day One)
Skip it. String fuses stay OUT. Sparky must be present. String leads are 1/0, mains are 3/0.

## W8. Aux buttons (later)
A01–A05 later. Pressed: 12 V at relay 86; released: 0 V; grounds < 0.5 Ω.

## End of day
5 V off, then 12 V off. Note every reading against its wire ID in the checklist.

## Install check: LEM DHAB S/44 orientation (added 10/23, chief)
- Mount each LEM so discharge current flows AGAINST the arrow. Mark the arrow and the discharge direction with paint pen before the cable goes through.
- Bench check (first low-current discharge, e.g. a small load on one string): Ch1 output must DROP below 2.50 V. If it rises, the sensor is backwards (it clips near +271 A and the 400 A trip can never fire). STOP and flip it.
