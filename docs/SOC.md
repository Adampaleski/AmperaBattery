# State of charge (coulomb count, split LEM pair)

Status: logic + host checks only. Not wired into the firmware loop; `CURRENT_SENSE_ENABLE` stays 0 and nothing here drives an output.

Code: `include/SocEstimator.h` (`BmsCore::Soc`), checks in `scripts/sim_bms.cpp` (10 SoC checks).

## How it works

- Input is the bank total current, + = discharge: LEM A (strings 1+2) + LEM B (strings 3+4).
- Ah used is integrated from the last anchor. Regen/charge counts back.
- Percent = anchor % − 100 × used Ah / capacity, clamped 0–100.

## Anchors (no voltage table)

There is no verified open-circuit-voltage curve for these cells, so SoC is **unknown at boot** and only becomes known at:

| Anchor | When | Notes |
|---|---|---|
| Full | Core reports `at charge limit` | P100 = 100 %. At P80 the point is tagged "not true 100 %" and shown as 80 % (linear-map assumption, UNVERIFIED). |
| Restore | Saved count read back after power-off | Needs backlog item 5 (fault/state log that survives power-off). |

## Losing / doubting the count

- Current invalid for more than 2 s while counting: SoC goes back to unknown.
- Min cell within 20 mV of the low cut while SoC still says above 10 %: drift flag (capacity or anchor wrong). No auto-correct.

## Capacity: UNVERIFIED

`capAh = 0` (unknown) by default, so the percent stays null and only Ah is counted. The bank Ah for 4 parallel 36S strings of Volt/Ampera modules is not verified. Plan: one P100 full charge, then a full discharge to low cut on the bench/vehicle, then `learnCapacity()` stores the counted Ah. Test checks use 100 Ah as a test value only.

## Not done yet

- Wire into `BmsApp` (needs CURRENT_SENSE_ENABLE and the split-LEM firmware read, see BACKLOG).
- `socPct` in STATUS_SCHEMA v1 (HubStatus prints null today).
- Persist used Ah + anchor at power-off (item 5).
- ~~Simulator page SoC readout.~~ Done 2026-10-20 (see below).

## Simulator page (sim only)

`sim/wasm_api.cpp` runs a `BmsCore::Soc` every 10 ms sim tick on LEM A + LEM B (+ = discharge), with the full anchor from Core `at charge limit`, the 2 s current-loss -> unknown rule and the low-cut drift flag. The Cluster tab Battery gauge shows the estimate, its state (unknown / anchored / counting), Ah used since the anchor, and a small "sim truth vs estimate" error line. Controls: Charge to limit (anchor), LEM dropout 3 s, cells drain with current.

- Capacity on the page: 180 Ah, SIM-ASSUMED (4 strings x 45 Ah, the same number the sim's range gauge already used). The real `capAh` stays 0 = unknown (UNVERIFIED).
- Sim truth: lowest cell on a SIM-ASSUMED linear map 3.30 V = 0 % -> 4.10 V = 100 %. The sim drains every cell along that map with load current (mains closed), so truth and estimate move together; the offset left after a P80 anchor (about -1.4 %) comes from the two linear maps not matching. It is not a measured error.
- Checks: `node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm` (8 "socpage" checks).
