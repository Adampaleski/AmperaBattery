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

## In the simulator (tools/bms_sim.html, Settings tab)

The **BMS thresholds** card lists all 20 keys with value, default, allowed range,
"lower only" / "raise only" (tighten-only), SET and PLACEHOLDER tags. Type a value and
press Enter to apply; **Reset** puts one key back to the code default, **Reset all…**
is `TX`.

- Validation is not a JS copy: `sim/wasm_api.cpp` runs the real `Thresholds.h`
  (`sim_thr_set` builds the serial line `T<key> <v>` and calls `handleLine`), so the
  page shows the firmware's own message, e.g. `REFUSED: ocA=700 outside 100-650 A`
  or `REFUSED: instA=550 breaks rule: ocA < instA`. A refused loosening of a trip or
  temperature cut also says it is tighten-only.
- Applied values go into the simulated BmsCore at once, the same way
  `configureCore()` does it (code defaults, then the overrides), so the Drive tab
  charts and the fault scenarios you trigger by hand use them.
- Stored on the simulated EEPROM at 1216–1305 (same layout + CRC as the Teensy)
  and mirrored to this browser's localStorage (`teryx.thr.v1`), so they survive
  Reset, Power cycle and page reload. The scripted Scenarios tab always runs code
  defaults.
- The order rule `floorV < L` is checked against the live low cutoff. If you later
  move L below floorV (what-if slider / Settings), the card shows a warning; the
  firmware behaves the same (rules are checked when a T value is set).
- The Faults tab "Active & latched" card and the dash Faults & log section show
  derate now, last derate (reason + lowest power %) and derates this boot.

Wasm checks: 8 `thresholds-page` checks in `scripts/check_bms_sim_wasm.js` (in-bounds
set applies live and 620 A then trips at ocA 600, out of bounds refused, loosening
refused, order rules refused, floorV vs L, reset one + all, reload from saved EEPROM
and corrupt = defaults). UI: 8 more checks in `scripts/test_dash_ui.py`.

Also fixed: `P80`, `Z` and `I120` were never starting a settings line (only
V/L/W/X/U did), so `P80` printed the contactor status instead of saving. P, Z, I and
T now start a line.
