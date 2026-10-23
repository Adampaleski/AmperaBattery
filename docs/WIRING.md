# Wiring (from AmperaBattery upstream)

## CAN map: CAN_LAYOUT_V2 (dual-string build, default when DUAL_STRING=1; include/FlexCanCompat.h)
| Teensy bus | Pins | Speed | Teensy A | Teensy B |
|---|---|---|---|---|
| CAN1 | 22 TX / 23 RX | 125k | BICM string 1 | BICM string 3 |
| CAN2 | 1 TX / 0 RX | 125k | BICM string 2 | BICM string 4 |
| CAN3 | 31 TX / 30 RX | 250k | vehicle bus: Kelly + Elcon + A-B link | same vehicle bus (stub, no 120 Ω) |
Each bus has its own SN65HVD230 (6 total; 4 owned). **The sections below are the older single-string / bench layout (CAN_LAYOUT_V2=0)**: there, BICM = CAN3, Elcon/Kelly = CAN2. Use the table above for the Teryx pack (`teensy41_dual_a` / `_b`). CREW REVIEW: bus remap not bench-tested.


## BICM internal CAN (125 kbps) — bench target (legacy layout, CAN_LAYOUT_V2=0)

Connector **X2** on BECM K16:

| Pin | Signal |
|-----|--------|
| 9 | 5V (do not tie to Teensy) |
| 10 | Ground |
| 11 | CAN_Low |
| 12 | CAN_High |

Teensy 4.1: **CAN3** — pin 30 RX, pin 31 TX, transceiver GND common with pack.
Use the existing SN65HVD230 (or equivalent 3.3 V transceiver).

## Elcon / TC charger CAN (250 kbps) — legacy CAN2 (V2: CAN3 30/31 vehicle bus)

**Not** Brusa 500 kbit. **Not** Eltek. **Not** the 125 kbit BICM bus.

Teensy 4.1 **CAN2** — pin 0 RX (`CRX2`), pin 1 TX (`CTX2`) @ **250000**. Extended 29-bit IDs.

A **second SN65HVD230** (3.3 V) is required. CAN1 pins 22/23 collide with `Pins::OUT5`/`OUT6` on the VoltBMS carrier, so the charger must not use CAN1.

| Teensy | Transceiver | Charger |
|--------|-------------|---------|
| pin 0 (`CRX2`) | CRX / RXD | — |
| pin 1 (`CTX2`) | CTX / TXD | — |
| GND | GND | CAN GND |
| 3.3 V | VCC | — |
| — | CANH / CANL | Elcon CANH / CANL |
| — | 120 Ω | terminate one end of this bus |

Sticker **E5** is the CAN node ID (IDs `0x1806E5F4` / `0x18FF50E5`), **not** voltage.

Winding (HK-MF-108 vs 144 vs 72) is still unread — protocol is the same; firmware does not hardcode a range. A **72/96 V** box cannot finish **36S** (~149 V at 4.15 V/cell). A **312 V** box will not start a drained 36S.

Pins 0/1 are also `Serial1`. This firmware does **not** use the VoltBMS ESP32 UART companion on those pins.

Weekend bench: leave CAN2 unwired. The controller still starts; TX is gated off.

## Contactors (12 V coils via FETs)

VoltBMSV2 names on `Pins::OUT1..OUT8`. Teensy pin = FET gate, **not** the coil.

| Alias | Pin | Function |
|-------|-----|----------|
| `OUT1` / `MAIN_POS` | 11 | B+ EV200 coil FET |
| `OUT2` / `PRECHARGE` | 12 | Volt AEV14012 precharge coil FET (not a third EV200) |
| `OUT4` / `MAIN_NEG` | 21 | B− EV200 coil FET |
| `OUT3` | 20 | unused here (VoltBMS charger-enable relay; we use CAN) |

Mechanic's HV kit (assumed present in the sequence, not encoded as P/Ns): 2× TE EV200HAANA (B+ and B−, owned), 1× Volt AEV14012 precharge, 50 Ω 100 W precharge resistor, 4× Mersen A6T200 200 A Class T (owned), 1/0 string leads.

### HV bus panel + service disconnect (crew 2026-10-22)
- **No Volt MSD in the current path** (removed). Path: string n → A6T200 n → common B+ bus → EV200 B+ → Kelly.
- **No fuse holders:** the 4 A6T200s bolt straight to the bus bar on **insulated standoffs**, all 4 reachable from **one side**.
- **Cover:** tool-to-remove, **captive screws** — the string side of each fuse stays LIVE with the fuses pulled.
- Any fuse holder used elsewhere at pack voltage must be rated **> 160 VDC**.
- **Service disconnect = pull all 4 string fuses:** key off, e-stop pressed, wait (Kelly caps), remove cover, pull fuses 1–4 with the **insulated wrench**, meter bus B+ to B− = 0 V, boots on the live string-side studs. Reverse to restore, then a normal precharge.
- Coil-driver FETs: **IRLB3813** (IRLB8721 is discontinued).

