# Wiring (from AmperaBattery upstream)

## BICM internal CAN (125 kbps) — bench target

Connector **X2** on BECM K16:

| Pin | Signal |
|-----|--------|
| 9 | 5V (do not tie to Teensy) |
| 10 | Ground |
| 11 | CAN_Low |
| 12 | CAN_High |

Teensy 4.1: **CAN3** — pin 30 RX, pin 31 TX, transceiver GND common with pack.
Use the existing SN65HVD230 (or equivalent 3.3 V transceiver).

## Elcon / TC charger CAN (250 kbps) — second transceiver

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

Do **not** wire coils until `BMS_CAP_CONTACTOR_DRV` is intentionally set to 1. Default flag is 0; `e` only dry-runs intended pin states.

Optional HV-bus analog: pin 24 (`A10`), compile with `HV_ANALOG_ENABLE=1`. Default is timer-only (5 s).

## BECM vehicle CAN (500 kbps) — not used by this firmware

Connector **X1** — 500 kbps when accessory wake lines are held high. See upstream README. The Elcon bus is a **separate** 250 kbit network on CAN2, not X1.

## Kelly KLS8080 NPS (250 kbps) — same CAN2 as Elcon

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
Teensy A pin 18 (IN3) --10k--> 3V3 ; pin 18 -- park pole 1 (NO) -- GND      LOW = brake set
Teensy B pin 18 (IN3) --10k--> 3V3 ; pin 18 -- park pole 2 (NO) -- GND
Kelly 12V (pin 11) -- park pole 3 (NC, opens when set) -- Kelly Foot_SW (pin 15)   [Kelly Foot Switch = checked]
Kelly 12V (pin 11) -- relay NC -- Kelly ECO (pin 22)      relay coil <- FET <- Teensy A OUT3 pin 20 (HIGH = release Low cap)
Teensy A pin 4 (A_THR_ALIVE, 10k pull-down) -> 74AHCT125 -> 74HCT123 #1 B   (heartbeat; CS no longer used as alive)
4066 VCC = shared 5 V rail; Kelly throttle pull-down 100k
Brake pedal (hall 0-5 V) -> Kelly Brake_AN (pin 2)  [Brake Type = 2, Boost = UNCHECKED]
Brake switch 12 V -> Kelly pin 25 + brake light; state readable on Kelly CAN 0x0CF11F05 b5 bit3
```
Kelly pin numbers are from the KLS-8080N V2.11 manual (SOURCED). Adam's harness is UNVERIFIED. Details: docs/PARK_BRAKE.md, docs/DRIVE_MODES.md, docs/THROTTLE_CLAMP.md, docs/KELLY_CAN_ROLES.md.

