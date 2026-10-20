# Backlog

## Priority list (standing dev routine)

1. [x] Tesla-style restyle + live Diagram tab in tools/bms_sim.html (034a3ae).
2. [ ] Kelly power/regen cutback wiring design + simulator controls.
   - [x] 2026-10-19 7 PM: Drive tab (allowance, limiter, regen, OUT3/OUT5/OUT6 lines, three cutback curves, sliders), BmsCore kellyWhy/regenWhy, 6 Kelly cutback checks in sim_bms, docs/KELLY_CUTBACK.md.
   - [ ] DECISION (Adam): overcurrent trip is 300 A for 500 ms and derate starts at 250 A, but the expected Kelly peak is ~550 A. Set real trip/derate from cell + fuse limits.
   - [ ] Derate hardware: pick CD4066 vs MCP4131 and RC values; confirm KLS low-speed and regen input pins/polarity from Adam's harness (UNVERIFIED).
3. [ ] State of charge from the split Volt LEM DHAB S/44 pair.
   - [x] 2026-10-19 10 PM: include/SocEstimator.h coulomb counter (LEM A + B sum, + = discharge), full-charge anchor from Core "at charge limit" (P<100 tagged not true 100 %), restore hook, 2 s current-loss -> unknown, low-cut drift flag, learnCapacity(); 10 SoC host checks; docs/SOC.md. No OCV table (none verified).
   - [ ] Bank capacity Ah UNVERIFIED (capAh 0 = unknown): measure with one full -> low-cut run, then learnCapacity().
   - [ ] Wire into BmsApp + STATUS_SCHEMA socPct (needs CURRENT_SENSE_ENABLE + split-LEM read); add SoC readout to bms_sim.html.
   - [ ] Persist used Ah + anchor at power-off (ties to item 5).
4. [ ] Full drive-cycle mode in the simulator.
5. [ ] Fault log that survives power-off.
6. [ ] Tesla-style dash on the ESP32 hub (teryx-ev-hub, never main).

## Other

- [ ] TODO (current sense sign): DHAB S/44 to be mounted so discharge reads in the −420 A direction, with regen/charge on the +230 A side. Firmware must flip the sign so discharge shows positive. Not done yet; CURRENT_SENSE_ENABLE stays 0. Range unverified until we have the datasheet. If it can't be mounted that way, fall back to a ≥500 A single-range LEM (e.g. HASS 500-S).
- [ ] IVT-S 1000 A integration: decide own CAN bus vs reconfigure to 250k; parser + DHAB cross-check (sim scenario 14 models it).
- [ ] Split-LEM firmware: read LEM A on Teensy A and LEM B on Teensy B, sign flip (discharge on −420 A side), share B's reading over the A–B link, port sim lemPair() pair-imbalance and sensor-fault checks into BmsCore. Thresholds (30%, 40 A, 100 A) are placeholders.