Do **not** wire coils until `BMS_CAP_CONTACTOR_DRV` is intentionally set to 1. Default flag is 0; `e` only dry-runs intended pin states.

Optional HV-bus analog: pin 24 (`A10`), compile with `HV_ANALOG_ENABLE=1`. Default is timer-only (5 s).

## BECM vehicle CAN (500 kbps) — not used by this firmware

Connector **X1** — 500 kbps when accessory wake lines are held high. See upstream README. The Elcon bus is a **separate** 250 kbit network on CAN2, not X1.

## Kelly KLS8080 NPS (250 kbps) — same bus as Elcon (legacy: CAN2; V2: CAN3 30/31)

Broadcast RX only. **Not** serial. **Not** BICM CAN3.

| Teensy | Shared with | Kelly |
|--------|-------------|-------|
| CAN2 pin 0/1 @ 250 kbit, 29-bit | Elcon SN65HVD230 #2 | CANH / CANL on same bus |
| IDs | — | `0x0CF11E05`, `0x0CF11F05` |

See [`KELLY_CAN.md`](KELLY_CAN.md). `BMS_CAP_KELLY_RX` default **0** (count + raw via serial `y`).

## Hub UART (ESP32) — Serial4, not CAN2

| Teensy | ESP32 | Notes |
|--------|-------|-------|
| **Serial4 TX pin 17** / **RX pin 16** | UART2 GPIO16 RX / GPIO17 TX (crossed) | Frees 0/1 for CAN2 |
| GND | GND | |
| Baud / format | 115200 8N1 | One **STATUS_SCHEMA v1** JSON object per line (~1 Hz) |

Firmware: `HubStatus` emits on Serial4. Caps `BALANCE_TX` / `CHARGE_TX` / `CONTACTOR_DRV` / `KELLY_RX` stay **0**.

`Pins::IN1`/`IN2` in Config.h also use 17/16 — unavailable when hub UART is wired (unused by contactors; BmsApp does not `pinMode` them). Details: teryx-ev-hub repo: `firmware/esp32-gateway/docs/PINMAP.md`.

## Schematic notes: park brake, Kelly interlocks, throttle clamp (2026-10-21, PROPOSED, CREW REVIEW)
```
Teensy A pin 32 --10k--> 3V3 ; pin 32 -- park pole 1 (NO) -- GND      LOW = brake set
Teensy B pin 32 --10k--> 3V3 ; pin 32 -- park pole 2 (NO) -- GND
Kelly 12V (pin 11) -- park pole 3 (NC, opens when set) -- Kelly Foot_SW (pin 15)   [Kelly Foot Switch = checked]
Kelly 12V (pin 11) -- relay NC -- Kelly ECO (pin 22)      relay coil <- FET <- Teensy A OUT3 pin 20 (HIGH = release Low cap)
Teensy A pin 4 (A_THR_ALIVE, 10k pull-down) -> 74AHCT125 -> 74HCT123 #1 B   (heartbeat; CS no longer used as alive)
4066 VCC = shared 5 V rail; Kelly throttle pull-down 100k
E-stop (docs/ESTOP.md): XB4/XB5 40 mm twist-release + 3x ZBE102 NC, wired DIRECT (no relay)
  12 V fused -> block 1 -> EV200 B+ coil, EV200 B- coil (high sides)
  12 V fused -> block 2 -> AEV14012 precharge coil (high side) + Kelly KSI/PWR pin 7
                block 3 -> Teensy B input (log; open = drop all coil drivers + LATCH until key-cycle reset, include/EstopLatch.h)
Brake_AN (pin 2): NOT WIRED for first drive (DEFERRED: Kelly config readback + Boost OFF confirmed first).
  First drive: Brake Type = 0 (switch regen, BRK_SW Brk fixed level)
Brake switch 12 V -> Kelly pin 25 + brake light; state readable on Kelly CAN 0x0CF11F05 b5 bit3
```
Kelly pin numbers are from the KLS-8080N V2.11 manual (SOURCED). Adam's harness is UNVERIFIED. Details: docs/PARK_BRAKE.md, docs/DRIVE_MODES.md, docs/THROTTLE_CLAMP.md, docs/KELLY_CAN_ROLES.md.


## Drawing standard (Adam, 2026-10-21, all diagrams incl. tools/schematic/gen.py)
- Wires that cross without connecting: draw a hop arc (or a gap break). Never a bare crossing.
- A real junction (same terminal / node): filled dot. No dot = no connection.
- Unrelated wires never run on top of each other.
- tools/wiring.html enforces this in draw(); checked by scripts/test_wiring_crossings.py. tools/schematic/gen.py (copied into the repo in Build 18; outputs docs/schematic/teryx-system.{drawio,png,pdf}) draws hop arcs at crossings and dots at real junctions.
