# Factory-bus build (`teensy41_factory_bus`)

Up to four factory-strapped Volt BICMs (module 1–4) on **one** CAN3 bus at 125 k.
Monitor only: `TELEMETRY_ONLY=1`, and all `BMS_CAP_*` flags are 0.

```bash
source /tmp/piov/bin/activate
pio run -e teensy41_factory_bus            # build
pio run -e teensy41_factory_bus -t upload  # flash (Adam's Mac)
```

## Map and flags (`include/FactoryCellMap.h`)
- `kFactoryMapM1..M4`: pack cell number for each channel. 0 means not wired.
- `FACTORY_PACK_CELLS`: must equal `PACK_S_CELLS` in platformio.ini.
- `FACTORY_SERIES_STRING`: 1 only when every mapped cell is in one series string. 0 (bench) means no pack total and no charging.
- `FACTORY_STRING_CELLS`: the charger voltage basis. It's 0 unless the map is one series string.
- `FACTORY_EXPECT_LIVE_Mn`: live-cell count for the LOW-LIVE diagnostic. Default -1 means derive it from the map.
- `FACTORY_BALANCE_MAP_VERIFIED 0`: balance is off on this build.
- `TEMPS_VERIFIED` (Config.h, 0): charging is refused until temps are verified and limits are enforced.
- `ELCON_MIN_V` / `ELCON_MAX_V` (Config.h, 100/180): charger range check.

## What `d` shows
For each module: present/absent, wired, ok, `live=x/expected`, voltage, frame counts, and every channel.
A line such as `M2 LOW-LIVE: 8/12 live cells expected; frames 0x465 0x475 missing - check plug/CAN`
is **report only**. It doesn't trip any alarm or fault. After that comes the temperature raw bytes with two UNVERIFIED guesses.

## Hub JSON additions (key `j`, Serial4 ~1 Hz)
`lastReset`, `bms.modules[].live/expected/lowLive`, `bms.temps{verified,raw[]}`,
`charge.refuseReason/stringCells/faultLatched`. `packV` stays null when there's no series string.

## Bench checklist (before any flag flips)
1. **Cells:** `d` on every module. All expected channels live, with no LOW-LIVE line.
2. **Stale:** pull BICM CAN. Within about 3 s the cells alarm and stable drops to N.
3. **Watchdog stall test:** flash a test build that adds `while(1){}` behind a key. After about 2 s the Teensy resets, boot prints `Last reset: *** WATCHDOG ***`, and the coil pins stay low throughout (check with a meter or LED).
4. **Temp warm-brick test:** warm one module (heat gun on low or a warm brick, under 45 °C) and watch `d`. Note which ID/bytes move and which guess (A/B) matches a thermometer. Repeat for each sensor.
5. **Elcon test (Sparky):** charger into the pack or a dummy load with a **1 A** request at a voltage a few volts **under** the pack voltage. Confirm the charger obeys, that a stop frame (byte4=1) stops it, and that it shuts off by itself within about 5 s when frames stop (unplug the Teensy).
6. **Balance:** with a verified map only, one bit at a time, and a meter on the cell.
7. **Contactors:** lamp or relay board in place of the coils, and the HV divider for precharge.

## Balance (bleed) plan — PRINT-ONLY (Oct 19 2026)

- Map: `include/FactoryBalanceMap.h`, from Tom de Bree's VoltBMSV2
  (`BMSModuleManager.cpp` 108-198, MIT) + swoozle's diyelectriccar table.
  0x300 (8 bytes) = circuits 0x460-0x466 + 0x468; 0x310 (5 bytes) = 0x469,
  0x46A, 0x46C-0x46E. 8-cell circuits use bits 0-7, 6-cell circuits bits 0-5.
  Trigger = 0x200 `02 00 00`; frame set every 200 ms.
- Planner: `include/FactoryBalancePlan.h`. Odd board channels / even channels /
  odd / even / blank sample cycle; never adjacent cells, never a whole circuit.
  Deadband = `W`, no plan below low cutoff `L`, bleed floor 3.90 V.
- `b` prints the plan per board and the exact `WOULD TX` frames. Nothing is
  sent: `BMS_CAP_BALANCE_TX=0`, `FACTORY_BALANCE_MAP_VERIFIED=0`, and the
  factory path in `BalanceTx::sendQueuedBeforeKeepAlive()` returns early.
- PC test: `g++ -std=c++17 -Iinclude scripts/test_factory_balance.cpp -o tfb && ./tfb`
  (cross-checks the encoder byte-for-byte against a transcription of Tom's code).
- **Bus constraint:** IDs are fixed by the board straps, so each CAN bus can hold
  at most one M1, one M2, one M3 and one M4. A second M4 18S (or second M2/M3)
  must be on its own bus (CAN3 vs CAN1, or the other Teensy).
- Before any live bleed: one-cell bench test (one cell, 10 s, watch it drop on `d`).
