# Backlog

## Priority list (standing dev routine)

1. [x] Tesla-style restyle + live Diagram tab in tools/bms_sim.html (034a3ae).
2. [ ] Kelly power/regen cutback wiring design + simulator controls.
   - [x] 2026-10-19 7 PM: Drive tab (allowance, limiter, regen, OUT3/OUT5/OUT6 lines, three cutback curves, sliders), BmsCore kellyWhy/regenWhy, 6 Kelly cutback checks in sim_bms, docs/KELLY_CUTBACK.md.
   - [x] DECISION (Adam, 2026-10-20): hard trip at 650 A for 100 ms or 750 A instantly on LEM A + LEM B, replacing 300 A/500 ms and the 250 A derate. Kelly phase + battery limits 550 A.
   - [x] 2026-10-20 7 AM: trip in BmsCore (overcurrentA 650 / 100 ms, new instantTripA 750, logs `OC instant`); 250 A discharge derate off (maxDischargeA 0). Scenarios: 700 A trips at 110 ms, 800 A spike trips in one 10 ms step (18/18). 6 boundary checks (600 A 3 s, -640 A, 700 A 80 ms, 745 A 90 ms stay closed). Drive tab: current-trip chart with 550/650/750 lines, sliders to 800 A. Sim only; CURRENT_SENSE_ENABLE 0.
   - [x] 2026-10-20 10 AM: derates in BmsCore: LEM imbalance |A − B| > 100 A for 1 s → warn + cap at imbalancePct (50 %, PLACEHOLDER), clears under 100 A; cell sag 3.2 → 3.0 V linear ("cell sag"); heat unchanged (only with valid temps). Inputs lemSplitValid/lemA/lemB, outputs lemImbalance/lemDiffA. 9 derate checks. Drive tab: LEM split slider, what-if low-cut slider, combined low-cell + sag chart, LEM imbalance chart. Sim only.
   - [x] DECISION (Adam, 2026-10-21): cell sag is relative (weak cell vs pack average), whole-pack sag judged against load, only a low hard floor that trips if voltage stays down briefly.
   - [x] 2026-10-21 7 AM (b2ba63c): in BmsCore. Weakest cell 0.15 → 0.40 V below pack average = "cell sag" 100 → 0 % (any load). Under load (|I| > 20 A) a uniform sag skips the low cut; pack-average taper 3.10 → 3.00 V ("pack sag"). Hard floor 3.00 V for 500 ms → CELL-LOW. 3.30 V low cut only at rest, debounced 1 s (no instant fixed cut; no current reading = rest). All PLACEHOLDER in Config. Drive tab: relative-sag chart + pack-sag/rest chart. 12/12 derate checks; cell-low scenario now 1 s. Gist rev b5b0cfe8.
   - [ ] Adam/Sparky: confirm the placeholder numbers (0.15/0.40 V below avg, 3.00 V floor 500 ms, 1 s rest debounce, 20 A rest) against Volt cell data; pick the imbalance cut level (placeholder 50 %).
   - [x] 2026-10-21 10 AM: derate events. BmsCore DerateCode (low cell / pack sag / cell sag / pack hot / LEM imbalance / discharge amps), debounced events (hold 100 ms to log, 1 s full power to clear, reason changes logged; PLACEHOLDER), outputs derateCode/lastDerate/derateMinPct/derateEvents. FaultLog kinds 4/5 (DerateSet value = power %, DerateClear value = lowest %), derates capped 12/boot so faults keep room. Sim: Fault log card shows DERATE rows (amber), Drive tab shows count + last reason. 3 derate-event checks (15/15), 2 FaultLog host checks (12/12), 2 faultlog-page wasm checks (8/8). docs/FAULT_LOG.md. Firmware still doesn't write FaultLog. Commit "derates logged as events", gist rev a505e09a.
   - [x] 2026-10-21 1 PM: thresholds adjustable over serial. include/Thresholds.h: 20 keys (trip, temps, sag, LEM imbalance, derate logging), `T` list / `T<key> <v>` / `T<key> -` / `TX`, bounded, trips + temp cuts tighten-only, order rules, EEPROM 1216 after FaultLog with CRC. Applied in configureCore (role A). Fixed: P80/Z/I never started a settings line. 9 host checks (scripts/test_thresholds.cpp), docs/THRESHOLDS.md. Not bench-tested; sim page unchanged.
   - [x] 2026-10-21 4 PM (62db3e9): Simulator: same threshold table editable in bms_sim.html (wasm), so Adam can try values; dash "last derate" in the fault/trip panel. Settings tab "BMS thresholds" card: all 20 keys with value, default, allowed range, lower-only/raise-only, SET, PLACEHOLDER; type + Enter to apply, per-key Reset, Reset all (TX). Validation is the real include/Thresholds.h in wasm (sim_thr_set -> handleLine), refused edits show the firmware's reason (+ tighten-only note). Applied live into the sim BmsCore like configureCore, saved on simulated EEPROM 1216-1305 + localStorage (survives Reset/Power cycle/reload); warns if L is later moved below floorV. Faults tab + dash Faults & log: derate now, last derate (reason + lowest %), count this boot. 8 thresholds-page wasm checks, 8 UI checks (57/57). Firmware unchanged. Gist rev 036ca1aa.
   - [ ] Temperature limits: use the more conservative of Chevy's or the EV community's numbers (Adam 2026-10-21); ask Sparky.
   - [ ] Docs mismatch: TERYX_MAIN_POWER_PATH.md says heat 50 °C start / 60 °C cut; BmsCore has derateTempC 50, overTempC 55. Reconcile.
   - [ ] Derate hardware: pick CD4066 vs MCP4131 and RC values; confirm KLS low-speed and regen input pins/polarity from Adam's harness (UNVERIFIED).
