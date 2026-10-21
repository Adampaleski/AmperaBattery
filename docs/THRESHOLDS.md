# Adjustable BMS thresholds (USB serial)

Adam 2026-10-21: every BMS threshold should be adjustable. `include/Thresholds.h`
holds the table; `Settings.cpp` stores it; `ContactorSeq::configureCore()` applies it
to `BmsCore::Config` every tick (dual role A). Nothing here drives an output.

| Command | Effect |
|---|---|
| `T` + Enter | list every key: value, default, allowed range, SET / PLACEHOLDER |
| `TocA 600` + Enter | set one (refused outside bounds or if it breaks an order rule) |
| `TocA -` + Enter | that key back to the code default |
| `TX` + Enter | all keys back to code defaults |

Keys: `ocA ocMs instA` (current trip), `tripC derateC chgMaxC chgMinC chgColdC`
(temps), `lowMs restA sagStart sagZero floorV floorMs packBand` (cells / sag),
`imbA imbMs imbPct` (LEM A/B imbalance), `drLogMs drClrMs` (derate logging).

Rules
- Bounds are software sanity limits, not part specs.
- Adam's decided trips (650 A / 100 ms, 750 A instant) and the temperature cuts can
  only be tightened over serial. Loosening needs a code change.
- Order rules: `ocA < instA`, `derateC < tripC`, `chgMinC < chgMaxC`,
  `chgMinC <= chgColdC <= chgMaxC`, `sagStart < sagZero`, `floorV < L` (low cutoff).
- Defaults marked PLACEHOLDER are not Chevy/LG values. Temperature defaults still
  wait on the "more conservative of Chevy or EV community" check (backlog).

Storage: EEPROM 1216..1305 (right after the FaultLog ring 256..1215), magic `THR1`,
override bitmask, 20 floats, CRC-16. Blank or corrupt = no overrides.

Checks: `scripts/test_thresholds.cpp` (9). Not bench-tested on a Teensy yet.

Also fixed: `P80`, `Z` and `I120` were never starting a settings line (only
V/L/W/X/U did), so `P80` printed the contactor status instead of saving. P, Z, I and
T now start a line.
