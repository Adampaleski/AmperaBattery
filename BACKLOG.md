# Backlog

## Priority list (standing dev routine)

1. [x] Tesla-style restyle + live Diagram tab in tools/bms_sim.html (034a3ae).
2. [ ] Kelly power/regen cutback wiring design + simulator controls.
   - [x] 2026-10-19 7 PM: Drive tab (allowance, limiter, regen, OUT3/OUT5/OUT6 lines, three cutback curves, sliders), BmsCore kellyWhy/regenWhy, 6 Kelly cutback checks in sim_bms, docs/KELLY_CUTBACK.md.
   - [x] DECISION (Adam, 2026-10-20): hard trip at 650 A for 100 ms or 750 A instantly on LEM A + LEM B, replacing 300 A/500 ms and the 250 A derate. Kelly phase + battery limits 550 A.
   - [x] 2026-10-20 7 AM: trip in BmsCore (overcurrentA 650 / 100 ms, new instantTripA 750, logs `OC instant`); 250 A discharge derate off (maxDischargeA 0). Scenarios: 700 A trips at 110 ms, 800 A spike trips in one 10 ms step (18/18). 6 boundary checks (600 A 3 s, -640 A, 700 A 80 ms, 745 A 90 ms stay closed). Drive tab: current-trip chart with 550/650/750 lines, sliders to 800 A. Sim only; CURRENT_SENSE_ENABLE 0.
   - [x] 2026-10-20 10 AM: derates in BmsCore: LEM imbalance |A − B| > 100 A for 1 s → warn + cap at imbalancePct (50 %, PLACEHOLDER), clears under 100 A; cell sag 3.2 → 3.0 V linear ("cell sag"); heat unchanged (only with valid temps). Inputs lemSplitValid/lemA/lemB, outputs lemImbalance/lemDiffA. 9 derate checks. Drive tab: LEM split slider, what-if low-cut slider, combined low-cell + sag chart, LEM imbalance chart. Sim only.
   - [ ] DECISION (Adam): 3.30 V low cut trips instantly, so the 3.2 → 3.0 V sag derate can never act and a launch sag under 3.30 V opens the mains. Keep, or make the low cut resting/debounced with a lower instant floor? Also pick the imbalance cut level (placeholder 50 %).
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
6. [ ] Tesla-style dash on the ESP32 hub (teryx-ev-hub, never main).
   - [x] 2026-10-20 (parent chat, 43a7c0c + c76fee2): standalone tools/dash.html + sim/dash.js (Digital/Analog/Minimal/Night, "—" + amber when no source, range/Wh/mi/trip, tell-tales, seat heater buttons, cooling PROPOSED). Simulator-side only; ESP32 hub port still to do.

## Other

- [ ] TODO (current sense sign): DHAB S/44 to be mounted so discharge reads in the −420 A direction, with regen/charge on the +230 A side. Firmware must flip the sign so discharge shows positive. Not done yet; CURRENT_SENSE_ENABLE stays 0. Range unverified until we have the datasheet. If it can't be mounted that way, fall back to a ≥500 A single-range LEM (e.g. HASS 500-S).
- [ ] (Optional) IVT-S 1000 A integration — dual LEM A + B is the primary sensing; ISA IVT-S is optional only: decide own CAN bus vs reconfigure to 250k; parser + DHAB cross-check (sim scenario 14 models it).
- [ ] Split-LEM firmware: read LEM A on Teensy A and LEM B on Teensy B, sign flip (discharge on −420 A side), share B's reading over the A–B link, port sim lemPair() pair-imbalance and sensor-fault checks into BmsCore. Thresholds (30%, 40 A, 100 A) are placeholders.
- [ ] HV path (2026-10-20, see docs/TERYX_MAIN_POWER_PATH.md): two main contactors, used TE EV200 on pack + and pack −. Volt AEV14012s only on charger/DC-DC/precharge branches.
- [ ] String fuses: 4× Mersen A6T200 200 A Class T, planned; size to be confirmed vs 650 A trip (Sparky).
- [ ] Strings: 36S each, built from 24S + 18S BICM boards (no 30S).
