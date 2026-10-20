# Kelly power and regen cutback

The BMS asks the Kelly KLS for less power before anything gets bad enough to open the contactors. Cutback is the soft layer; opening the EV200 mains is still the hard stop.

The KLS CAN protocol is broadcast-only (`0x0CF11E05`, `0x0CF11F05`). The controller accepts no CAN commands, so the BMS cannot set a current limit over CAN. Every cutback goes through the Kelly's digital inputs and throttle line.

## What the firmware computes (`BmsCore::kellyLogic`, Teensy A)

| Limiter | Starts | Reaches 0 % | Config |
|---|---|---|---|
| Low cell | weakest cell below `lowCutV + derateLowV` (3.45 V) | `lowCutV` (3.30 V), where the contactors open | `derateLowV = 0.15` |
| Pack hot | hottest sensor above `derateTempC` (50 °C) | `overTempC` (55 °C), where the contactors open | |
| Discharge amps | pack current above `maxDischargeA` (250 A) | never 0; allowance = 250 / I | `maxDischargeA = 250` |

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

## Open decision

`overcurrentA = 300 A` for 500 ms opens the contactors, and derate starts at 250 A. Adam's expected Kelly peak in the 36S build is about 550 A (four 150 A Class T fuses, about 137 A per string at 550 A). As configured, a full-throttle pull past 300 A for half a second would drop the pack. The trip and derate levels need to be set from the real cell and fuse limits before driving.

## Simulator

`tools/bms_sim.html`, Drive tab: live allowance, limiter, regen state, the three output lines, and the three cutback curves with the current operating point. The curves are drawn from the firmware's config, and the numbers come from the compiled BmsCore.
