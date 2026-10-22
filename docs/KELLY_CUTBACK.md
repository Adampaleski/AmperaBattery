# Kelly power and regen cutback

The BMS asks the Kelly KLS for less power before anything gets bad enough to open the contactors. Cutback is the soft layer; opening the EV200 mains is still the hard stop.

The KLS CAN protocol is broadcast-only (`0x0CF11E05`, `0x0CF11F05`). The controller accepts no CAN commands, so the BMS cannot set a current limit over CAN. Every cutback goes through the Kelly's digital inputs and throttle line.

## What the firmware computes (`BmsCore::kellyLogic`, Teensy A)

| Limiter | Starts | Reaches 0 % | Config |
|---|---|---|---|
| Low cell | weakest cell below `lowCutV + derateLowV` (3.45 V) | `lowCutV` (3.30 V), where the contactors open | `derateLowV = 0.15` |
| Pack hot | hottest sensor above `derateTempC` (50 °C) | `overTempC` (55 °C), where the contactors open | |
| Discharge amps | **off** (`maxDischargeA = 0`, 2026-10-20) | — | Kelly limits 550 A; the BMS does not cap current in normal driving. Setting `maxDischargeA > 0` re-enables allowance = max / I. |

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

## Decision (Adam, 2026-10-20)

Trips and the derate removal are in `BmsCore` as of 2026-10-20 (sim-tested, not on hardware). The imbalance and cell-sag derates were added the same day (sim-tested, see below). The decision:

- Kelly settings: phase current 550 A, battery current 550 A. The BMS does not cap current in normal driving.
- Hard trip (opens contactors): total LEM A + LEM B above **650 A for 100 ms**, or above **750 A instantly**.
- Derates: LEM imbalance |A − B| > 60 A for 1 s (crew 2026-10-21, was 100 A; soft 50 % derate, never the Low cap); cell sag relative to the pack average (2026-10-21, see below); heat derate coded but **off until `TEMPS_VERIFIED`**.
- String fuses: 4× Mersen A6T200 200 A Class T — final, bought 2026-10-20.
- Mains: two used TE EV200 (pack + and −). Volt AEV14012s only on charger/DC-DC/precharge.

The low-cell and pack-hot rows above show current code values, not these targets.

### Derates in code (2026-10-20, sim only)

- **LEM imbalance:** `imbalanceLogic()` in `BmsCore`. With the mains closed and both LEM halves valid, |LEM A − LEM B| above `imbalanceA = 100` A continuously for `imbalanceMs = 1000` ms sets `lemImbalance`, logs `LEM imbalance: A … / B … A`, and caps the allowance at `imbalancePct`. It clears as soon as the difference drops back under 100 A. A dip under 100 A restarts the 1 s timer. Contactors stay closed (warn + derate only).
  - `imbalancePct = 50` is a **placeholder**. Adam set the trigger, not how far to cut. Needs his number.
- **Cell sag (Adam, 2026-10-21): relative, not a fixed per-cell voltage.** Code in `BmsCore` (`cellStats()`, `cellLowLogic()`, `kellyLogic()`), sim-tested only. Every number is a **PLACEHOLDER** in `Config` and adjustable; none is a Chevy/LG spec.
  - *Relative cut (any load):* weakest cell more than `relSagStartV = 0.15` V below the pack-average cell scales the allowance linearly to 0 % at `relSagZeroV = 0.40` V below. Limiter `cell sag`. Contactors stay closed.
  - *Whole-pack sag vs load:* under load (`|I| > restA = 20` A with a valid current reading) a uniform sag does not use the low cut. Only the pack average near the floor tapers power (`hardFloorV + packSagBandV` = 3.10 V down to `hardFloorV` = 3.00 V, limiter `pack sag`).
  - *Hard floor:* any cell below `hardFloorV = 3.00` V for `hardFloorMs = 500` ms opens the mains (`CELL-LOW`, log says `floor`).
  - *At rest* (or no current reading, treated as rest): the old low-cell taper (`lowCutV + 0.15` to `lowCutV`, limiter `low cell`) and `lowCutV = 3.30` V now debounced: a cell must stay under it for `lowCutMs = 1000` ms before `CELL-LOW`. The old instant 3.30 V trip is gone.
  - Replaces `sagStartV`/`sagZeroV` (3.2 to 3.0 V), which could never act behind the instant 3.30 V trip. That conflict is resolved.
- **Heat:** unchanged, only acts when temps are valid (`TEMPS_VERIFIED`).
- Host checks: 12 derate checks in `scripts/sim_bms.cpp`: 6 imbalance (even 550 A split, 0.9 s vs 1.1 s at 130 A apart, clear, 90 A apart for 5 s, timer restart) and 6 sag (uniform 3.15 V at 550 A for 3 s = closed, full power; uniform 3.05 V at 550 A = pack sag 50 %; uniform 3.15 V at rest opens at 1 s, not 0.9 s; one cell 0.25 V low at 400 A = cell sag ~60 %; all 2.95 V at 550 A opens at 0.5 s; one cell 3.20 V for 300 ms at 550 A = cut, no trip, recovers).

## Simulator

`tools/bms_sim.html`, Drive tab: live allowance, limiter, regen state, the three output lines, the combined low-cell + sag curve (what-if low-cut slider), the LEM imbalance chart (LEM split slider), the heat cutback curve, and the current-trip chart (Kelly 550 A, 650 A for 100 ms, 750 A instant; slider to 800 A) with the current operating point. The curves are drawn from the firmware's config, and the numbers come from the compiled BmsCore.