3. [ ] State of charge from the split Volt LEM DHAB S/44 pair.
   - [x] 2026-10-19 10 PM: include/SocEstimator.h coulomb counter (LEM A + B sum, + = discharge), full-charge anchor from Core "at charge limit" (P<100 tagged not true 100 %), restore hook, 2 s current-loss -> unknown, low-cut drift flag, learnCapacity(); 10 SoC host checks; docs/SOC.md. No OCV table (none verified).
   - [ ] Bank capacity Ah UNVERIFIED (capAh 0 = unknown): measure with one full -> low-cut run, then learnCapacity().
   - [x] 2026-10-20 1 PM: simulator SoC readout. sim/wasm_api.cpp runs the real BmsCore::Soc on the sim's LEM A + B each 10 ms tick (anchor from Core "at charge limit", 2 s current loss -> unknown, low-cut drift flag). Cluster tab Battery gauge shows the estimate + state (unknown/anchored/counting) + Ah used, "unknown until full-charge anchor" when unanchored, and sim truth vs estimate error. Buttons: Charge to limit (anchor), LEM dropout 3 s; cell drain toggle. Capacity 180 Ah labelled SIM-ASSUMED (real capAh still UNVERIFIED). 8 socpage wasm checks. Firmware unchanged.
   - [ ] Wire into BmsApp + STATUS_SCHEMA socPct (needs CURRENT_SENSE_ENABLE + split-LEM read).
   - [ ] Persist used Ah + anchor at power-off (ties to item 5).
4. [ ] Full drive-cycle mode in the simulator.
   - [x] 2026-10-20 4 PM (10ba99e): Drive cycle card on the Cluster tab. sim/drive_cycles.js profiles (SIM PROFILES, not logged data): Trail loop 125 s, Hard launches 5x, Long climb 10 min. Plays throttle/brake/gear into the existing driver-input model on any tab, auto key-on, stops early if mains open; live step + summary (distance from toy speed model, Wh out/regen, Wh/mi, peak discharge/regen A vs 650/750 trip, lowest cell, lowest power allowed + why, SoC change). 5 drivecycle wasm checks; headless page run of Trail loop completes. Gist rev 0f905bd5.
   - [ ] Next: record a real Teryx drive (Kelly CAN speed/current) as a replayable profile; add a heat/temps-valid profile once temps are verified; cycle results into the dash trip panel.
   - Note: in the Hard launches run regen showed 0 A near full charge (BMS blocks regen near the charge ceiling, expected); trail loop regen hit 60 A.
5. [ ] Fault log that survives power-off.
   - [x] 2026-10-20 7 PM: include/FaultLog.h (header-only, not wired into firmware). 48 x 20-byte records at EEPROM 256..1215 (Settings stays at 0; Teensy 4.1 EEPROM 4284 B per installed core). Seq + CRC-16 per record, no header: torn write skipped, blank = empty. Fault set/clear edges with boot no., uptime ms, one value. Wear limits: same edge max 1/s, 24 records per boot then 'log limit reached'. 10 host checks (scripts/test_fault_log.cpp), ARM compile check. docs/FAULT_LOG.md. Flash endurance UNVERIFIED.
   - [x] 2026-10-20 10 PM: simulator Fault log card on the Faults tab. Real FaultLog.h in wasm on a simulated EEPROM that survives Reset, Power cycle and page reload (localStorage); Power cycle / Pull power mid-write (torn record skipped) / Clear log buttons; records newest first with boot, time, set/clear, value (cell V or amps). 6 faultlog-page wasm checks. Firmware unchanged. be2106d, gist rev 8c570165.
   - [ ] Next: wire into BmsApp (EEPROM.read/update, edges each tick), serial print/clear command, STATUS_SCHEMA, bench power-pull test.
