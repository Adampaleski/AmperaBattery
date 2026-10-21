# Kelly KLS-8080N: CAN, regen and brake inputs, and what lives where (2026-10-21)

Sources:
- [M] Kelly KLS-8080N User Manual V2.11, https://media.kellycontroller.com/new/Kelly-KLS-8080NUserManuaV2.11.pdf
- [C] Kelly "Sinusoidal Wave Controller KLS-D/8080I/8080IPS Broadcast CAN Protocol", hosted in Kelly's KLS8080N-NPS download folder: https://kellycontroller.com.ua/download/KLS8080N-NPS/Sinusoidal-Wave-Controller-KLS-D-8080I-8080IPS-Broadcast-CAN-Protocol.pdf

The protocol doc is titled for the KLS-D/8080I, so whether it matches Adam's exact KLS-8080N firmware is **UNVERIFIED** until a bench candump.

## CAN broadcast (what the Kelly sends)
| Frame | Period | Bytes (LE where 2 bytes) | Label |
|---|---|---|---|
| 0x0CF11E05 | 50 ms | b0-1 RPM (1 rpm/bit, 0-6000); b2-3 motor current (0.1 A/bit, 0-4000 = 0-400 A); b4-5 battery V (0.1 V/bit, 0-180 V); b6-7 error bits ERR0-ERR15 | SOURCED [C] |
| 0x0CF11F05 | 50 ms | b0 throttle 0-255 = 0-5 V; b1 controller temp (°C + 40); b2 motor temp (°C + 30); b3 reserved; b4 status: bits3-2 feedback direction, bits1-0 command direction (0 stop, 1 fwd, 2 rev); b5 switches: bit7 Boost, bit6 Foot SW, bit5 FWD, bit4 REV, bit3 12 V brake switch, bits2-0 Hall C/B/A; b6-7 reserved | SOURCED [C] |
- The two frames go out about 25 ms apart (forum capture, UNVERIFIED).
- 250 kbit/s by default; the KLS-N needs the CAN option ("CAN bus is not included in KLS-N controller by default") [M] SOURCED. Whether Adam's unit has CAN is UNVERIFIED.

**Repo check (include/KellyKls.h, docs/KELLY_CAN.md):**
- IDs and the 0x0CF11E05 layout match.
- Corrected: the error field is 16 bits (ERR0-15), not "code u8 + bits u8".
- Corrected: motor current is an *unsigned* 0-400 A field, so it cannot show regen sign and **saturates at 400 A** while Adam's limit is 550 A (UNVERIFIED whether the 8080N scales differently). Use the LEMs for pack amps.
- Corrected: temp offsets are +40 (controller) and +30 (motor). The status and switch bytes are b4/b5, previously "TBD".
- `BMS_CAP_KELLY_RX` stays 0 until the candump matches.

## Does it accept commands?
No. The protocol defines only these two controller → instrument frames, and the manual calls the CAN "broadcast type" [M][C] (SOURCED). There is no enable, regen, limit or mode command over CAN. The ID is J1939-style (P3, PF 241, SA 05), and nothing documents an RX frame. Limits go through digital inputs (ECO, FWD/high-speed, Foot_SW, brake switch, Brake_AN).

## Inputs visible on CAN
| Input | On CAN? | Label |
|---|---|---|
| Throttle (pin 3) | yes, b0 0-5 V | SOURCED |
| Foot_SW, FWD, REV, Boost, 12 V brake switch | yes, b5 bits | SOURCED |
| Brake_AN analog (pin 2) | **no** | SOURCED (absent from [C]) |
| ECO / low-speed switch (pin 22) | **no** | SOURCED (absent) |
| Hall A/B/C | yes | SOURCED |

