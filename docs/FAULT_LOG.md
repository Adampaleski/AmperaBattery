# Fault log that survives power-off

Status: **step 1, module + host checks only.** `include/FaultLog.h` is not called from the firmware yet. Nothing here drives an output, and every safety flag stays as before (`BMS_CAP_*` 0, `TELEMETRY_ONLY=1`, `CURRENT_SENSE_ENABLE=0`).

## What it stores

- [x] Fault set and fault clear edges, using the BmsCore fault bit numbers (`F_CELL_LOW` = 3, `F_OVERCURRENT` = 6, and so on).
- [x] Boot number, uptime ms, global sequence number, and one optional value per record (for example the lowest cell in mV or the trip current in A).
- [x] A "log limit reached" note when a boot hits its write limit.

## Where

- Teensy 4.1 emulated EEPROM (flash). Size 4284 bytes (`E2END 0x10BB`, read from the installed `framework-arduinoteensy` core).
- `Settings` blob stays at address 0 (under 32 bytes). The log uses addresses 256 to 1215: 48 records of 20 bytes.
- No header block. Each record has a sequence number and a CRC-16, so boot finds the newest record by scanning. A record cut off by power loss fails its CRC and is skipped; the other records survive. Blank flash reads as an empty log.

## Flash wear limits

- The same fault and edge type is written at most once per second.
- At most 24 records per power-on, the last being the "log limit reached" note.
- Teensy's EEPROM emulation spreads writes across its flash sectors. The real write endurance of that flash is **UNVERIFIED** here, so these limits stay until it is checked.

## Host checks

`scripts/test_fault_log.cpp`, 10 checks: blank EEPROM, survive power-off, Settings area untouched, wrap at 48, torn record skipped, sequence continues after a torn record, flapping fault rate limit, per-boot limit, bounded byte writes, never writes outside its range.

## Next steps

- [ ] Wire into BmsApp: `begin()` at boot with `EEPROM.read`/`EEPROM.update`, `edges(prevFaults, faults, ms, value)` each tick.
- [ ] Serial command to print the log (newest first) and to clear it (explicit command only).
- [x] Show the log in the simulator: Faults tab "Fault log" card (2026-10-20 10 PM). The real FaultLog runs in wasm on a simulated 4284-byte EEPROM that survives Reset, Power cycle and page reload (localStorage). Buttons: Power cycle, Pull power mid-write (torn record, skipped by CRC), Clear log. Values: cell mV for cell-low/high/charge-OV, amps for overcurrent. 6 faultlog-page wasm checks.
- [ ] Show the log in STATUS_SCHEMA for the hub.
- [ ] Save SoC used Ah + anchor at power-off in a separate record (backlog item 3).
- [ ] Bench check on a real Teensy: power-pull during a write, confirm the next boot reads the log.

## Derates are logged too (2026-10-21)
Adam asked that every power cut or derate be logged as a fault, with its reason. BmsCore now turns the Kelly limiter into debounced derate events:
- An event starts when a cutback (allowance under 100 %) holds for `derateLogMs` (100 ms). The RAM event log gets `DERATE <reason>: power N%`.
- If the reason changes mid-event, it logs `DERATE <new> (was <old>)`. That is still the same event.
- An event clears after `derateClearMs` (1 s) at full power, or with the contactors open. The log shows `derate cleared: <reason> (lowest N%)`.
- Reason codes (`BmsCore::DerateCode`, stored in records, never renumber): 1 low cell, 2 pack sag, 3 cell sag, 4 pack hot, 5 LEM imbalance, 6 discharge amps.
- FaultLog record kinds 4 (DerateSet, value = power % at start) and 5 (DerateClear, value = lowest %).
- **Derates are capped at 12 records per boot** (`maxDeratePerBoot`), and drops beyond that are counted in RAM. This keeps repeated launch derates from using up the 24-per-boot budget that real faults need.
- Outputs `derateCode`, `lastDerate`, `derateMinPct` and `derateEvents` are for the dash and STATUS_SCHEMA. They are shown in the simulator (Drive tab line, Faults tab Fault log card). The live reason already reaches the dash as `powerWhy`.

All debounce and cap values are PLACEHOLDER and adjustable. Firmware still does not write FaultLog (BmsApp wiring is next).

## Firmware wiring (2026-10-21 10 PM)

`src/FaultLogRun.{h,cpp}` runs this same `include/FaultLog.h` on the Teensy
emulated EEPROM (`EEPROM.read` / `EEPROM.update`, bytes 256..1215). Dual-string
role A only (where BmsCore runs); every other env compiles to no-ops.

- `begin()` after `Settings::begin()` and `BmsApp::begin()`: scans the log, starts a new boot number, prints the record count.
- `tick()` after `BmsApp::tick()` each loop: BmsCore active-fault-word edges (set/clear) and debounced derate edges, same rules as the simulator (cell mV for CELL-HIGH/CELL-LOW/CHARGE-OV, pack amps for OVERCURRENT when the current sensor is valid; derate set = power %, clear = lowest %).
- Serial: `f` prints the log newest first; `FCLEAR` + Enter erases it (anything else starting with `F` erases nothing).
- No pins, no outputs; it only writes EEPROM on edges, inside FaultLog's rate limits (1 s per edge, 24 per boot, 12 derates).
- Note: with TEMPS_VERIFIED=0, TEMPS-NOT-VERIFIED is active from boot, so each boot logs one SET for it (one 20-byte record).
- NOT bench-tested. Still to do: power-pull test on a bench Teensy, STATUS_SCHEMA field for the hub.

## Hub summary (2026-10-22, STATUS_SCHEMA v1 optional key)

Every hub frame (Serial4 JSON, USB `j`) now carries a read-only summary of the saved log, so the ESP32 dash can show "N saved faults, last: …" without reading EEPROM:

```json
"faultLog":{"available":true,"count":2,"slots":48,"boot":1,"limited":false,"deratesDropped":0,
            "last":{"kind":"derate","name":"low cell","boot":1,"sec":20.0,"value":62,"unit":"%"}}
```

- `available` false on every env except dual-string role A (where BmsCore runs); then no other fields.
- `last` = newest record or `null`. `kind` is `set`, `clear`, `derate`, `derate-end` or `note`. `value`/`unit` follow the `f` print rules: cell `mV` for CELL-HIGH/CELL-LOW/CHARGE-OV, `A` for OVERCURRENT, `%` for derates (power % at start, lowest % at end), otherwise `null`.
- `sec` is uptime in that boot (no RTC). Additive only; clients must tolerate its absence. Not yet documented in teryx-ev-hub docs/STATUS_SCHEMA.md and not shown by the hub UI.
- Host-checked: JSON valid for empty / fault / derate cases. Not bench-tested.