6. [ ] Tesla-style dash on the ESP32 hub (teryx-ev-hub, never main).
   - [x] 2026-10-20 (parent chat, 43a7c0c + c76fee2): standalone tools/dash.html + sim/dash.js (Digital/Analog/Minimal/Night, "—" + amber when no source, range/Wh/mi/trip, tell-tales, seat heater buttons, cooling PROPOSED). Simulator-side only; ESP32 hub port still to do.

## Other

- [ ] TODO (current sense sign): DHAB S/44 to be mounted so discharge reads in the −420 A direction, with regen/charge on the +230 A side. Firmware must flip the sign so discharge shows positive. Not done yet; CURRENT_SENSE_ENABLE stays 0. Range unverified until we have the datasheet. If it can't be mounted that way, fall back to a ≥500 A single-range LEM (e.g. HASS 500-S).
- [x] 2026-10-21: charge mode separate from key-on (BmsCore Mode/plugIn/chargerPath/chargeStop, 5 plug scenarios, crew review pending), sim vehicle model (sim/vehicle.js: throttle/brake/grade, wheel speed, motor RPM, brake = regen up to PROPOSED 60 A, fade < ~3 mph), dash charging screen with sleep/wake.
- [ ] Pick a proximity / charger-present input pin and wire `plugIn` in ContactorSeq (crew).
- [ ] (Optional) IVT-S 1000 A integration — dual LEM A + B is the primary sensing; ISA IVT-S is optional only: decide own CAN bus vs reconfigure to 250k; parser + DHAB cross-check (sim scenario 14 models it).
- [ ] Split-LEM firmware: read LEM A on Teensy A and LEM B on Teensy B, sign flip (discharge on −420 A side), share B's reading over the A–B link, port sim lemPair() pair-imbalance and sensor-fault checks into BmsCore. Thresholds (30%, 40 A, 100 A) are placeholders.
- [ ] HV path (2026-10-20, see docs/TERYX_MAIN_POWER_PATH.md): two main contactors, used TE EV200 on pack + and pack −. Volt AEV14012s only on charger/DC-DC/precharge branches.
- [ ] String fuses: 4× Mersen A6T200 200 A Class T, planned; size to be confirmed vs 650 A trip (Sparky).
- [ ] Strings: 36S each, built from 24S + 18S BICM boards (no 30S).

## Safety gaps (crew 2026-10-21) — TODO, NOT BUILT
Pack protection today uses the **BICMs (cell V/temps) and LEMs (pack A) only**. The Kelly CAN is telemetry, not protection.
- [ ] **REQUIRED BEFORE FIRST DRIVE:** charge-plug interlock. No drive while plugged in, and no charging below 0 °C or with stale/unverified temps (TEMPS_VERIFIED=0 today).
- [ ] **REQUIRED BEFORE FIRST DRIVE:** contactor weld check. Bus voltage vs pack with mains commanded open at key-off and before precharge (sim has a welded-contactor toggle; firmware not built).
- [ ] **REQUIRED BEFORE FIRST DRIVE:** isolation monitoring (HV to chassis). Part not chosen.
- [ ] **REQUIRED BEFORE FIRST DRIVE:** 12 V low alarm at 11.5 V (PROPOSED). Needs a 12 V sense divider pin.
- [ ] Service disconnect (SOURCED: Mechanic 2026-10-21): the Volt MSD sits on the **common B+ bus after the 4 string fuses**. Procedure: **Step 1** pull the MSD (Kelly side safe). **Step 2** also pull all 4 Mersen A6T200 string fuses before working on the packs or the bus.
- [ ] **REQUIRED BEFORE FIRST DRIVE:** MSD rating ≈350 A (schematic/gen.py) is below the 550 A launch current. Verify against Volt specs or replace with a combined bus fuse.

## Mechanical (Mechanic 2026-10-21) — TODO
- [ ] **REQUIRED BEFORE FIRST DRIVE:** Chain guard + chain tensioner (#50 chain, 16/32T).
- [ ] **REQUIRED BEFORE FIRST DRIVE:** Pack retention / tie-downs (straps/brackets), HV cable clamps, grommets at every pass-through.
- [ ] Enclosure sealing with a breather and a drain.
- [ ] Chassis ground straps: 12 V negative to frame, Kelly case to frame.

## 2026-10-21 park / Kelly / Low cap (this commit, CREW REVIEW)
- [x] Clamp review fixes: A heartbeat pin 4 (not CS), 4066 on shared 5 V, Kelly pull-down 100k + bench curve recheck.
- [x] Park detection: ParkState.h + sim; Foot_SW NC hardware interlock documented (docs/PARK_BRAKE.md).
- [x] Kelly CAN/regen research: docs/KELLY_CAN_ROLES.md; sim analog Brake_AN regen.
- [x] Kelly ECO fail-safe Low cap (OUT3 inverted: HIGH = release); Kid + both-Teensys-dead sim.
- [ ] Next: B → A park contact bit on the link; wire ParkState into ContactorSeq (precharge gate) and Selector (Kid); bench the Foot_SW / ECO behaviour.

