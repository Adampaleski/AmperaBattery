# LEM health / dead-LEM detection

Status: **logic + host test only** (`include/LemHealth.h`, `scripts/test_lem_health.cpp`, 18/18). Not wired:
`CURRENT_SENSE_ENABLE=0`, and the split-LEM read (LEM A on Teensy A, LEM B on Teensy B) isn't built yet.
Backlog: CRITICAL "firmware dead-LEM detection" and "LEM zero offset", both required before first drive.

One `LemHealth::Monitor` per LEM DHAB S/44. Feed it the same pin volts `CurrentSense` reads (ch1, ch2, 5 V sense,
all through the 10k/18k divider). It uses `BmsCore::dhabAmps`, so the maths match the firmware and the simulator.

| Code | Condition (all PROPOSED) | Hold |
|---|---|---|
| RAIL | any output within 0.2 V of 0 V or Vc, or Vc outside 4.5–5.5 V (dead, unplugged, lost 5 V) | 100 ms |
| RANGE | \|I\| > 450 A | 100 ms |
| AGREE | high channel < 15 A and low vs high channel differ by > 5 A | 500 ms |
| ZERO | key-on self-test with mains open reads \|I\| > 5 A | one sample |

- A fault latches. `hardFault()` is meant to engage the 10 mph (Kid/Low) cap. It never closes anything.
- `reset()` (key cycle) is refused while the fault is still present.
- The stored zero still comes from `Z` (`CurrentSense::calibrateZero`, EEPROM Settings). ZERO only checks that the zero is still good.

Bench checks before relying on it (UNVERIFIED):
- A live output clamps at 0.25 / 4.75 V (datasheet numbers as written in `BmsCore.h`), only 50 mV inside the 0.2 V rail band.
  Check divider tolerance and ADC noise at a hard ch1 clamp (> 20 A) so RAIL doesn't false-trip.
- The 5 A AGREE tolerance is about 31 mV on ch2 (6.15 mV/A). Check real ch1/ch2 spread below 15 A.
- ch2 clamps near −460 A on the discharge side, so 450 A RANGE sits just inside that.

Next: run one Monitor per Teensy inside `CurrentSense::tick`, send B's code over the A–B link, set BmsCore `lemAValid` / `lemBValid`, and run the ZERO self-test before precharge.
