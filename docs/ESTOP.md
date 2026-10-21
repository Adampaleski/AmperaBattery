# E-stop (crew sign-off 2026-10-21: chief, Sparky) — PROPOSED, nothing bought

The e-stop is a **hardware cut**. It works with both Teensys dead and doesn't depend on park or software.

## Wiring
```
12 V fused (rating TBD) --> E-stop NC block 1 --+--> EV200 B+ coil high side   (FET low side <- Teensy A pin 11 OUT1)
                                                +--> EV200 B- coil high side   (FET low side <- Teensy A pin 21 OUT4)
                                                +--> AEV14012 precharge coil   (FET low side <- Teensy A pin 12 OUT2)
                                                +--> Kelly KSI / PWR (pin 7, V2.11 manual; harness UNVERIFIED)
E-stop NC block 2 --> Teensy B input (pull-up, open = pressed) -- LOGGING ONLY, never part of the cut (pin TBD)
```
- **NC block 1** is in series with the coil feed to **both EV200s, the precharge relay coil and the Kelly KSI**. Pressing it drops every contactor and powers down the Kelly logic, whatever the FETs are doing.
- **NC block 2** only tells Teensy B the e-stop was pressed (event log and dash). The cut never depends on it.

## Parts (PROPOSED)
- Mushroom head, latching, **twist-to-release**: Schneider Harmony **XB4** metal series or an IDEC mushroom, with 2 NC contact blocks.
- In an **IP65** enclosure with a guard collar (no accidental hits, still reachable from the seat).

## Reset
1. Twist to release the mushroom.
2. **Key cycle** (key off, then on). The BMS doesn't re-close on release alone.
3. A **new precharge** sequence (park brake set, PROPOSED; docs/PARK_BRAKE.md).

## Load on NC block 1 — UNVERIFIED rating
| Load | Current | Label |
|---|---|---|
| EV200HAANA hold (economized), each | ≈0.26 A (×2 = 0.52 A) | SOURCED crew (TE datasheet value, recheck) |
| EV200 inrush, each | ≈3.8 A for ≈130 ms | SOURCED crew (recheck datasheet) |
| AEV14012 coil | ≈0.35 A (not economized) | SOURCED (Volt part, sim note) |
| Kelly KSI / PWR | UNVERIFIED (measure) | — |
**Both EV200s close together**, so the e-stop contact must **make and carry the combined inrush of ≈7.6 A for ≈130 ms**, plus the precharge coil and KSI. It only ever **breaks the hold current** (≈0.52 A + precharge coil + KSI), because pressing it happens with the coils economized. The XB4/IDEC block's DC-13 make/break rating at 12–24 V is a **bench/datasheet check (UNVERIFIED)**.

**No interposing relay:** the crew (Sparky) prefers the contact block directly in the coil feed. A relay adds a failure point and a coil of its own. Only revisit this if the datasheet check fails.

## Bench checks before first drive
- Press with mains closed: both EV200s and the precharge relay open, the Kelly powers down, and Teensy B logs the press.
- Twist to release: nothing re-closes until a key cycle and a new precharge.
- Measure the coil currents on NC1 and compare them with the block's datasheet rating.
