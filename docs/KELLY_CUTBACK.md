# Kelly power and regen cutback

The BMS asks the Kelly KLS for less power before anything gets bad enough to open the contactors. Cutback is the soft layer; opening the EV200 mains is still the hard stop.

The KLS CAN protocol is broadcast-only (`0x0CF11E05`, `0x0CF11F05`). The controller accepts no CAN commands, so the BMS cannot set a current limit over CAN. Every cutback goes through the Kelly's digital inputs and throttle line.

## What the firmware computes (`BmsCore::kellyLogic`, Teensy A)

| Limiter | Starts | Reaches 0 % | Config |
|---|---|---|---|
| Low cell | weakest cell below `lowCutV + derateLowV` (3.45 V) | `lowCutV` (3.30 V), where the contactors open | `derateLowV = 0.15` |
| Pack hot | hottest sensor above `derateTempC` (50 °C) | `overTempC` (55 °C), where the contactors open | |
| Discharge amps | pack current above `maxDischargeA` (250 A) | never 0; allowance = 250 / I | `maxDischargeA = 250` (**superseded 2026-10-20**: no BMS current cap; Kelly limits 550 A. Code not yet changed.) |

The lowest of the three wins. `Outputs.kellyWhy` names it (`low cell`, `pack hot`, `discharge amps`, or empty for full power). With the contactors open or a drive fault active, allowance is 0 % and limp is on.

Regen is allowed unless the fullest cell is within 20 mV of 4.10 V (`cells full`) or the coldest sensor is below `chargeMinTempC` (0 °C, `pack cold`). `Outputs.regenWhy` says which.

All thresholds are placeholders until Adam signs them off. Temperatures are not decoded on the real pack yet.

## Outputs (Teensy A), all print-only today

| Teensy | Signal | Meaning |
|---|---|---|
| OUT3, pin 20 | LIMP | HIGH whenever allowance < 100 % |
| OUT5, pin 2 | DERATE PWM | `analogWrite` duty = allowance % × 2.55 |
| OUT6, pin 3 | REGEN OK | HIGH = regen allowed |

Pins are only written when `BMS_CAP_KELLY_LIMIT_DRV=1` and `TELEMETRY_ONLY=0`. Both stay off. Serial `y` prints what would be driven and which limiter is active.

## Wiring design (Kelly side UNVERIFIED)

1. **Limp:** OUT3 drives a PC817 opto (with a series resistor sized for the Teensy's 3.3 V output). The opto transistor pulls the KLS low-speed (speed switch) input to its return. Limp max speed is set in the Kelly config tool and limits speed, not battery amps. **UNVERIFIED:** KLS connector pin for the low-speed input, and whether it is active-low on Adam's unit.
2. **Derate:** OUT5 PWM through an RC low-pass to a DC level that scales the throttle wiper, using a CD4066 analog switch or an MCP4131 digital pot. **NOT CHOSEN:** part, RC values, and whether the Kelly throttle input tolerates the scaled signal. Until this exists, limp is the only active cutback path.
3. **Regen:** OUT6 HIGH turns on a PC817 placed in series with the KLS regen or brake-switch input, so a LOW (or a dead Teensy) inhibits regen. **UNVERIFIED:** which KLS input enables regen on this controller and its polarity.
4. **Fail-safe:** with the Teensy unpowered, all three outputs are LOW. That means no limp (full Kelly power) and no regen. The contactors are open in that case, so there is no traction power anyway.

## Decision (Adam, 2026-10-20) — firmware not yet updated

The code still has `overcurrentA = 300 A` for 500 ms and the 250 A derate. Both are replaced by:

- Kelly settings: phase current 550 A, battery current 550 A. The BMS does not cap current in normal driving.
- Hard trip (opens contactors): total LEM A + LEM B above **650 A for 100 ms**, or above **750 A instantly**.
- Derates: LEM imbalance |A − B| > 100 A for 1 s; cell sag starting at 3.2 V, reaching 0 % at 3.0 V; heat derate coded but **off until `TEMPS_VERIFIED`**.
- String fuses: 4× Mersen A6T200 200 A Class T, planned; size to be confirmed vs 650 A trip (Sparky).
- Mains: two used TE EV200 (pack + and −). Volt AEV14012s only on charger/DC-DC/precharge.

The low-cell and pack-hot rows above show current code values, not these targets.

## Simulator

`tools/bms_sim.html`, Drive tab: live allowance, limiter, regen state, the three output lines, and the three cutback curves with the current operating point. The curves are drawn from the firmware's config, and the numbers come from the compiled BmsCore.