## Regen and brake (answers for Adam)
- **Analog brake input exists:** Brake_AN, pin 2, 0-5 V. "Boost and brake analog regen use the same port on Brake_AN(Pin2). When boost is disabled in user program, Pin2 is used for brake analog regen" [M p.~12] — SOURCED.
- **Proportional, not fixed:** Brake Type (item 20) "0: Switch regen mode. 1: 0-5K resistance pedal regen. 2: 0-5V hall active pedal regen." Brake Dead Low 5-40 % (default 20 % = 1 V) and Dead High 60-95 % (default 80 % = 4 V) [M] — SOURCED. That regen ramps linearly between them is our reading (the "variable regen" wording), **UNVERIFIED** curve shape.
- **Regen settings** [M], all SOURCED:
  - BRK_AD Brk 0-50 % (default 0 % = off): "Brake pedal regen's regen strength"
  - BRK_SW Brk 0-50 % (default 10 %): "Switch regen's regen strength"
  - RLS_TPS Brk 0-50 % (0 %): pedal (throttle) release regen
  - NTL Brk 0-50 % (0 %): neutral regen
  - Change Dir Brk 0-50 % (5 %): change-direction regen
  - Brk_Speed Limit 0-500 RPM: below it the Kelly exits regen
  - Brake Time / Brake Rls Time: ramps
  - **No separate "max regen current (A)" setting** in V2.11. What the % strengths are a percent *of* is UNVERIFIED (assumed motor-current max).
- **Brake switch:** 12 V brake switch on pin 25 [M] SOURCED. With Brake Type 0 it triggers fixed switch regen (BRK_SW Brk). Analog mode "doesn't need brake switch any more" (8080N-NPS manual) SOURCED. Brake H-Pedal (item 87), when checked: "press the brake and throttle at the same time, the controller will report a high pedal error to stop running" SOURCED. Whether the brake switch alone cuts throttle when unchecked is UNVERIFIED, so the sim/Teensy treat the brake switch as a throttle override and we recommend checking Brake H-Pedal.
- Boost must stay **unchecked** (item 91) or pin 2 becomes Boost (full throttle on 12 V!). SOURCED, critical config check.
- **Speed modes:** Three Speed (item 31) 0/1/2 = 1, 2 or 3 speed modes. ECO pin 22 = low-speed switch, FWD pin 12 can be the high-speed switch, and you can't use 3 gears and 3 speeds together by default (shared pin 12) [M] SOURCED. So **F/N/R (Three Gears) + Low cap on ECO** is the compatible combination: Low via ECO plus max speed, not all three tiers. Throttle shape: TPS Fwd MAP (midpoint %) is one fixed curve, not per mode.

## What goes where (recommendation)
| Job | Where | Why |
|---|---|---|
| Motor RPM, speed, Kelly V, motor/controller temps, Kelly errors, throttle readback, F/N/R and brake-switch state | **Kelly CAN** (single source of truth for telemetry) | free, already on CAN2 with the Elcon; no extra wires |
| Pack V/A, cell V, pack temps, charge/regen allowance | **BICMs + LEMs** (BMS) | Kelly current saturates at 400 A and has no sign; pack protection never trusts the Kelly |
| Park state (brake contacts) | **Teensy A + B direct wires** + Kelly Foot_SW NC pole | a safety input must not depend on a broadcast-only bus; CAN only adds RPM/neutral |
| Drive interlock with park | **Hardware** (NC pole → Foot_SW) | works with both Teensys dead |
| E-stop, contactors, precharge | **Hardware + Teensy A** | unchanged (docs/SAFETY.md) |
| Pedal maps / trim | **Teensys** (clamp) | the Kelly has one fixed curve and no CAN command |
| Low speed cap (Kid, limp, Teensys dead) | **Hardware relay on ECO**, Teensy A releases it | fail-safe (docs/DRIVE_MODES.md) |
| Brake pedal analog → regen | **Kelly Brake_AN direct**; Teensy reads the brake *switch* from CAN b5 bit3 and its own wire for the brake light | Brake_AN is not on CAN; tap it with a divider only if the dash needs %, PROPOSED |
| Regen inhibit (full / cold) | **Teensy OUT6** (pin 3) in the Brake_AN/brake-switch path | the Kelly can't take a CAN command |

**Wiring impact:**
- No new Teensy wires for RPM, F/N/R or the brake switch: they come from CAN.
- New: 2 park wires (pin 18 on A and B), 1 Foot_SW NC pole, and the ECO relay (OUT3 pin 20).
- Brake_AN stays pedal → Kelly direct.
