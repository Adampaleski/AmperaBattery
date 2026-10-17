# Dual-string 36S 4P on two Teensy 4.1s (monitor / print-only)

Envs: `teensy41_dual_a` (TEENSY_ROLE=0, master) and `teensy41_dual_b` (TEENSY_ROLE=1).
All BMS_CAP flags 0, TELEMETRY_ONLY=1: contactors, charge and balance are PRINT-ONLY.

## Buses (each Teensy)
| Bus | Pins | Rate | Use |
|---|---|---|---|
| CAN3 | 30/31 | 125k 11-bit | string 1: M1 30S (6/8/8/8) + M4 18S bottom 6S (0x46C) |
| CAN1 | 22/23 | 125k 11-bit | string 2: M2 or M3 24S + M4 18S 12S (0x46D+0x46E default) |
| CAN2 | 0/1 | 250k 29-bit | Elcon + Kelly + inter-Teensy link |

Keep-alive 0x200 is sent on CAN3 and CAN1 (BICMs stop reporting without it).
Each bus needs its own transceiver (SN65HVD230) and 120 ohm termination at both ends.

## Pin map
| Function | Pin | Note |
|---|---|---|
| Precharge (OUT2) | 12 | unchanged |
| Main+ (OUT1) | 11 | unchanged |
| Main- (OUT4) | 21 | unchanged |
| OUT3 | 20 | unchanged |
| OUT5 | **2** | was 22 (now CAN1 TX) |
| OUT6 | **3** | was 23 (now CAN1 RX) |
| OUT7 / OUT8 | 5 / 6 | unchanged |
| B fault out | 4 | role B only, DISABLED (`B_FAULT_PIN_ENABLE=0`); HIGH = ok, LOW = fault |
| CAN1 TX/RX | 22/23 | string 2 |
| Serial4 (hub) | 16/17 | unchanged |

## String config (include/StringConfig.h)
`STRING1_CIRCUITS` / `STRING2_CIRCUITS` are circuit bitmasks (circuit numbers in that file).
M4 6S mode = one of `M4_C1/M4_C2/M4_C3`; 12S mode = two. Each string must total
`STRING_EXPECT_S` (36) or the build fails. Example: `-D STRING2_CIRCUITS="(M3_ALL|M4_C1|M4_C2)"`.
An expected circuit with no fresh frame for > `STRING_SILENT_MS` (1000 ms) = FAULT.

## Link IDs (CAN2, 29-bit extended)
| ID | Dir | Bytes |
|---|---|---|
| 0x6A0 | B->A | string 1: [0-1] V x100 LE, [2-3] min mV LE, [4-5] max mV LE, [6] faults, [7] heartbeat |
| 0x6A1 | B->A | string 2: same layout |
| 0x6A2 | B->A | [0] hb, [1] role=1, [2] s1 cells, [3] s2 cells, [4] faults OR, [5] bit0 fault pin enabled |
| 0x6A8 | A->B | [0] hb, [1] contactor state, [2] A faults OR |
0x6A3-0x6A7, 0x6A9-0x6AF reserved. Period 100 ms. A: heartbeat not advancing > 1 s = LINK-STALE fault.
No clash with Elcon 0x1806E5F4 / 0x18FF50E5 or Kelly 0x0CF11E05 / 0x0CF11F05.

Fault bits: 0x01 SILENT, 0x02 NO-DATA, 0x04 CELL-BAD, 0x08 LOW-CUTOFF, 0x10 OVER-MAX (4.25 V),
0x20 LINK-STALE, 0x40 COUNT, 0x80 info (extra circuit, not a fault).

## Serial
Single keys as before (`d` now adds STRING / REMOTE / LINK lines; `b` adds the balance plan).
Settings lines (type, then Enter; stored in EEPROM, printed at boot):
- `V4.05` charge ceiling V/cell, 3.90-4.15 (default 4.05); charge fault latch = ceiling + 0.05 V
- `L3.30` low cutoff V/cell, 3.00-3.50 (default 3.30); below = string FAULT
- `W10` balance deadband mV, 2-50 (default 10)
- `X` restore defaults
- `U145.0` simulate HV bus volts for the dry-run precharge (`U0` off; RAM only)

## Precharge (role A, print-only)
Dead-man on, no faults -> Precharge. Main closes when bus >= 95% of the LOWEST string
voltage (all 4 strings, local + B) AND >= 300 ms. Not within 3 s -> RetryWait 30 s; max 2
retries, then Lockout (dead-man off clears). Bus voltage source: `U` simulation, else the
HV divider if `HV_ANALOG_ENABLE=1`, else none (precharge times out = fails safe).
