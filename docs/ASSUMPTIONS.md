# Assumptions — 2026-10-15

Everything below is assumed, not yet proven at the bench. Every one of these
must be checked before the related `BMS_CAP_*` flag is flipped. All flags are
still 0 and `TELEMETRY_ONLY=1`.

| # | Assumption | Where | How to verify |
|---|-----------|-------|---------------|
| 1 | Charger is Elcon **HK-LF-144-46** (config CAN3865), nameplate output **100–202 V, 46 A DC** (confirmed from label 2026-10-15). Current request 5 A, guarded <= 8 A for a 120 V outlet. | `include/Config.h` | CAN3865 ID set still to be confirmed by sniffing before power-up. |
| 2 | Elcon protocol: 0x1806E5F4, extended ID, 250 k, V/I ×0.1 big-endian, byte4 0 = charge / 1 = stop. The charger stops itself about 5 s after frames stop. Same layout as Tom's VoltBMSV2 `chargercomms()`. | `include/ElconCharger.h` | Test into a dummy load (checklist below). |
| 3 | **Module 2 is a 12S block** (per Adam). Seeing only 8 live cells points to a dropped frame or a loose plug, not a 8S block. | `include/FactoryCellMap.h` | `d` LOW-LIVE line, reseat the plug, meter the taps. |
| 4 | Factory frame layout (IDs 0x460–0x47E, channels per frame, 12-bit 1.25 mV/LSB) is taken from Tom de Bree's `candecode()`, with his `<<` precedence bug fixed. | `include/FactoryCellMap.h`, `src/BicmDecodeFactory.cpp` | Partly proven: M1 and M2 cells read sensibly. |
| 5 | **`TEMPS_VERIFIED=0`**, so charging is always refused. Two candidate decodes for 0x7E0–0x7EF are printed as UNVERIFIED: A = DBC 10-bit `(b0&3)<<8|b1` × 0.0556 − 27.778 °C; B = Tom `(b6<<8|b7)` × −0.0324 + 150 °C. No limits are enforced. | `src/BicmDecodeFactory.cpp` | Warm-brick test. |
| 6 | Hardware watchdog: WDOG3, 32 kHz LPO, **~2 s** timeout, fed only from `loop()`. Reset cause comes from SRC_SRSR. | `src/Watchdog.cpp` | Stall test. |
| 7 | A cell is **stale after 3 s** (`kCellStaleMs`). Same rule on K112 and factory decode. | `include/Config.h` | Unplug CAN, cells go `!`/0 within about 3 s. |
| 8 | Charge setpoint is **4.10 V/cell**: charging stops when any cell reaches 4.10 V. A cell at **4.12 V or higher latches a charge fault** until reboot. Cell OV fault is 4.25 V, UV is 2.50 V. | `include/Config.h` | Review only. |
| 9 | Charge voltage = 4.10 × cells in **one series string** (factory: `FACTORY_STRING_CELLS`; K112 36S env: 36). The bench map (`FACTORY_SERIES_STRING 0`) makes charging impossible. | `src/ChargeTx.cpp` | Review only. |
| 10 | **Balance bit map is unverified.** The K112-36S map is based on forum posts. No factory map exists, so balance stays off on `teensy41_factory_bus`. | `include/K112BalanceMap.h` | Bench check one bit at a time with a meter. |
| 11 | Precharge is a 5 s timer only (`HV_ANALOG_ENABLE=0`), the same as Tom's. Not acceptable for live use. | `src/ContactorSeq.cpp` | Use the HV divider and voltage-based gate. |
| 12 | Kelly KLS layout is little-endian (RPM, current, battery V, error); it's listen-only. | `src/KellyDecode.cpp` | Compare with the Kelly PC software. |
| 13 | No current sensor and no SOC. `packA` and `socPct` stay null. | — | Hardware still to come. |

## Current sensing (2026-10-19)
- Kelly peak discharge assumed 550 A in the simulator.
- Main sensor: ISA IVT-S 1000 A (IVT-S-1K-U3-I-CAN1-12/24), ±1000 A, CAN (ships at 500 kbps vs CAN2 250k, may need its own bus). Not owned.
- LEM DHAB S/44 (pins 1 +5V, 2 GND, 3 high out, 4 low out; Vero BMS manual): cross-check and low current only. Saturates ~450–490 A (unverified).
