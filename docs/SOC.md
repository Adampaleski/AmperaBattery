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

- Go live: CURRENT_SENSE_ENABLE=1 plus LEM B's amps over the A-B link (then `kHaveLemB` in src/SocRun.cpp), and a measured capAh. Until then `socPct` stays null.
- Persist used Ah + anchor at power-off (item 5).
- ~~Simulator page SoC readout.~~ Done 2026-10-20 (see below).

## Simulator page (sim only)

`sim/wasm_api.cpp` runs a `BmsCore::Soc` every 10 ms sim tick on LEM A + LEM B (+ = discharge), with the full anchor from Core `at charge limit`, the 2 s current-loss -> unknown rule and the low-cut drift flag. The Cluster tab Battery gauge shows the estimate, its state (unknown / anchored / counting), Ah used since the anchor, and a small "sim truth vs estimate" error line. Controls: Charge to limit (anchor), LEM dropout 3 s, cells drain with current.

- Capacity on the page: 180 Ah = 4 strings (SOURCED: Adam 2026-10-21, 36S × 4P) × 45 Ah per Volt Gen1 cell group (ASSUMED, 3P × 15 Ah). The real `capAh` stays 0 = unknown (UNVERIFIED).
- Sim truth: lowest cell on a SIM-ASSUMED linear map 3.30 V = 0 % -> 4.10 V = 100 %. The sim drains every cell along that map with load current (mains closed), so truth and estimate move together; the offset left after a P80 anchor (about -1.4 %) comes from the two linear maps not matching. It is not a measured error.
- Checks: `node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm` (8 "socpage" checks).

## Firmware wiring (2026-10-22, not bench-tested)

`src/SocRun.{h,cpp}` runs the same `BmsCore::Soc` on Teensy A (dual string role A only; no-op elsewhere). Inputs each loop: `CurrentSense::valid()/amps()` (LEM A only), Core `chargeWhy == "at charge limit"` as the anchor, Settings P and L. Read-only: no pins, no EEPROM.

- USB `p` prints a `SOC` line (state, Ah used, percent or unknown, why).
- Hub frame: `bms.socPct` comes from SocRun but stays **null** while the count is half the bank (LEM B not shared, `halfBank:true`) or capAh is unknown. Optional key `"soc":{available,known,state,usedAh,pct,drift,why,halfBank,capAh}`; off role A `{"available":false}`.
- With CURRENT_SENSE_ENABLE=0 the count only runs on the bench `I<amps>` dry-run.
- Host check: `g++ -std=c++17 -DDUAL_STRING=1 -DTEENSY_ROLE=0 -Iscripts/hoststub -Iinclude -Isrc scripts/test_soc_run.cpp -o /tmp/tsr && /tmp/tsr` (5 checks).
