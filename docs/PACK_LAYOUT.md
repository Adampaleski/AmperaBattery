# Teryx pack layout: 36S × 4P (SOURCED: Adam 2026-10-21)

Adam: minimum 36S, **four 36S strings in parallel (4P)**. Correction (Adam 2026-10-21 17:10): **2 BICMs per 36S string = 8 BICMs total**, on **four separate BICM CAN buses, one per string** (Adam's CAN decision 17:11).
This settles the cash-ledger "36S2P" vs docs "4 × 36S" conflict in favour of **4 × 36S**.

## Numbers
| Item | Value | Status |
|---|---|---|
| Series cells per string | 36 | SOURCED: Adam |
| Strings in parallel | 4 | SOURCED: Adam |
| Series groups in the pack | 36 (each string monitored separately: 144 group taps) | SOURCED layout |
| Volt Gen1 cell-group Ah | 45 Ah (3P × 15 Ah Gen1 cells) | ASSUMED: usual Gen1 figure, not measured; 2013–14 packs may be slightly higher |
| Pack capacity | 4 × 45 = **180 Ah** | layout SOURCED × group Ah ASSUMED |
| Pack energy | 133.2 V × 180 Ah ≈ **24.0 kWh** (sim/dash still use 36 × 3.6 V = 23.3 kWh, conservative) | ASSUMED |
| Nominal voltage | 36 × 3.7 = **≈133 V** (133.2 V) | ASSUMED 3.7 V/cell nominal. 147.6 V is FULL, not nominal |
| Charge ceiling | 36 × 4.10 = **147.6 V** (Config kChargeCellSetpointV) | firmware |
| Charge fault latch | 36 × 4.12 = 148.3 V | firmware |
| OVER-MAX fault | 36 × 4.25 = 153.0 V | firmware |
| Low cut at rest | 36 × 3.30 = **118.8 V** | PLACEHOLDER |
| Hard floor under load | 36 × 3.00 = 108.0 V | PLACEHOLDER |
| Kelly | 72–168 V model; 147.6 V is inside | SOURCED: order #81746 |
| Elcon HK-LF-144-46 | 100–202 V, 46 A; 147.6 V inside | SOURCED: label |

The ledger's "~150–155 V full" is 36S at 4.17–4.3 V/cell; the firmware stops at 4.10 V (147.6 V).

## Current per string (equal share, 4P)
| Pack current | Per string | vs Mersen A6T200 (200 A) |
|---|---|---|
| 550 A launch (Kelly battery limit) | **137.5 A** | 69 % |
| 650 A for 100 ms (BMS trip) | 162.5 A | 81 % |
| 750 A instant trip | 187.5 A | 94 % |
| Earlier "uneven" estimate | ~185 A on the strongest string at 550 A | 93 % |

Per-string current is **not measured**: LEM A sees strings 1+2 and LEM B sees strings 3+4. The imbalance derate (|A − B| > 100 A for 1 s, PLACEHOLDER) only catches a pair-to-pair split; a split inside a pair is invisible. Class T fuses are time-delay, so 187.5 A brief peaks are inside their rating, but check the A6T200 time-current curve against the 650 A / 100 ms trip with Sparky. No firmware change made.

## Module inventory vs 4 × 36S
- Owned modules (cash ledger): 13 × 12S + 4 × 6S = **180S** worth of cell groups.
- Needed: 4 × 36S = **144S**.
- **Spare: 36S** worth = one full extra string. Example: strings of 3 × 12S use 12 of the 13 twelve-S modules, leaving **1 × 12S + 4 × 6S** spare. Or build some strings as 2 × 12S + 2 × 6S and keep 3 × 12S spare. Which modules go in which string: not decided.

## Monitoring: 8 BICMs, 2 per string (SOURCED: Adam 2026-10-21)
In 4P each string's 36 groups are separate (string-level parallel, one fuse per string), so the BMS needs **144 group taps**. Parallel strings do not reduce that.

**BICM type per string** (the boards the firmware decodes, include/StringConfig.h + FactoryCellMap.h):
| Board | Volt Gen1 position | Channels used | CAN IDs (voltage / pair) |
|---|---|---|---|
| 24S BICM | M2 (strings 1, 3) or M3 (strings 2, 4) | 24 of 24 | M2 0x464–0x466 / 0x474–0x476 · M3 0x468–0x46A / 0x478–0x47A |
| 18S BICM | M4 | 12 of 18 (c1–12; c13–18 0x46E empty by design) | 0x46C–0x46D / 0x47C–0x47D |
GM part numbers: **not in the repo or BOM.** "K112" in docs/K112_24S.md is the GM schematic reference designator (K112A–D on the daisy chain), not an orderable P/N. Read the label on an owned M4 board before buying (UNVERIFIED).

**Count:** need 8 = 4 × 24S (2 × M2, 2 × M3) + 4 × M4 18S. docs/COMPONENTS.md: owned M2 × 2, M3 × 2, M4 × 2 = 6; "M4 18S × 2 (extra) — needed for 4 strings: NO". **"Buy 2 more" checks out: 2 × M4 18S BICM** (strings 3 and 4). The cash ledger figures given to me do not list BICMs, so ownership is from COMPONENTS.md only. Nothing bought.

**ID clash:** every M4 has the same fixed IDs, and so does every M2 or M3, so two M4s (or two M2s) can never share a bus. One 24S + one M4 per bus has no clash (different positions). That is why there are 4 BICM buses.

**K112 burst caveat:** docs/K112_24S.md saw a 24S board report as a 0x460/0x470 burst (K112_PACK_DECODE envs) instead of the factory IDs above. If all boards behave like that, two 24S-burst boards on one bus would collide. Bench-check each board's IDs on its own bus before wiring. UNVERIFIED.

## CAN bus plan (Adam: 4 BICM buses, one per string)
Teensy 4.1 has 3 FlexCAN controllers; A + B = 6. This is what include/StringConfig.h and docs/DUAL_STRING.md already implement — **no MCP2515/MCP2518FD needed, no firmware logic change.**
| Teensy | Controller | Pins | Rate | Bus | Nodes |
|---|---|---|---|---|---|
| A | CAN1 | 22/23 | 125k 11-bit | BICM bus 1 → string 1 | M2 24S + M4 (owned) |
| A | CAN2 | 0/1 | 125k 11-bit | BICM bus 2 → string 2 | M3 24S + M4 (owned) |
| A | CAN3 | 30/31 | 250k 29-bit | vehicle bus | Kelly KLS-8080N (0x0CF11E05/0x0CF11F05), Elcon HK-LF-144-46, A↔B link 0x6A0–0x6A8 |
| B | CAN1 | 22/23 | 125k 11-bit | BICM bus 3 → string 3 | M2 24S + **M4 (to buy)** |
| B | CAN2 | 0/1 | 125k 11-bit | BICM bus 4 → string 4 | M3 24S + **M4 (to buy)** |
| B | CAN3 | 30/31 | 250k 29-bit | same vehicle bus: A↔B link (stub, unterminated) | "spare for charger/hub" is NOT free: the link lives here. Freeing it means moving the link to UART (logic change, not done) |

Source: Adam 2026-10-21. Bench envs (sniffer, k112_*, factory_bus, 1bicm, bench_*) keep CAN3 30/31 @125k for one BICM bus; the remap is `CAN_LAYOUT_V2` (default on when `DUAL_STRING=1`, include/FlexCanCompat.h). **CREW REVIEW: firmware logic change, not bench-tested.**

Pins: CAN2 0/1 = Serial1 (unused; hub is Serial4 16/17), OUT5/OUT6 already moved to 2/3, no Config.h pin on 0/1/22/23/30/31 except the CAN pins. No conflict on A or B.

Link: B → A every 100 ms with string V, cell min/max, faults, heartbeat (0x6A0/0x6A1) and cell counts (0x6A2). Cell avg = V / cells (derived on A, no new frame). Temps are NOT sent: BICM temps aren't decoded yet (TEMPS_VERIFIED=0); add a frame when they are. A: B heartbeat older than **200 ms (PROPOSED, `T linkMs`, 100–2000)** → F_LINK fault, mains open, logged. Was 1000 ms.
Spare controllers: none. A dedicated Kelly or vehicle bus later would need an external MCP2518FD (SPI) — not needed now. The Kelly CAN is broadcast-only and at 250k with the Elcon; that's the current Config; Kelly CAN rate itself is UNVERIFIED (docs/KELLY_CAN.md).

**Transceivers:** one 3.3 V SN65HVD230 per controller = **6** (4 BICM + 2 on the vehicle bus). docs/COMPONENTS.md lists × 4 — **2 more needed** (or confirm the charger-bus ones are counted elsewhere).
**Termination:** 120 Ω at both ends of every bus. BICM buses: 120 Ω at the Teensy end; the far end (last BICM) — whether the Volt BICM chain has internal termination is UNVERIFIED, meter it (≈60 Ω across CAN_H/CAN_L with power off = both ends present). Vehicle bus: 120 Ω at Teensy A and at the farthest node (Elcon or Kelly); check for built-in terminators in either (UNVERIFIED) so the bus is not triple-terminated; Teensy B is a stub with no terminator.

No safety flag or output was changed.
