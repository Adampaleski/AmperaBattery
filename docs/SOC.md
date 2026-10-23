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
| Restore | Saved count read back after power-off | `include/SocStore.h`, wired into `src/SocRun.cpp` 2026-10-23 1 PM (host-tested, NOT bench-tested). State shows `restored` until the next anchor. |

## Losing / doubting the count

- Current invalid for more than 2 s while counting: SoC goes back to unknown.
- Min cell within 20 mV of the low cut while SoC still says above 10 %: drift flag (capacity or anchor wrong). No auto-correct.

## Capacity: UNVERIFIED

`capAh = 0` (unknown) by default, so the percent stays null and only Ah is counted. The bank Ah for 4 parallel 36S strings of Volt/Ampera modules is not verified. Plan: one P100 full charge, then a full discharge to low cut on the bench/vehicle, then `learnCapacity()` stores the counted Ah. Test checks use 100 Ah as a test value only.

## Not done yet

- Go live: CURRENT_SENSE_ENABLE=1 plus LEM B's amps over the A-B link (then `kHaveLemB` in src/SocRun.cpp), and a measured capAh. Until then `socPct` stays null.
- ~~Wire `include/SocStore.h` into SocRun.~~ Done 2026-10-23 1 PM, see "Firmware persistence" below.
- Real key-off / power-loss save: read KEY-ON (A:34) and add a 12 V power-loss detect, so the last save isn't up to 0.5 Ah / 60 s old after a hard power pull.

## Firmware persistence (2026-10-23 1 PM, role A dual only)

- Boot: `SocRun::begin()` loads the newest valid slot. Blank, bad CRC, out of range or a saved `known=0` = nothing restored, count stays UNKNOWN (fail closed). A learned capAh in the record is restored too; otherwise capAh stays 0 (UNVERIFIED).
- Running: `SocRun::tick()` passes the live count to SocStore every loop. Wear rules (PLACEHOLDER): write when it moved >= 0.5 Ah and >= 60 s since the last write, or when known/drift/capAh changed, max 120 writes per boot. Forced write on BmsCore's DRIVE/CHARGE -> IDLE edge (mains opening at key-off or unplug).
- A boot that never had a known count writes nothing (no wear).
- Losing current for more than 2 s makes the count UNKNOWN and that is saved as `known=0`, so a stale count can't come back after the next reboot. Today CURRENT_SENSE_ENABLE=0, so a restored count goes UNKNOWN 2 s after boot.
- Gap (UNVERIFIED): no KEY-ON pin and no power-loss detect are read yet, so a hard power pull keeps only the last periodic save. Ah used while the BMS is off are assumed zero (not measured).
- Serial `p` shows the EEPROM range, restored yes/no and saves this boot. Hub frame `soc` object gains `restored` and `saves`.
- Host checks: `scripts/test_soc_run.cpp` 14/14 (key-off save, reboot restore, loss saved as unknown, corrupt EEPROM, no wear on a never-known boot).
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

## Saving the count at power-off (2026-10-23, store only, not wired)

`include/SocStore.h` keeps the count in EEPROM 1469..1514, right after DriveStore. It uses two ping-pong slots, each with magic `SOC1`, a sequence number, used Ah, anchor %, learned capAh, the known and drift flags, and a CRC-16.

- Boot restores the newest valid slot. A blank slot, a bad CRC or an out-of-range value restores nothing, so the count stays UNKNOWN until the next full-charge anchor (fail closed).
- Power pulled mid-write only damages the slot being written, so the previous count comes back.
- When the count goes UNKNOWN (current lost), it writes known=0, so a stale count can never come back after a reboot.
- Wear (PLACEHOLDER): it writes on the key-off / power-loss edge, or after 0.5 Ah moved and at least 60 s since the last write, at most 120 writes per boot. Identical records are skipped.
- UNVERIFIED: it assumes no unmetered Ah flows while the BMS is off (self-discharge and 12 V draw not measured), and Teensy emulated-EEPROM endurance is unverified.
- 16 host checks in `scripts/test_soc_store.cpp`, part of `scripts/run_all_tests.sh`.

## LEM B over the link (2026-10-23 4 PM, frame only, NOT consumed)
Teensy B now sends LEM B on CAN2 frame 0x6A5 every 50 ms: [0-1] amps x10 (int16 LE, + = discharge), [2] bit0 valid, [7] heartbeat. Format + A-side rule in include/LemLink.h. A trusts it only when valid=1, the heartbeat advanced within the link stale limit (linkMs, 200 ms), and the value is in range; otherwise no LEM B (fail closed). With CURRENT_SENSE_ENABLE 0 B always sends valid=0 (a bench `I<amps>` dry-run on B would send valid=1; A still ignores it). `Link::remoteLemB()` exposes it and the link print shows `REMOTE LEM B`. SocRun kHaveLemB and ContactorSeq lemBValid are still false: wiring them is the next step. 12 host checks (scripts/test_lem_link.cpp). NOT bench-tested.
