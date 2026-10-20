# Teryx main power path — master checklist

Tick boxes on GitHub or in VS Code. Prices are verified new prices (Oct 2026); TBD means not yet known.

## Decision
- [x] **2 open-box TE EV200HAANA mains (12–24 V economized coil)**: one on pack +, one on pack −, each sees the full 550 A
- [x] **One 200 A Class T fuse per string (4 total); 1/0 or heavier string leads**
- [x] **No per-pair contactors** (no limp-on-half-pack on a SxS)

## Already owned
- [x] Volt AEV14012 contactors — **only** on charger / DC-DC branches or precharge
- [x] 2× Volt LEM DHAB S/44 current sensors (LEM A, LEM B)
- [x] Kelly KLS8080NPS controller
- [x] 2× Teensy 4.1 (A, B)
- [x] Volt BICMs (M2/M3 24S, M4 18S)
- [x] ESP32 hub
- [x] Surface 3 head unit
- [x] SN65HVD230 CAN transceivers
- [x] DROK buck converters
- [x] Precharge relay + 50 Ω resistor (listed owned in `docs/COMPONENTS.md`; resistor value still to be confirmed against Kelly capacitance)

## Already on BOM (not yet owned)
From `docs/COMPONENTS.md`, `../diagram/components.json`, `../perfboard/BOM.md`:
- [x] EV200HAANA main + (bought 2026-10-20, eBay ecrunch, $31.65)
- [x] EV200HAANA main − (bought 2026-10-20, eBay ecrunch, $31.65)
- [x] Mersen A6T200 200 A Class T ×4 — **BOUGHT 2026-10-20** ($36.20 ea, unitedfuses.com); A6T is 300 V class, check the body on arrival
- [ ] Volt BICM M4 18S ×2 (extra, needed for 4 strings)
- [ ] Perfboard parts: 90×150 perfboard, female headers, 120 Ω terminators + jumper, 100 µF caps, SMBJ18A TVS, 10 k/18 k 1% resistors, 100 nF caps, JST-XH headers/housings/pins, DT04/DT06 bulkheads, IRLB8721 FETs ×6 per board, 100 Ω/10 k gate resistors, 1N4007, inline blade fuse holders + 1 A fuses, panel-mount micro-USB extension, M3 standoffs, zip-ties

## Remaining buys
| Item | Part number | Price | Status |
|---|---|---|---|
| Main contactor ×2 | TE EV200HAANA 1-1618002-8 (bought) | $63.30 | - [x] |
| Optional upgrade ×2 | Gigavac GX14BA (525 A / 10 s rating) | $205 each | - [ ] optional |
| Precharge resistor | TBD — needs Kelly input capacitance | TBD | - [ ] |
| Kelly-side HV voltage sense | Kelly CAN bus voltage (to confirm) **or** isolated HV module (TBD) | TBD / $0 if CAN works | - [ ] |
| String fuses | Mersen A6T200 ×4 (bought) | $144.80 | - [x] |

- [x] Buy EV200 ×2 (open-box EV200HAANA, $63.30)
- [ ] Decide on GX14BA upgrade
- [ ] Get Kelly input capacitance → size precharge resistor
- [ ] Confirm Kelly bus voltage on CAN, else pick HV sense module
- [x] Size string fuses with Sparky (200 A; about 140 A/string at launch, about 185 A uneven)

**Projected total (known numbers only):** Bought: 2× EV200HAANA $63.30 + 4× A6T200 $144.80 = **$208.10**. Plus TBD: precharge resistor, HV sense, 1/0 string leads.

## Firmware open
- [ ] Close sequence: main − → precharge → wait Kelly side ≥ 95 % pack V → main + → precharge off; timeout → fault
- [ ] Startup weld check: both mains commanded open, Kelly side must read ≈ 0 V or refuse to start
- [ ] Hard trips on LEM A + LEM B: 650 A for 100 ms, 750 A instant (replaces 300 A / 500 ms) — code + sim done 2026-10-20 (`BmsCore`); tick after a bench test with `CURRENT_SENSE_ENABLE` on
- [ ] LEM imbalance derate: |A − B| > 100 A for 1 s → derate + warn
- [ ] Cell sag derate: lowest cell 3.2 V → 3.0 V linear to zero
- [ ] Heat derate (50 °C start / 60 °C cut) coded but **off** until `TEMPS_VERIFIED`
- [ ] LEM sign flip (discharge positive), zero calibration, enable `CURRENT_SENSE_ENABLE`
- [ ] Kelly settings: phase 550 A / battery 550 A (100 % / 100 %)
- [ ] Kelly input polarity check: LIMP / DERATE / REGEN active on switch-to-GND

## Bench tests before first drive
- [ ] LEM zero and sign verified with a clamp meter
- [ ] Bleed meter test on M1 cell 1 (`docs/BENCH_BLEED_TEST.md`)
- [ ] M4 harness fix (only 6 cells reported) — plug seated, all wires present
- [ ] Contactor sequence dry run with HV disconnected
- [ ] Open-box EV200 inspection, each unit: closed ≈ 0 Ω, open infinite, coil holds ≈ 0.13 A at 12 V
- [ ] Transceiver / buck pinout meter check before soldering

## Sparky's hard no's
- [ ] AEV14012 **not** as mains
- [ ] No sustained > 120 A on any AEV14012
- [ ] No bleeding all cells at once — one cell at a time on bench; never adjacent cells, never a whole circuit (`docs/FACTORY_BUS.md`, `docs/BENCH_BLEED_TEST.md`)
- [ ] Balance command resend every 200 ms
- [ ] Do not set `BMS_CAP_BALANCE_TX` / `CHARGE_TX` / `CONTACTOR_DRV` / `KELLY_RX` to 1 until each is bench-proven (`docs/BENCH.md`, `docs/SAFETY.md`)
- [ ] Do not set `BMS_CAP_KELLY_RX=1` until raw Kelly CAN bytes match the map (`docs/KELLY_CAN.md`)
- [ ] Never feed pack/BICM 5 V (X2 pin 9) into Teensy I/O (`docs/BENCH.md`, `docs/WIRING.md`)
- [ ] Elcon test: request ≤ 1 A a few volts under pack V; confirm stop frame and 5 s timeout (`docs/FACTORY_BUS.md`)
