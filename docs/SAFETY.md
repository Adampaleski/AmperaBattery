# Safety — what this firmware does and does not do

## Safe to use on the bench today

| Feature | Status |
|---------|--------|
| Listen on 125k BICM CAN | Yes |
| Decode and print cell voltages | Yes |
| Send keep-alive `0x200` | Yes (required for BICMs to stay awake) |

Default builds use **`TELEMETRY_ONLY`**: no contactor coils, no charger commands.
Capability flags stay **0** until a later, explicit reflash:

- `BMS_CAP_BALANCE_TX=0` — `0x300` / `0x310` packing is compiled, frames are **not** sent.
- `BMS_CAP_CHARGE_TX=0` — Elcon `0x1806E5F4` is packed and printed, **never** written to CAN2.
- `BMS_CAP_CONTACTOR_DRV=0` — precharge/contactor state machine dry-runs over serial; coil FETs stay LOW.

Weekend bench still does **not** need the charger or coils.

## Does NOT run a production BMS (yet)

| Feature | Status | Risk if assumed |
|---------|--------|-----------------|
| Cell balancing (`0x300` / `0x310`) | **Implemented, gated off** (`BMS_CAP_BALANCE_TX=0`) | Pack will not balance until the flag is 1; even then this is not a production balancer |
| Charger control (Elcon / TC) | **Implemented, gated off** (`BMS_CAP_CHARGE_TX=0`) | No charge current on the wire until the flag is 1 |
| Contactor / precharge | **State machine compiled, coils gated off** (`BMS_CAP_CONTACTOR_DRV=0`) | Serial `e` prints intended pin states; GPIO stays LOW |
| Multi-pack parallel (4× 36S) | **Not implemented** | One CAN bus / one decode context only |
| HV isolation / IMD | **Not implemented** | Software will not detect an isolation fault |

Treat this repo as a **monitor and bring-up tool**. Enabling any of the three TX/drive flags still does **not** make this a production BMS.

## Keep-alive

The Teensy sends master keep-alive (`0x200#02.00.00`) so slaves wake up. That is normal for bench work and matches Volt BMS master behavior. It does **not** by itself enable charge, balancing, or coils. When `BMS_CAP_BALANCE_TX=1`, `0x300`/`0x310` are queued immediately before each keep-alive (OEM order: queue bleed, then trigger).

## Contactors

`e` is a dead-man: it must be on before the sequence will *intend* to close coils. Any dead-man off, cell over/under (if a stable pack is present), or `TELEMETRY_ONLY` forces intended/driven outputs open. Physical FET gates are driven only if `BMS_CAP_CONTACTOR_DRV=1` **and** `TELEMETRY_ONLY=0`.

OUT2 is the shop **TE EV11 / K1KABNA** precharge coil, not a third EV200. Pin map is unchanged (OUT1 main+, OUT2 precharge, OUT4 main−).

## Charger

Elcon / TC lives on **CAN2 @ 250 kbit** (pins 0/1, 29-bit), not the 125 kbit BICM bus and not Brusa 500 kbit. Sticker E5 is the CAN node ID. Voltage winding (HK-MF-108 vs 144 vs 72) is unread — protocol is the same; firmware does not hardcode a winding. A 72/96 V box cannot finish 36S (~149 V). A 312 V box will not start a drained 36S. TX is gated by `BMS_CAP_CHARGE_TX`.
