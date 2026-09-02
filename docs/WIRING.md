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

## Brusa NLG5x1 charger CAN (500 kbps) — second transceiver

**Not** Eltek. **Not** the 125 kbit BICM bus.

Teensy 4.1 **CAN2** — pin 0 RX (`CRX2`), pin 1 TX (`CTX2`).

A **second SN65HVD230** (3.3 V) is required. CAN1 pins 22/23 collide with `Pins::OUT5`/`OUT6` on the VoltBMS carrier, so the charger must not use CAN1.

| Teensy | Transceiver | Charger |
|--------|-------------|---------|
| pin 0 (`CRX2`) | CRX / RXD | — |
| pin 1 (`CTX2`) | CTX / TXD | — |
| GND | GND | CAN GND (isolated on the NLG5) |
| 3.3 V | VCC | — |
| — | CANH / CANL | NLG5 CANH / CANL |
| — | 120 Ω | terminate one end of this bus |

Pins 0/1 are also `Serial1`. This firmware does **not** use the VoltBMS ESP32 UART companion on those pins.

Weekend bench: leave CAN2 unwired. The controller still starts; TX is gated off.

## Contactors (12 V coils via FETs)

VoltBMSV2 names on `Pins::OUT1..OUT8`. Teensy pin = FET gate, **not** the coil.

| Alias | Pin | Function |
|-------|-----|----------|
| `OUT1` / `MAIN_POS` | 11 | B+ EV200 coil FET |
| `OUT2` / `PRECHARGE` | 12 | HV precharge relay FET |
| `OUT4` / `MAIN_NEG` | 21 | B− EV200 coil FET |
| `OUT3` | 20 | unused here (VoltBMS charger-enable relay; we use CAN) |

Mechanic's HV kit (assumed present in the sequence, not encoded as P/Ns): 2× EV200 (B+ and B−), 1× HV precharge relay, 50 Ω 100 W precharge resistor, Class T 200 A fuse.

Do **not** wire coils until `BMS_CAP_CONTACTOR_DRV` is intentionally set to 1. Default flag is 0; `e` only dry-runs intended pin states.

Optional HV-bus analog: pin 24 (`A10`), compile with `HV_ANALOG_ENABLE=1`. Default is timer-only (5 s).

## BECM vehicle CAN (500 kbps) — not used by this firmware

Connector **X1** — 500 kbps when accessory wake lines are held high. See upstream README. The Brusa bus is a **separate** 500 kbit network on CAN2, not X1.
