# BMS protection settings vs open-source references (Build 10)

Adam 2026-10-22: he defers BMS numbers to the crew. Base them on SimpBMS / open-source
BMS references plus the cell maker's limits; the more conservative value wins. Every
value below stays adjustable over USB serial (`docs/THRESHOLDS.md`).

Labels: **SOURCED** (file:line or doc) · **ASSUMED** · **PROPOSED** · **UNVERIFIED**.

## Reference values

| Setting | SimpBMS / VoltBMSV2 (Tom de Bree) | ENNOID-BMS | LG cell (Volt/Ampera module) | Teryx firmware (now) |
|---|---|---|---|---|
| Cell max (over-V fault) | 4.20 V `OverVSetpoint` SOURCED VoltBMSV2.ino:704 | soft/hard over-V both exist; numeric defaults UNVERIFIED (not found in ENNOID-BMS-Tool source) | 4.15 V UNVERIFIED (community figure, gm-volt / NASA teardown cited by crew; no datasheet on file) | charge stop 4.10 V `kCeil100V` SOURCED include/BmsCore.h:78; fault at ceiling + 0.05 V (BmsCore.h:36) |
| Charge target | 4.10 V `ChargeVsetpoint` SOURCED VoltBMSV2.ino:706 | UNVERIFIED | — | 4.10 V (same) |
| Cell min (under-V fault) | 3.00 V `UnderVSetpoint` SOURCED VoltBMSV2.ino:705 | soft/hard under-V; defaults UNVERIFIED | 2.8 V UNVERIFIED (as above) | 3.00 V under load `hardFloorV` (BmsCore.h:141); 3.30 V at rest `lowCutV` (BmsCore.h:88) |
| Discharge stop | 3.20 V `DischVsetpoint` SOURCED VoltBMSV2.ino:709 | — | — | 3.30 V at rest (more conservative) |
| Discharge taper | 0.30 V above DischV (taper 3.50→3.20 V) `DisTaper` SOURCED VoltBMSV2.ino:726 | — | — | **3.50 → 3.30 V** (`derateLowV` 0.20, was 0.15) PROPOSED |
| Derate temp | 40 °C `DisTSetpoint` SOURCED VoltBMSV2.ino:714 | UNVERIFIED | — | **40 °C** (was 50) PROPOSED |
| Over-temp trip | 65 °C `OverTSetpoint` SOURCED VoltBMSV2.ino:711 | UNVERIFIED | — | 55 °C (more conservative) BmsCore.h:100 |
| Charge temp window | 0 °C min `ChargeTSetpoint` SOURCED VoltBMSV2.ino:713 | — | — | 0–45 °C BmsCore.h:101-102 |
| Cell gap | 0.20 V `CellGap` SOURCED VoltBMSV2.ino:710 | — | — | relative sag logic (Adam 10/21) |

## Sparky's proposals (crew) vs references

| # | Sparky | Verdict | What the code does |
|---|---|---|---|
| 1 | Derate 3.45 → 3.25 V | **CONFLICT**: 3.25 V is below our 3.30 V at-rest cutoff, so the contactors could open before the derate reaches zero | at rest 3.50 → 3.30 V (SimpBMS-shaped taper, PROPOSED); under load the relative sag-vs-pack-average logic stays (Adam 10/21) |
| 2 | Hard cut 3.0 V under load | no conflict: = SimpBMS UnderV 3.0, above LG 2.8 V (UNVERIFIED) | `hardFloorV` 3.00 |
| 3 | 400 A trip per LEM sensor | added, `lemTripA` 400 (serial key `lemOcA`, tighten-only, max 450), uses `ocMs` 100 ms. **UNVERIFIED catch:** notes say each LEM tops out near 230 A in one direction; if that is the discharge side a single sensor can never read 400 A. Sparky to confirm | include/BmsCore.h `lemTripA` |
| 4 | 50 % derate if sensors differ > 60 A | no conflict; no reference has a 2-sensor check, stays PROPOSED | imbalance keys `imbA/imbMs/imbPct` |

Nothing here is bench-tested. Outputs remain off (safety flags unchanged).
