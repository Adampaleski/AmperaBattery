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
