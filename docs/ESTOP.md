# E-stop (crew final spec 2026-10-22: chief, Sparky) — nothing bought

The e-stop is a **hardware cut** wired **directly, no relay**. It works with both Teensys dead and doesn't depend on park or software.

## Parts (crew spec)
- Schneider Harmony **XB4/XB5 40 mm twist-release mushroom head** (latching), in an IP65 guarded box, reachable from the seat.
- **3 NC blocks total.** BOM: the **XB4BS8442 head includes 1 NC block**, so buy **2 extra ZBE102** (not 3).
- **Schneider ZBE102** NC contact blocks. Datasheet (SOURCED, fetched 2026-10-22, 200, part number found): https://static.datasheets.com/doc/13749203-schneider-zbe102-ds.pdf
  - Ie DC-13: **0.55 A at 125 V** (Q600); 0.27 A at 250 V. Ith thermal **10 A**. Durability: 1 M cycles DC-13 0.5 A at 24 V.
  - **Block 1: SOURCED / OK** (chief 2026-10-22). Breaks ≈0.26 A (Adam bench-measured ≈0.13 A hold per EV200), under the 0.5 A @24 V DC-13 rating. No make rating needed: the contact is already closed before the coils energize; it only carries the ≈7.6 A inrush, within the 10 A Ith.
  - **Block 2: PENDING** until Adam meters the Kelly KSI draw.

## Wiring
```
12 V fused (rating TBD) --> ZBE102 block 1 (NC) --+--> EV200 B+ coil high side   (FET low side <- Teensy A pin 11 OUT1)
                                                  +--> EV200 B- coil high side   (FET low side <- Teensy A pin 21 OUT4)
12 V fused (rating TBD) --> ZBE102 block 2 (NC) --+--> AEV14012 precharge coil   (FET low side <- Teensy A pin 12 OUT2)
                                                  +--> Kelly KSI / PWR (pin 7, V2.11 manual; harness UNVERIFIED)
ZBE102 block 3 (NC) --> Teensy B input (pull-up, open = pressed or wire broken) -- log + software latch (pin TBD)
```
- **Block 1** cuts both EV200 coils. **Block 2** cuts the precharge coil and the Kelly KSI. Pressing drops every contactor and powers down the Kelly logic, whatever the FETs are doing.
- **Block 3** reports to Teensy B. It is never part of the cut.

## Firmware rule (CRITICAL; `include/EstopLatch.h`, test `scripts/test_estop_latch.cpp` 7/7)
When block 3 reports **open**, the Teensy drops **every coil driver and latches**. Releasing the button never closes anything. The latch clears only on an explicit reset (key cycle) while block 3 reads closed, then a new precharge.
Status: logic + host test only. **Not wired into any output**: BMS_CAP flags stay 0, TELEMETRY_ONLY=1 (print-only).

## Reset
1. Twist to release the mushroom.
2. **Key cycle** (key off, then on): clears the software latch (refused if block 3 still reads open).
3. A **new precharge** sequence (park brake set, PROPOSED; docs/PARK_BRAKE.md).

## Loads per block
| Block | Load | Current | Label |
|---|---|---|---|
| 1 | EV200HAANA hold (economized) ×2 | ≈0.13 A each = **≈0.26 A break** | SOURCED (Adam bench-measured 2026-10-22; TE datasheet 0.13 A @12 V) |
| 1 | EV200 inrush ×2 (close together) | ≈3.8 A each = **≈7.6 A carried for ≈130 ms** (contact already closed) | SOURCED (TE datasheet 3.8 A max / 130 ms) |
| 2 | AEV14012 coil | ≈0.35 A (not economized) | SOURCED (Volt part, sim note) |
| 2 | Kelly KSI / PWR | expected **< 0.2 A** — **Adam meters it** | UNVERIFIED |
**Block 1: OK** — breaks ≈0.26 A < 0.5 A @24 V DC-13; carries (never makes) the ≈7.6 A inrush < 10 A Ith. Block 2 breaks ≈0.35 A + KSI.

## Bench checks before first drive
- Press with mains closed: both EV200s and the precharge relay open, the Kelly powers down, Teensy B logs the press and latches.
- Twist to release: nothing re-closes until a key cycle and a new precharge.
- Unplug block 3's wire: same as a press (latched).
- Meter the Kelly KSI draw (expected < 0.2 A) to close out block 2.
